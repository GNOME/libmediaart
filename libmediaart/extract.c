/*
 * Copyright (C) 2008, Nokia <ivan.frade@nokia.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 */

#include "config.h"

#include <utime.h>
#include <string.h>
#include <errno.h>

#include <glib/gstdio.h>
#include <gio/gio.h>

#include "extractgeneric.h"
#include "storage.h"

#include "extract.h"
#include "cache.h"

#define ALBUMARTER_SERVICE    "com.nokia.albumart"
#define ALBUMARTER_PATH       "/com/nokia/albumart/Requester"
#define ALBUMARTER_INTERFACE  "com.nokia.albumart.Requester"

/**
 * SECTION:extract
 * @title: Extraction
 * @short_description: Extraction of music and movie art.
 * @include: libmediaart/mediaart.h
 *
 * The libmediaart library supports taking image data that you have extracted
 * from a media file and saving it into the media art cache, so that future
 * applications can display the media art without having to extract the image
 * again. This is done using the media_art_process_file() or
 * media_art_process_buffer() functions.
 *
 * Extracting new media art from a file needs to be done by your application.
 * Usually, when an application loads a media file any embedded images will be
 * made available as a side effect. For example, if you are using GStreamer any
 * images will be returned through the #GstTagList interface as %GST_TAG_IMAGE
 * tags.
 *
 * The media art cache requires that all images are saved as 'application/jpeg'
 * files. Embedded images can be in several formats, and
 * media_art_process_file() and media_art_process_buffer() functions will
 * convert the supplied image data into the correct format if
 * necessary. There are multiple backends that can be used for this,
 * and you can choose which is used at build time using the library's
 * 'configure' script.
 *
 * If there is no embedded media art in a file,
 * media_art_process_file() and media_art_process_buffer() functions will
 * look in the directory that contains the media file for likely media
 * art using a simple heuristic.
 **/

typedef struct {
	gboolean disable_requests;

	GHashTable *media_art_cache;
	GDBusConnection *connection;
	Storage *storage;
} MediaArtProcessPrivate;

static const gchar *media_art_type_name[MEDIA_ART_TYPE_COUNT] = {
	"invalid",
	"album",
	"video"
};

typedef struct {
	MediaArtProcess *process;
	gchar *art_path;
	gchar *local_uri;
} FileInfo;

typedef struct {
	gchar *uri;
	MediaArtType type;
	gchar *artist_strdown;
	gchar *title_strdown;
} MediaArtSearch;

typedef enum {
	IMAGE_MATCH_EXACT = 0,
	IMAGE_MATCH_EXACT_SMALL = 1,
	IMAGE_MATCH_SAME_DIRECTORY = 2,
	IMAGE_MATCH_TYPE_COUNT
} ImageMatchType;

static void media_art_queue_cb                    (GObject        *source_object,
                                                   GAsyncResult   *res,
                                                   gpointer        user_data);
static void media_art_process_initable_iface_init (GInitableIface *iface);

G_DEFINE_TYPE_WITH_CODE (MediaArtProcess, media_art_process, G_TYPE_OBJECT,
			 G_IMPLEMENT_INTERFACE (G_TYPE_INITABLE,
						media_art_process_initable_iface_init)
			 G_ADD_PRIVATE (MediaArtProcess))

static void
media_art_process_finalize (GObject *object)
{
	MediaArtProcessPrivate *private;
	MediaArtProcess *process;

	process = MEDIA_ART_PROCESS (object);
	private = media_art_process_get_instance_private (process);

	if (private->storage) {
		g_object_unref (private->storage);
	}

	if (private->connection) {
		g_object_unref (private->connection);
	}

	if (private->media_art_cache) {
		g_hash_table_unref (private->media_art_cache);
	}

	media_art_plugin_shutdown ();

	G_OBJECT_CLASS (media_art_process_parent_class)->finalize (object);
}

static gboolean
media_art_process_initable_init (GInitable     *initable,
                                 GCancellable  *cancellable,
                                 GError       **error)
{
	MediaArtProcessPrivate *private;
	MediaArtProcess *process;
	GError *local_error = NULL;

	process = MEDIA_ART_PROCESS (initable);
	private = media_art_process_get_instance_private (process);

	g_debug ("Initializing media art processing requirements...");

	media_art_plugin_init (0);

	/* Cache to know if we have already handled uris */
	private->media_art_cache = g_hash_table_new_full (g_str_hash,
	                                                  g_str_equal,
	                                                  (GDestroyNotify) g_free,
	                                                  NULL);

	/* Signal handler for new album art from the extractor */
	private->connection = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, &local_error);

	if (!private->connection) {
		g_critical ("Could not connect to the D-Bus session bus, %s",
		            local_error ? local_error->message : "no error given.");
		g_propagate_error (error, local_error);

		return FALSE;
	}

	private->storage = storage_new ();
	if (!private->storage) {
		g_critical ("Could not start storage module for removable media detection");

		if (error) {
			*error = g_error_new (media_art_error_quark (),
			                      MEDIA_ART_ERROR_NO_STORAGE,
			                      "Could not initialize storage module");
		}

		return FALSE;
	}

	return TRUE;
}

static void
media_art_process_initable_iface_init (GInitableIface *iface)
{
	iface->init = media_art_process_initable_init;
}

static void
media_art_process_class_init (MediaArtProcessClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = media_art_process_finalize;
}

static void
media_art_process_init (MediaArtProcess *thumbnailer)
{
}

/**
 * media_art_process_new:
 * @error: Pointer to potential GLib / MediaArt error, or %NULL
 *
 * Initialize a GObject for processing and extracting media art.
 *
 * This function initializes cache hash tables, backend plugins,
 * storage modules used for removable devices and a connection to D-Bus.
 *
 * Returns: A new #MediaArtProcess object on success or %NULL if
 * @error is set. This object must be freed using g_object_unref().
 *
 * Since: 0.5.0
 */
MediaArtProcess *
media_art_process_new (GError **error)
{
	return g_initable_new (MEDIA_ART_TYPE_PROCESS, NULL, error, NULL);
}

static GDir *
get_parent_g_dir (const gchar  *uri,
                  gchar       **dirname,
                  GError      **error)
{
	GFile *file, *dirf;
	GDir *dir;

	g_return_val_if_fail (dirname != NULL, NULL);

	*dirname = NULL;

	file = g_file_new_for_uri (uri);
	dirf = g_file_get_parent (file);

	if (dirf) {
		*dirname = g_file_get_path (dirf);
		g_object_unref (dirf);
	}

	g_object_unref (file);

	if (*dirname == NULL) {
		*error = g_error_new (G_FILE_ERROR,
		                      G_FILE_ERROR_EXIST,
		                      "No parent directory found for '%s'",
		                      uri);
		return NULL;
	}

	dir = g_dir_open (*dirname, 0, error);

	return dir;
}

static gchar *
checksum_for_data (GChecksumType  checksum_type,
                   const guchar  *data,
                   gsize          length)
{
	GChecksum *checksum;
	gchar *retval;

	checksum = g_checksum_new (checksum_type);
	if (!checksum) {
		return NULL;
	}

	g_checksum_update (checksum, data, length);
	retval = g_strdup (g_checksum_get_string (checksum));
	g_checksum_free (checksum);

	return retval;
}

static gboolean
file_get_checksum_if_exists (GChecksumType   checksum_type,
                             const gchar    *path,
                             gchar         **md5,
                             gboolean        check_jpeg,
                             gboolean       *is_jpeg)
{
	GFile *file = g_file_new_for_path (path);
	GFileInputStream *stream;
	GChecksum *checksum;
	gboolean retval;

	checksum = g_checksum_new (checksum_type);

	if (!checksum) {
		g_debug ("Can't create checksum engine");
		g_object_unref (file);
		return FALSE;
	}

	stream = g_file_read (file, NULL, NULL);

	if (stream) {
		gssize rsize;
		guchar buffer[1024];

		/* File exists & readable always means true retval */
		retval = TRUE;

		if (check_jpeg) {
			if (g_input_stream_read_all (G_INPUT_STREAM (stream),
			                             buffer,
			                             3,
			                             (gsize *) &rsize,
			                             NULL,
			                             NULL)) {
				if (rsize >= 3 &&
				    buffer[0] == 0xff &&
				    buffer[1] == 0xd8 &&
				    buffer[2] == 0xff) {
					if (is_jpeg) {
						*is_jpeg = TRUE;
					}

					/* Add the read bytes to the checksum */
					g_checksum_update (checksum, buffer, rsize);
				} else {
					/* Larger than 3 bytes but incorrect jpeg header */
					if (is_jpeg) {
						*is_jpeg = FALSE;
					}
					goto end;
				}
			} else {
				/* Smaller than 3 bytes, not a jpeg */
				if (is_jpeg) {
					*is_jpeg = FALSE;
				}
				goto end;
			}
		}

		while ((rsize = g_input_stream_read (G_INPUT_STREAM (stream),
		                                     buffer,
		                                     1024,
		                                     NULL,
		                                     NULL)) > 0) {
			g_checksum_update (checksum, buffer, rsize);
		}

		if (md5) {
			*md5 = g_strdup (g_checksum_get_string (checksum));
		}

	} else {
		g_debug ("%s isn't readable while calculating MD5 checksum", path);
		/* File doesn't exist or isn't readable */
		retval = FALSE;
	}

end:
	if (stream) {
		g_object_unref (stream);
	}

	g_checksum_free (checksum);
	g_object_unref (file);

	return retval;
}

static gboolean
convert_from_other_format (const gchar  *found,
                           const gchar  *target,
                           const gchar  *album_path,
                           const gchar  *artist,
                           GError      **error)
{
	gboolean retval;
	gchar *sum1 = NULL;
	gchar *target_temp;

	target_temp = g_strdup_printf ("%s-tmp", target);

	if (!media_art_file_to_jpeg (found, target_temp, error)) {
		g_free (target_temp);
		return FALSE;
	}

	if (artist == NULL || g_strcmp0 (artist, " ") == 0) {
		retval = g_rename (target_temp, album_path) == 0;

		if (!retval) {
			g_set_error (error,
			             media_art_error_quark (),
			             MEDIA_ART_ERROR_RENAME_FAILED,
			             "Could not rename '%s' to '%s': %s",
			             target_temp,
			             album_path,
			             g_strerror (errno));
		}

		g_debug ("rename(%s, %s) error: %s",
		         target_temp,
		         album_path,
		         g_strerror (errno));
	} else if (file_get_checksum_if_exists (G_CHECKSUM_MD5,
	                                        target_temp,
	                                        &sum1,
	                                        FALSE,
	                                        NULL)) {
		gchar *sum2 = NULL;

		if (file_get_checksum_if_exists (G_CHECKSUM_MD5,
		                                 album_path,
		                                 &sum2,
		                                 FALSE,
		                                 NULL)) {
			if (g_strcmp0 (sum1, sum2) == 0) {

				/* If album-space-md5.jpg is the same as found,
				 * make a symlink */
				retval = symlink (album_path, target) == 0;

				if (!retval) {
					g_set_error (error,
					             media_art_error_quark (),
					             MEDIA_ART_ERROR_RENAME_FAILED,
					             "Could not rename '%s' to '%s': %s",
					             album_path,
					             target,
					             g_strerror (errno));
				}

				g_debug ("symlink(%s, %s) error: %s",
				         album_path,
				         target,
				         g_strerror (errno));

				g_unlink (target_temp);
			} else {

				/* If album-space-md5.jpg isn't the same as found,
				 * make a new album-md5-md5.jpg (found -> target) */
				retval = g_rename (target_temp, album_path) == 0;

				if (!retval) {
					g_set_error (error,
					             media_art_error_quark (),
					             MEDIA_ART_ERROR_RENAME_FAILED,
					             "Could not rename '%s' to '%s': %s",
					             target_temp,
					             album_path,
					             g_strerror (errno));
				}

				g_debug ("rename(%s, %s) error: %s",
				         target_temp,
				         album_path,
				         g_strerror (errno));
			}

			g_free (sum2);
		} else {
			/* If there's not yet a album-space-md5.jpg, make one,
			 * and symlink album-md5-md5.jpg to it */
			retval = g_rename (target_temp, album_path) == 0;

			if (!retval) {
				g_set_error (error,
				             media_art_error_quark (),
				             MEDIA_ART_ERROR_RENAME_FAILED,
				             "Could not rename '%s' to '%s': %s",
				             album_path,
				             target,
				             g_strerror (errno));
			} else {
				retval = symlink (album_path, target) == 0;

				if (!retval) {
					g_set_error (error,
					             media_art_error_quark (),
					             MEDIA_ART_ERROR_SYMLINK_FAILED,
					             "Could not rename '%s' to '%s': %s",
					             album_path,
					             target,
					             g_strerror (errno));
				}

				g_debug ("symlink(%s,%s) error: %s",
					album_path,
					target,
					g_strerror (errno));
			}
		}

		g_free (sum1);
	} else {
		g_debug ("Can't read %s while calculating checksum", target_temp);
		/* Can't read the file that it was converted to, strange ... */
		g_unlink (target_temp);
	}

	g_free (target_temp);

	return retval;
}

static MediaArtSearch *
media_art_search_new (const gchar  *uri,
                      MediaArtType  type,
                      const gchar  *artist,
                      const gchar  *title)
{
	MediaArtSearch *search;
	gchar *temp;

	search = g_slice_new0 (MediaArtSearch);
	search->uri = g_strdup (uri);
	search->type = type;

	if (artist) {
		temp = media_art_strip_invalid_entities (artist);
		search->artist_strdown = g_utf8_strdown (temp, -1);
		g_free (temp);
	}

	temp = media_art_strip_invalid_entities (title);
	search->title_strdown = g_utf8_strdown (temp, -1);
	g_free (temp);

	return search;
}

static void
media_art_search_free (MediaArtSearch *search)
{
	g_free (search->uri);
	g_free (search->artist_strdown);
	g_free (search->title_strdown);

	g_slice_free (MediaArtSearch, search);
}

static ImageMatchType
classify_image_file (MediaArtSearch *search,
                     const gchar    *file_name_strdown)
{
	if ((search->artist_strdown && search->artist_strdown[0] != '\0' &&
	     strstr (file_name_strdown, search->artist_strdown)) ||
	    (search->title_strdown && search->title_strdown[0] != '\0' &&
	     strstr (file_name_strdown, search->title_strdown))) {
		return IMAGE_MATCH_EXACT;
	}

	if (search->type == MEDIA_ART_ALBUM) {
		/* Accept cover, front, folder, AlbumArt_{GUID}_Large (first choice)
		 * second choice is AlbumArt_{GUID}_Small and AlbumArtSmall. We
		 * don't support just AlbumArt. (it must have a Small or Large) */

		if (strstr (file_name_strdown, "cover") ||
		    strstr (file_name_strdown, "front") ||
		    strstr (file_name_strdown, "folder")) {
			return IMAGE_MATCH_EXACT;
		}

		if (strstr (file_name_strdown, "albumart")) {
			if (strstr (file_name_strdown, "large")) {
				return IMAGE_MATCH_EXACT;
			} else if (strstr (file_name_strdown, "small")) {
				return IMAGE_MATCH_EXACT_SMALL;
			}
		}
	}

	if (search->type == MEDIA_ART_VIDEO) {
		if (strstr (file_name_strdown, "folder") ||
		    strstr (file_name_strdown, "poster")) {
			return IMAGE_MATCH_EXACT;
		}
	}

	/* Lowest priority for other images, but we still might use it for videos */
	return IMAGE_MATCH_SAME_DIRECTORY;
}

static gchar *
media_art_find_by_artist_and_title (const gchar  *uri,
                                    MediaArtType  type,
                                    const gchar  *artist,
                                    const gchar  *title)
{
	MediaArtSearch *search;
	GDir *dir;
	GError *error = NULL;
	gchar *dirname = NULL;
	const gchar *name;
	gchar *name_utf8, *name_strdown;
	guint i;
	gchar *art_file_name;
	gchar *art_file_path;
	gint priority;

	GList *image_list[IMAGE_MATCH_TYPE_COUNT] = { NULL, };

	g_return_val_if_fail (type > MEDIA_ART_NONE && type < MEDIA_ART_TYPE_COUNT, FALSE);
	g_return_val_if_fail (title != NULL, FALSE);

	dir = get_parent_g_dir (uri, &dirname, &error);

	if (!dir) {
		g_debug ("Media art directory could not be opened: %s",
		         error ? error->message : "no error given");

		g_clear_error (&error);
		g_free (dirname);

		return NULL;
	}

	/* First, classify each file in the directory as either an image, relevant
	 * to the media object in question, or irrelevant. We use this information
	 * to decide if the image is a cover or if the file is in a random directory.
	 */

	search = media_art_search_new (uri, type, artist, title);

	for (name = g_dir_read_name (dir);
	     name != NULL;
	     name = g_dir_read_name (dir)) {

		name_utf8 = g_filename_to_utf8 (name, -1, NULL, NULL, NULL);

		if (!name_utf8) {
			g_debug ("Could not convert filename '%s' to UTF-8", name);
			continue;
		}

		name_strdown = g_utf8_strdown (name_utf8, -1);

		if (g_str_has_suffix (name_strdown, "jpeg") ||
		    g_str_has_suffix (name_strdown, "jpg") ||
		    g_str_has_suffix (name_strdown, "png")) {

			priority = classify_image_file (search, name_strdown);
			image_list[priority] = g_list_prepend (image_list[priority],
			                                       name_utf8);
		} else {
			g_free (name_utf8);
		}

		g_free (name_strdown);
	}

	/* Use the results to pick a media art image */

	art_file_name = NULL;
	art_file_path = NULL;

	if (g_list_length (image_list[IMAGE_MATCH_EXACT]) > 0) {
		art_file_name = g_strdup (image_list[IMAGE_MATCH_EXACT]->data);
	} else if (g_list_length (image_list[IMAGE_MATCH_EXACT_SMALL]) > 0) {
		art_file_name = g_strdup (image_list[IMAGE_MATCH_EXACT_SMALL]->data);
	} else {
		if (type == MEDIA_ART_VIDEO && g_list_length (image_list[IMAGE_MATCH_SAME_DIRECTORY]) == 1) {
			art_file_name = g_strdup (image_list[IMAGE_MATCH_SAME_DIRECTORY]->data);
		}
	}

	for (i = 0; i < IMAGE_MATCH_TYPE_COUNT; i ++) {
		g_list_foreach (image_list[i], (GFunc)g_free, NULL);
		g_list_free (image_list[i]);
	}

	if (art_file_name) {
		art_file_path = g_build_filename (dirname, art_file_name, NULL);
		g_free (art_file_name);
	} else {
		g_debug ("Album art NOT found in same directory");
		art_file_path = NULL;
	}

	media_art_search_free (search);
	g_dir_close (dir);
	g_free (dirname);

	return art_file_path;
}

static gboolean
get_heuristic (MediaArtType   type,
               const gchar   *filename_uri,
               const gchar   *local_uri,
               const gchar   *artist,
               const gchar   *title,
               GError       **error)
{
	gchar *art_file_path = NULL;
	gchar *album_art_file_path = NULL;
	gchar *target = NULL;
	gchar *artist_stripped = NULL;
	gchar *title_stripped = NULL;
	gboolean retval = FALSE;

	if (title == NULL || title[0] == '\0') {
		g_set_error (error,
		             media_art_error_quark (),
		             MEDIA_ART_ERROR_NO_TITLE,
		             "Title is required, but was not provided, or was empty");
		return FALSE;
	}

	if (artist) {
		artist_stripped = media_art_strip_invalid_entities (artist);
	}
	title_stripped = media_art_strip_invalid_entities (title);

	media_art_get_path (artist_stripped,
	                    title_stripped,
	                    media_art_type_name[type],
	                    NULL,
	                    &target,
	                    NULL);

	/* Copy from local album art (.mediaartlocal) to spec */
	if (local_uri) {
		GFile *local_file, *file;

		local_file = g_file_new_for_uri (local_uri);

		if (g_file_query_exists (local_file, NULL)) {
			g_debug ("Album art being copied from local (.mediaartlocal) file:'%s'",
			         local_uri);

			file = g_file_new_for_path (target);

			g_file_copy_async (local_file,
			                   file,
			                   0,
			                   0,
			                   NULL,
			                   NULL,
			                   NULL,
			                   NULL,
			                   NULL);

			g_object_unref (file);
			g_object_unref (local_file);

			g_free (target);
			g_free (artist_stripped);
			g_free (title_stripped);

			return TRUE;
		}

		g_object_unref (local_file);
	}

	art_file_path = media_art_find_by_artist_and_title (filename_uri,
	                                                    type,
	                                                    artist,
	                                                    title);

	if (!art_file_path) {
		// FIXME: Do we GError here?

		g_free (art_file_path);
		g_free (album_art_file_path);

		g_free (target);
		g_free (artist_stripped);
		g_free (title_stripped);

		return FALSE;
	}

	if (g_str_has_suffix (art_file_path, "jpeg") ||
	    g_str_has_suffix (art_file_path, "jpg")) {
		gboolean is_jpeg = FALSE;
		gchar *sum1 = NULL;

		if (type != MEDIA_ART_ALBUM ||
		    (artist == NULL || g_strcmp0 (artist, " ") == 0)) {
			GFile *art_file;
			GFile *target_file;
			GError *local_error = NULL;

			g_debug ("Album art (JPEG) found in same directory being used:'%s'",
			         art_file_path);

			target_file = g_file_new_for_path (target);
			art_file = g_file_new_for_path (art_file_path);

			g_file_copy (art_file,
			             target_file,
			             0,
			             NULL,
			             NULL,
			             NULL,
			             &local_error);

			if (local_error) {
				g_debug ("%s", local_error->message);
				g_propagate_error (error, local_error);
			}

			g_object_unref (art_file);
			g_object_unref (target_file);
		} else if (file_get_checksum_if_exists (G_CHECKSUM_MD5,
		                                        art_file_path,
		                                        &sum1,
		                                        TRUE,
		                                        &is_jpeg)) {
			/* Avoid duplicate artwork for each track in an album */
			media_art_get_path (NULL,
			                    title_stripped,
			                    media_art_type_name [type],
			                    NULL,
			                    &album_art_file_path,
			                    NULL);

			if (is_jpeg) {
				gchar *sum2 = NULL;

				g_debug ("Album art (JPEG) found in same directory being used:'%s'", art_file_path);

				if (file_get_checksum_if_exists (G_CHECKSUM_MD5,
				                                 album_art_file_path,
				                                 &sum2,
				                                 FALSE,
				                                 NULL)) {
					if (g_strcmp0 (sum1, sum2) == 0) {
						/* If album-space-md5.jpg is the same as found,
						 * make a symlink */
						retval = symlink (album_art_file_path, target) == 0;

						if (!retval) {
							g_set_error (error,
							             media_art_error_quark (),
							             MEDIA_ART_ERROR_SYMLINK_FAILED,
							             "Could not link '%s' to '%s': %s",
							             album_art_file_path,
							             target,
							             g_strerror (errno));
						}

						g_debug ("symlink(%s, %s) error: %s",
						         album_art_file_path,
						         target,
						         g_strerror (errno));
					} else {
						GFile *art_file;
						GFile *target_file;

						/* If album-space-md5.jpg isn't the same as found,
						 * make a new album-md5-md5.jpg (found -> target) */
						target_file = g_file_new_for_path (target);
						art_file = g_file_new_for_path (art_file_path);
						retval = g_file_copy (art_file,
						                      target_file,
						                      0,
						                      NULL,
						                      NULL,
						                      NULL,
						                      error);

						g_object_unref (art_file);
						g_object_unref (target_file);
					}

					g_free (sum2);
				} else {
					GFile *art_file;
					GFile *album_art_file;

					/* If there's not yet a album-space-md5.jpg, make one,
					 * and symlink album-md5-md5.jpg to it */
					album_art_file = g_file_new_for_path (album_art_file_path);
					art_file = g_file_new_for_path (art_file_path);
					retval = g_file_copy (art_file,
					                      album_art_file,
					                      0,
					                      NULL,
					                      NULL,
					                      NULL,
					                      error);

					if (retval) {
						retval = symlink (album_art_file_path, target) == 0;

						if (!retval) {
							g_set_error (error,
							             media_art_error_quark (),
							             MEDIA_ART_ERROR_SYMLINK_FAILED,
							             "Could not link '%s' to '%s': %s",
							             album_art_file_path,
							             target,
							             g_strerror (errno));
						}

						g_debug ("symlink(%s, %s) error: %s",
						         album_art_file_path, target,
						         g_strerror (errno));
					}

					g_object_unref (album_art_file);
					g_object_unref (art_file);
				}
			} else {
				g_debug ("Album art found in same directory but not a real JPEG file (trying to convert): '%s'", art_file_path);
				retval = convert_from_other_format (art_file_path,
				                                    target,
				                                    album_art_file_path,
				                                    artist,
				                                    error);
			}

			g_free (sum1);
		} else {
			/* Can't read contents of the cover.jpg file ... */
			retval = FALSE;
		}
	} else if (g_str_has_suffix (art_file_path, "png")) {
		if (!album_art_file_path) {
			media_art_get_path (NULL,
			                    title_stripped,
			                    media_art_type_name[type],
			                    NULL,
			                    &album_art_file_path,
			                    NULL);
		}

		g_debug ("Album art (PNG) found in same directory being used:'%s'", art_file_path);
		retval = convert_from_other_format (art_file_path,
		                                    target,
		                                    album_art_file_path,
		                                    artist,
		                                    error);
	}

	g_free (art_file_path);
	g_free (album_art_file_path);

	g_free (target);
	g_free (artist_stripped);
	g_free (title_stripped);

	return retval;
}

static inline gboolean
is_buffer_jpeg (const gchar         *mime,
                const unsigned char *buffer,
                size_t               buffer_len)
{
	if (buffer == NULL || buffer_len < 3) {
		return FALSE;
	}

	if (g_strcmp0 (mime, "image/jpeg") == 0 ||
	    g_strcmp0 (mime, "JPG") == 0) {
		return TRUE;
	}

	if (buffer[0] == 0xff &&
	    buffer[1] == 0xd8 &&
	    buffer[2] == 0xff) {
		return TRUE;
	}

	return FALSE;
}

static gboolean
media_art_set (const unsigned char  *buffer,
               size_t                len,
               const gchar          *mime,
               MediaArtType          type,
               const gchar          *artist,
               const gchar          *title,
               GError              **error)
{
	gchar *local_path;
	gchar *album_path;
	gchar *md5_album = NULL;
	gchar *md5_tmp = NULL;
	gchar *temp;
	gboolean retval = FALSE;

	g_return_val_if_fail (type > MEDIA_ART_NONE && type < MEDIA_ART_TYPE_COUNT, FALSE);
	g_return_val_if_fail (artist != NULL, FALSE);
	g_return_val_if_fail (title != NULL, FALSE);

	/* What we do here:
	 *
	 * NOTE: local_path is the final location for the media art
	 * always here.
	 *
	 * 1. Get details based on artist and title.
	 * 2. If not ALBUM! or artist is unknown:
	 *       i) save buffer to jpeg only.
	 * 3. If no cache for ALBUM!:
	 *       i) save buffer to jpeg.
	 *      ii) symlink to local_path.
	 * 4. If buffer is jpeg:
	 *       i) If the MD5sum is the same for buffer and existing
	 *          file, symlink to local_path.
	 *      ii) Otherwise, save buffer to jpeg and call it local_path.
	 * 5. If buffer is not jpeg, save to disk:
	 *       i) Compare to existing md5sum cache for ALBUM.
	 *      ii) If same, unlink new jpeg from buffer and symlink to local_path.
	 *     iii) If not same, rename new buffer to local_path.
	 *      iv) If we couldn't save the buffer or read from the new
	 *          cache, unlink it...
	 */

	/* 1. Get details based on artist and title */
	media_art_get_path (artist,
	                    title,
	                    media_art_type_name[type],
	                    NULL,
	                    &local_path,
	                    NULL);

	/* 2. If not ALBUM! or artist is unknown:
	 *       i) save buffer to jpeg only.
	 */
	if (type != MEDIA_ART_ALBUM || (artist == NULL || g_strcmp0 (artist, " ") == 0)) {
		retval = media_art_buffer_to_jpeg (buffer, len, mime, local_path, error);
		g_debug ("Saving buffer to jpeg (%ld bytes) --> '%s'", len, local_path);
		g_free (local_path);

		return retval;
	}

	/* 3. If no cache for ALBUM!:
	 *       i) save buffer to jpeg.
	 *      ii) symlink to local_path.
	 */
	media_art_get_path (NULL,
	                    title,
	                    media_art_type_name[type],
	                    NULL,
	                    &album_path,
	                    NULL);

	if (!g_file_test (album_path, G_FILE_TEST_EXISTS)) {
		retval = TRUE;

		if (media_art_buffer_to_jpeg (buffer, len, mime, album_path, error)) {
			g_debug ("Saving buffer to jpeg (%ld bytes) --> '%s'", len, album_path);

			/* If album-space-md5.jpg doesn't
			 * exist, make one and make a symlink
			 * to album-md5-md5.jpg
			 */
			retval = symlink (album_path, local_path) == 0;

			if (!retval) {
				g_set_error (error,
				             media_art_error_quark (),
				             MEDIA_ART_ERROR_SYMLINK_FAILED,
				             "Could not link '%s' to '%s': %s",
				             album_path,
				             local_path,
				             g_strerror (errno));
			}

			g_debug ("Creating symlink '%s' --> '%s', %s",
			         album_path,
			         local_path,
			         retval ? g_strerror (errno) : "no error given");
		}

		g_free (album_path);
		g_free (local_path);

		return retval;
	}

	if (!file_get_checksum_if_exists (G_CHECKSUM_MD5,
	                                  album_path,
	                                  &md5_album,
	                                  FALSE,
	                                  NULL)) {
		g_debug ("No MD5 checksum found for album path:'%s'", album_path);

		g_free (album_path);
		g_free (local_path);

		return TRUE;
	}

	/* 4. If buffer is jpeg:
	 *       i) If the MD5sum is the same for buffer and existing
	 *          file, symlink to local_path.
	 *      ii) Otherwise, save buffer to jpeg and call it local_path.
	 */
	if (is_buffer_jpeg (mime, buffer, len)) {
		gchar *md5_data;

		md5_data = checksum_for_data (G_CHECKSUM_MD5, buffer, len);

		/* If album-space-md5.jpg is the same as buffer, make
		 * a symlink to album-md5-md5.jpg
		 */
		if (g_strcmp0 (md5_data, md5_album) == 0) {
			retval = symlink (album_path, local_path) == 0;

			if (!retval) {
				g_set_error (error,
				             media_art_error_quark (),
				             MEDIA_ART_ERROR_SYMLINK_FAILED,
				             "Could not link '%s' to '%s': %s",
				             album_path,
				             local_path,
				             g_strerror (errno));
			}

			g_debug ("Creating symlink '%s' --> '%s', %s",
			         album_path,
			         local_path,
			         retval ? g_strerror (errno) : "no error given");
		} else {
			/* If album-space-md5.jpg isn't the same as
			 * buffer, make a new album-md5-md5.jpg
			 */
			retval = media_art_buffer_to_jpeg (buffer, len, mime, local_path, error);
			g_debug ("Saving buffer to jpeg (%ld bytes) --> '%s'", len, local_path);
		}

		g_free (md5_data);
		g_free (album_path);
		g_free (local_path);

		return retval;
	}

	/* 5. If buffer is not jpeg:
	 *       i) Compare to existing md5sum data with cache for ALBUM.
	 *      ii) If same, unlink new jpeg from buffer and symlink to local_path.
	 *     iii) If not same, rename new buffer to local_path.
	 *      iv) If we couldn't save the buffer or read from the new
	 *          cache, unlink it...
	 */
	temp = g_strdup_printf ("%s-tmp", album_path);

	/* If buffer isn't a JPEG */
	if (!media_art_buffer_to_jpeg (buffer, len, mime, temp, error)) {
		/* Can't read temp file ... */
		g_unlink (temp);

		g_free (temp);
		g_free (md5_album);
		g_free (album_path);
		g_free (local_path);

		return FALSE;
	}

	if (file_get_checksum_if_exists (G_CHECKSUM_MD5,
	                                 temp,
	                                 &md5_tmp,
	                                 FALSE,
	                                 NULL)) {
		if (g_strcmp0 (md5_tmp, md5_album) == 0) {
			/* If album-space-md5.jpg is the same as
			 * buffer, make a symlink to album-md5-md5.jpg
			 */
			retval = symlink (album_path, local_path) == 0;

			if (!retval) {
				g_set_error (error,
				             media_art_error_quark (),
				             MEDIA_ART_ERROR_SYMLINK_FAILED,
				             "Could not link '%s' to '%s': %s",
				             album_path,
				             local_path,
				             g_strerror (errno));
			}

			g_debug ("Creating symlink '%s' --> '%s', %s",
			         album_path,
			         local_path,
			         retval ? g_strerror (errno) : "no error given");
		} else {
			/* If album-space-md5.jpg isn't the same as
			 * buffer, make a new album-md5-md5.jpg
			 */
			retval = g_rename (temp, local_path) == 0;

			if (!retval) {
				g_set_error (error,
				             media_art_error_quark (),
				             MEDIA_ART_ERROR_RENAME_FAILED,
				             "Could not rename '%s' to '%s': %s",
				             temp,
				             local_path,
				             g_strerror (errno));
			}

			g_debug ("Renaming temp file '%s' --> '%s', %s",
			         temp,
			         local_path,
			         retval ? g_strerror (errno) : "no error given");
		}

		g_free (md5_tmp);
	}

	/* Clean up */
	g_unlink (temp);
	g_free (temp);

	g_free (md5_album);
	g_free (album_path);
	g_free (local_path);

	return retval;
}

static FileInfo *
file_info_new (MediaArtProcess *process,
               const gchar     *local_uri,
               const gchar     *art_path)
{
	FileInfo *fi;

	fi = g_slice_new (FileInfo);

	fi->process = g_object_ref (process);
	fi->local_uri = g_strdup (local_uri);
	fi->art_path = g_strdup (art_path);

	return fi;
}

static void
file_info_free (FileInfo *fi)
{
	if (!fi) {
		return;
	}

	g_free (fi->local_uri);
	g_free (fi->art_path);
	g_object_unref (fi->process);

	g_slice_free (FileInfo, fi);
}

static void
media_art_request_download (MediaArtProcess *process,
                            MediaArtType     type,
                            const gchar     *album,
                            const gchar     *artist,
                            const gchar     *local_uri,
                            const gchar     *art_path)
{
	MediaArtProcessPrivate *private;

	private = media_art_process_get_instance_private (process);

	if (private->connection) {
		FileInfo *info;

		if (private->disable_requests) {
			return;
		}

		if (type != MEDIA_ART_ALBUM) {
			return;
		}

		info = file_info_new (process, local_uri, art_path);

		g_dbus_connection_call (private->connection,
		                        ALBUMARTER_SERVICE,
		                        ALBUMARTER_PATH,
		                        ALBUMARTER_INTERFACE,
		                        "Queue",
		                        g_variant_new ("(sssu)",
		                                       artist ? artist : "",
		                                       album ? album : "",
		                                       "album",
		                                       0),
		                        NULL,
		                        G_DBUS_CALL_FLAGS_NONE,
		                        -1,
		                        NULL,
		                        media_art_queue_cb,
		                        info);
	}
}

static void
media_art_copy_to_local (MediaArtProcess *process,
                         const gchar     *filename,
                         const gchar     *local_uri)
{
	MediaArtProcessPrivate *private;
	GSList *roots, *l;
	gboolean on_removable_device = FALSE;
	guint flen;

	private = media_art_process_get_instance_private (process);

	roots = storage_get_device_roots (private->storage, STORAGE_REMOVABLE, FALSE);
	flen = strlen (filename);

	for (l = roots; l; l = l->next) {
		guint len;

		len = strlen (l->data);

		if (flen >= len && strncmp (filename, l->data, len)) {
			on_removable_device = TRUE;
			break;
		}
	}

	g_slist_foreach (roots, (GFunc) g_free, NULL);
	g_slist_free (roots);

	if (on_removable_device) {
		GFile *local_file, *from;

		from = g_file_new_for_path (filename);
		local_file = g_file_new_for_uri (local_uri);

		/* We don't try to overwrite, but we also ignore all errors.
		 * Such an error could be that the removable device is
		 * read-only. Well that's fine then ... ignore */

		if (!g_file_query_exists (local_file, NULL)) {
			GFile *dirf;

			dirf = g_file_get_parent (local_file);
			if (dirf) {
				/* Parent file may not exist, as if the file is in the
				 * root of a gvfs mount. In this case we won't try to
				 * create the parent directory, just try to copy the
				 * file there. */
				g_file_make_directory_with_parents (dirf, NULL, NULL);
				g_object_unref (dirf);
			}

			g_debug ("Copying media art from:'%s' to:'%s'",
			         filename, local_uri);

			g_file_copy_async (from, local_file, 0, 0,
			                   NULL, NULL, NULL, NULL, NULL);
		}

		g_object_unref (local_file);
		g_object_unref (from);
	}
}

static void
media_art_queue_cb (GObject      *source_object,
                    GAsyncResult *res,
                    gpointer      user_data)
{
	MediaArtProcessPrivate *private;
	GError *error = NULL;
	FileInfo *fi;
	GVariant *v;

	fi = user_data;

	private = media_art_process_get_instance_private (fi->process);

	v = g_dbus_connection_call_finish ((GDBusConnection *) source_object, res, &error);

	if (error) {
		if (error->code == G_DBUS_ERROR_SERVICE_UNKNOWN) {
			private->disable_requests = TRUE;
		} else {
			g_warning ("%s", error->message);
		}
		g_clear_error (&error);
	}

	if (v) {
		g_variant_unref (v);
	}

	if (private->storage &&
	    fi->art_path &&
	    g_file_test (fi->art_path, G_FILE_TEST_EXISTS)) {
		media_art_copy_to_local (fi->process,
		                         fi->art_path,
		                         fi->local_uri);
	}

	file_info_free (fi);
}

/**
 * media_art_error_quark:
 *
 * Returns: the #GQuark used to identify media art errors in
 * GError structures.
 *
 * Since: 0.2.0
 **/
GQuark
media_art_error_quark (void)
{
	static GQuark error_quark = 0;

	if (G_UNLIKELY (error_quark == 0)) {
		error_quark = g_quark_from_static_string ("media-art-error-quark");
	}

	return error_quark;
}

static void
set_mtime (const gchar *filename,
           guint64      mtime)
{
	struct utimbuf buf;

	buf.actime = buf.modtime = mtime;
	utime (filename, &buf);
}

static
guint64
get_mtime (GFile   *file,
           GError **error)
{
	GFileInfo *info;
	GError *local_error = NULL;
	guint64 mtime;

	info = g_file_query_info (file,
	                          G_FILE_ATTRIBUTE_TIME_MODIFIED,
	                          G_FILE_QUERY_INFO_NONE,
	                          NULL,
	                          &local_error);

	if (G_UNLIKELY (local_error != NULL)) {
		g_propagate_error (error, local_error);
		mtime = 0;
	} else {
		mtime = g_file_info_get_attribute_uint64 (info, G_FILE_ATTRIBUTE_TIME_MODIFIED);
		g_object_unref (info);
	}

	return mtime;
}

static gchar *
get_heuristic_for_parent_path (GFile        *file,
                               MediaArtType  type,
                               const gchar  *artist,
                               const gchar  *title)
{
	gchar *key;
	gchar *parent_path = NULL;
	GFile *parent;

	if (!file) {
		return NULL;
	}

	parent = g_file_get_parent (file);
	if (parent) {
		parent_path = g_file_get_path (parent);
		g_object_unref (parent);
	}

	/* Just used for caching in our hash table */
	key = g_strdup_printf ("%s:%s:%s:%s",
	                       parent_path ? parent_path : "",
	                       media_art_type_name[type],
	                       artist ? artist : "",
	                       title ? title : "");

	g_free (parent_path);

	return key;
}

/**
 * media_art_process_buffer:
 * @process: Media art process object
 * @type: The type of media
 * @flags: The options given for how to process the media art
 * @related_file: File related to the media art
 * @buffer: (array length=len)(allow-none): a buffer containing @file data, or %NULL
 * @len: length of @buffer, or 0
 * @mime: MIME type of @buffer, or %NULL
 * @artist: (allow-none): The artist name @file or %NULL
 * @title: (allow-none): The title for @file or %NULL
 * @error: Pointer to potential GLib / MediaArt error, or %NULL
 *
 * Processes a memory buffer represented by @buffer and @len. If you
 * have extracted any embedded media art and passed this in as
 * @buffer, the image data will be converted to the correct format and
 * saved in the media art cache.
 *
 * Either @artist OR @title can be %NULL, but they can not both be %NULL.
 *
 * If @file is on a removable filesystem, the media art file will be saved in a
 * cache on the removable file system rather than on the host machine.
 *
 * Returns: %TRUE if @file could be processed or %FALSE if @error is set.
 *
 * Since: 0.5.0
 */
gboolean
media_art_process_buffer (MediaArtProcess       *process,
                          MediaArtType           type,
                          MediaArtProcessFlags   flags,
                          GFile                 *related_file,
                          const guchar          *buffer,
                          gsize                  len,
                          const gchar           *mime,
                          const gchar           *artist,
                          const gchar           *title,
                          GError               **error)
{
	GFile *cache_art_file, *local_art_file;
	GError *local_error = NULL;
	gchar *cache_art_path, *uri;
	gboolean processed, created;
	guint64 mtime, cache_mtime = 0;

	g_return_val_if_fail (MEDIA_ART_IS_PROCESS (process), FALSE);
	g_return_val_if_fail (type > MEDIA_ART_NONE && type < MEDIA_ART_TYPE_COUNT, FALSE);
	g_return_val_if_fail (G_IS_FILE (related_file), FALSE);
	g_return_val_if_fail (buffer != NULL, FALSE);
	g_return_val_if_fail (len > 0, FALSE);
	g_return_val_if_fail (artist != NULL || title != NULL, FALSE);

	processed = created = FALSE;

	uri = g_file_get_uri (related_file);
	g_debug ("Processing media art: artist:'%s', title:'%s', type:'%s', uri:'%s', flags:0x%.8x. Buffer is %ld bytes, mime:'%s'",
	         artist ? artist : "",
	         title ? title : "",
	         media_art_type_name[type],
	         uri,
	         flags,
	         (long int) len,
	         mime);

	mtime = get_mtime (related_file, &local_error);
	if (local_error != NULL) {
		g_debug ("Could not get mtime for related file '%s': %s",
		         uri,
		         local_error->message);
		g_propagate_error (error, local_error);
		g_free (uri);

		return FALSE;
	}

	media_art_get_file (artist,
	                    title,
	                    media_art_type_name[type],
	                    related_file,
	                    &cache_art_file,
	                    &local_art_file);

	cache_mtime = get_mtime (cache_art_file, &local_error);

	if (local_error &&
	    local_error->domain == g_io_error_quark () &&
	    local_error->code == G_IO_ERROR_NOT_FOUND) {
		/* cache_art_file not existing is the only error we
		 * accept here, anything else and we return.
		 */
		gchar *path;

		path = g_file_get_uri (cache_art_file);
		g_debug ("Cache for media art did not exist (%s)",
		         path);
		g_free (path);
		g_clear_error (&local_error);
	} else if (local_error) {
		g_free (uri);

		uri = g_file_get_uri (cache_art_file);
		g_debug ("Could not get mtime for cache '%s': %s",
		         uri,
		         local_error->message);
		g_free (uri);

		if (local_art_file) {
			g_object_unref (local_art_file);
		}

		g_propagate_error (error, local_error);

		return FALSE;
	}


	cache_art_path = g_file_get_path (cache_art_file);

	if (flags & MEDIA_ART_PROCESS_FLAGS_FORCE ||
	    cache_mtime == 0 || mtime > cache_mtime) {
		processed = media_art_set (buffer, len, mime, type, artist, title, error);
		set_mtime (cache_art_path, mtime);
	} else {
		g_debug ("Album art already exists for uri:'%s' as '%s'",
		         uri,
		         cache_art_path);
		processed = TRUE;
	}

	if (local_art_file && !g_file_query_exists (local_art_file, NULL)) {
		/* We can't reuse art_exists here because the
		 * situation might have changed
		 */
		if (g_file_query_exists (cache_art_file, NULL)) {
			gchar *local_art_uri;

			local_art_uri = g_file_get_uri (local_art_file);
			media_art_copy_to_local (process, cache_art_path, local_art_uri);
			g_free (local_art_uri);
		}
	}

	if (cache_art_file) {
		g_object_unref (cache_art_file);
	}

	if (local_art_file) {
		g_object_unref (local_art_file);
	}

	g_free (cache_art_path);
	g_free (uri);

	return processed;
}

/**
 * media_art_process_file:
 * @process: Media art process object
 * @type: The type of media
 * @flags: The options given for how to process the media art
 * @file: File to be processed
 * @artist: (allow-none): The artist name @file or %NULL
 * @title: (allow-none): The title for @file or %NULL
 * @error: Pointer to potential GLib / MediaArt error, or %NULL
 *
 * Process @file and check if media art exists and if it is up to date
 * with @artist and @title provided. Either @artist OR @title can be
 * %NULL, but they can not both be %NULL.
 *
 * NOTE: This function MAY retrieve media art for
 * @artist and @title combinations. It is not guaranteed and depends
 * on download services available over DBus at the time.
 *
 * In cases where download is unavailable, media_art_process_file()
 * will only try to procure a cache for possible media art found in
 * directories surrounding the location of @file. If a buffer or
 * memory chunk needs to be saved to disk which has been retrieved
 * from an MP3 (for example), you should use
 * media_art_process_buffer().
 *
 * The modification time (mtime) of @file is checked against the
 * cached stored for @artist and @title. If the cache is old or
 * doesn't exist, it will be updated. What this actually does is
 * update the mtime of the cache (a symlink) on the disk.
 *
 * If there is no actual media art stored locally (for example, it's
 * stored in a directory on a removable device), it is copied locally
 * (usually to an XDG cache directory).
 *
 * If @file is on a removable filesystem, the media art file will be
 * saved in a cache on the removable file system rather than on the
 * host machine.
 *
 * Returns: %TRUE if @file could be processed or %FALSE if @error is set.
 *
 * Since: 0.3.0
 */
gboolean
media_art_process_file (MediaArtProcess       *process,
                        MediaArtType           type,
                        MediaArtProcessFlags   flags,
                        GFile                 *file,
                        const gchar           *artist,
                        const gchar           *title,
                        GError               **error)
{
	MediaArtProcessPrivate *private;
	GFile *cache_art_file, *local_art_file;
	GError *local_error = NULL;
	gchar *cache_art_path, *uri;
	gboolean no_cache_or_old;
	guint64 mtime, cache_mtime;

	g_return_val_if_fail (MEDIA_ART_IS_PROCESS (process), FALSE);
	g_return_val_if_fail (type > MEDIA_ART_NONE && type < MEDIA_ART_TYPE_COUNT, FALSE);
	g_return_val_if_fail (G_IS_FILE (file), FALSE);
	g_return_val_if_fail (artist != NULL || title != NULL, FALSE);

	private = media_art_process_get_instance_private (process);

	uri = g_file_get_uri (file);
	g_debug ("Processing media art: artist:'%s', title:'%s', type:'%s', uri:'%s', flags:0x%.8x",
	         artist ? artist : "",
	         title ? title : "",
	         media_art_type_name[type],
	         uri,
	         flags);

	mtime = get_mtime (file, &local_error);
	if (local_error != NULL) {
		g_debug ("Could not get mtime for file '%s': %s",
		         uri,
		         local_error->message);
		g_propagate_error (error, local_error);
		g_free (uri);

		return FALSE;
	}

	media_art_get_file (artist,
	                    title,
	                    media_art_type_name[type],
	                    file,
	                    &cache_art_file,
	                    &local_art_file);

	cache_art_path = g_file_get_path (cache_art_file);

	cache_mtime = get_mtime (cache_art_file, &local_error);
	no_cache_or_old = cache_mtime == -1 || cache_mtime < mtime;

	if (no_cache_or_old) {
		/* If not, we perform a heuristic on the dir */
		gchar *key;

		key = get_heuristic_for_parent_path (file, type, artist, title);

		if (!g_hash_table_lookup (private->media_art_cache, key)) {
			gchar *local_art_uri;

			local_art_uri = g_file_get_uri (local_art_file);

			if (!get_heuristic (type, uri, local_art_uri, artist, title, error)) {
				/* If the heuristic failed, we
				 * request the download the
				 * media-art to the media-art
				 * downloaders
				 */
				media_art_request_download (process,
				                            type,
				                            artist,
				                            title,
				                            local_art_uri,
				                            cache_art_path);

				/* FIXME: Should return TRUE or FALSE? */
			}

			set_mtime (cache_art_path, mtime);

			g_hash_table_insert (private->media_art_cache,
			                     key,
			                     GINT_TO_POINTER(TRUE));
			g_free (local_art_uri);
		} else {
			g_free (key);
		}
	} else {
		g_debug ("Album art already exists for uri:'%s' as '%s'",
		         uri,
		         cache_art_path);
	}

	if (cache_art_file) {
		g_object_unref (cache_art_file);
	}

	if (local_art_file) {
		g_object_unref (local_art_file);
	}

	g_free (cache_art_path);
	g_free (uri);

	return TRUE;
}

/**
 * media_art_process_uri:
 * @process: Media art process object
 * @type: The type of media that contained the image data
 * @flags: How the media art is processed
 * @uri: URI of the media file that contained the data
 * @artist: (allow-none): The artist name @uri or %NULL
 * @title: (allow-none): The title for @uri or %NULL
 * @error: Pointer to potential GLib / MediaArt error, or %NULL
 *
 * This function calls media_art_process_file(), but takes the @uri as
 * a string rather than a #GFile object. Either @artist OR @title can be
 * %NULL, but they can not both be %NULL.
 *
 * Returns: %TRUE if @uri could be processed or %FALSE if @error is set.
 *
 * Since: 0.5.0
 */
gboolean
media_art_process_uri (MediaArtProcess       *process,
                       MediaArtType           type,
                       MediaArtProcessFlags   flags,
                       const gchar           *uri,
                       const gchar           *artist,
                       const gchar           *title,
                       GError               **error)
{
	GFile *file;
	gboolean result;

	g_return_val_if_fail (MEDIA_ART_IS_PROCESS (process), FALSE);
	g_return_val_if_fail (type > MEDIA_ART_NONE && type < MEDIA_ART_TYPE_COUNT, FALSE);
	g_return_val_if_fail (uri != NULL, FALSE);
	g_return_val_if_fail (artist != NULL || title != NULL, FALSE);

	file = g_file_new_for_uri (uri);

	result = media_art_process_file (process,
	                                 type,
	                                 flags,
	                                 file,
	                                 artist,
	                                 title,
	                                 error);

	g_object_unref (file);

	return result;
}
