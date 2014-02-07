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
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301  USA
 */

#include "config.h"

#include <string.h>

#include <glib.h>
#include <glib/gstdio.h>
#include <gio/gio.h>

#include "cache.h"

/**
 * SECTION:cache
 * @title: Caching and Management
 * @short_description: Caching and management of stored media art.
 * @include: libmediaart/mediaart.h
 *
 * These functions give you access to the media art that has been extracted
 * and saved in the user's XDG_CACHE_HOME directory.
 *
 * To find the media art for a given media file, use the function
 * media_art_get_file() (you can also use media_art_get_path(), which does the
 * same thing but for path strings instead of #GFile objects).
 *
 * If media art for the file is not found in the cache, these functions will
 * return %NULL. You may find some embedded media art upon loading the file,
 * and you can use media_art_process() to convert it to the correct format and
 * save it in the cache for next time. The media_art_process() function also
 * supports searching for external media art images using a basic heuristic.
 **/

static gboolean
media_art_strip_find_next_block (const gchar    *original,
                                 const gunichar  open_char,
                                 const gunichar  close_char,
                                 gint           *open_pos,
                                 gint           *close_pos)
{
	const gchar *p1, *p2;

	if (open_pos) {
		*open_pos = -1;
	}

	if (close_pos) {
		*close_pos = -1;
	}

	p1 = g_utf8_strchr (original, -1, open_char);
	if (p1) {
		if (open_pos) {
			*open_pos = p1 - original;
		}

		p2 = g_utf8_strchr (g_utf8_next_char (p1), -1, close_char);
		if (p2) {
			if (close_pos) {
				*close_pos = p2 - original;
			}

			return TRUE;
		}
	}

	return FALSE;
}

/**
 * media_art_strip_invalid_entities:
 * @original: original string
 *
 * Strip a albumname or artistname string to prepare it for calculating the
 * media art path with it. Certain characters and charactersets will be stripped
 * and a newly allocated string returned which you must free with g_free().
 *
 * This functions is used internally by media_art_get_file() and
 * media_art_get_path(). You will not normally need to call it yourself.
 *
 * This function provides the following features:
 * 1. Invalid characters include: ()[]<>{}_!@#$^&*+=|\/"'?~;
 * 2. Text inside brackets of (), {}, [] and <> pairs are removed.
 * 3. Multiples of space characters are removed.
 *
 * Returns: @original stripped of invalid characters which must be
 * freed. On error or if @original is empty, %NULL is returned.
 *
 * Since: 0.2.0
 */
gchar *
media_art_strip_invalid_entities (const gchar *original)
{
	GString *str_no_blocks;
	gchar **strv;
	gchar *str;
	gboolean blocks_done = FALSE;
	const gchar *p;
	const gchar *invalid_chars = "()[]<>{}_!@#$^&*+=|\\/\"'?~";
	const gchar *invalid_chars_delimiter = "*";
	const gchar *convert_chars = "\t";
	const gchar *convert_chars_delimiter = " ";
	const gunichar blocks[5][2] = {
		{ '(', ')' },
		{ '{', '}' },
		{ '[', ']' },
		{ '<', '>' },
		{  0,   0  }
	};

	g_return_val_if_fail (original != NULL, NULL);

	str_no_blocks = g_string_new ("");

	p = original;

	while (!blocks_done) {
		gint pos1, pos2, i;

		pos1 = -1;
		pos2 = -1;

		for (i = 0; blocks[i][0] != 0; i++) {
			gint start, end;

			/* Go through blocks, find the earliest block we can */
			if (media_art_strip_find_next_block (p, blocks[i][0], blocks[i][1], &start, &end)) {
				if (pos1 == -1 || start < pos1) {
					pos1 = start;
					pos2 = end;
				}
			}
		}

		/* If either are -1 we didn't find any */
		if (pos1 == -1) {
			/* This means no blocks were found */
			g_string_append (str_no_blocks, p);
			blocks_done = TRUE;
		} else {
			/* Append the test BEFORE the block */
			if (pos1 > 0) {
				g_string_append_len (str_no_blocks, p, pos1);
			}

			p = g_utf8_next_char (p + pos2);

			/* Do same again for position AFTER block */
			if (*p == '\0') {
				blocks_done = TRUE;
			}
		}
	}

	/* Now convert chars to lower case */
	str = g_utf8_strdown (str_no_blocks->str, -1);
	g_string_free (str_no_blocks, TRUE);

	/* Now strip invalid chars */
	g_strdelimit (str, invalid_chars, *invalid_chars_delimiter);
	strv = g_strsplit (str, invalid_chars_delimiter, -1);
	g_free (str);
	str = g_strjoinv (NULL, strv);
	g_strfreev (strv);

	/* Now convert chars */
	g_strdelimit (str, convert_chars, *convert_chars_delimiter);
	strv = g_strsplit (str, convert_chars_delimiter, -1);
	g_free (str);
	str = g_strjoinv (convert_chars_delimiter, strv);
	g_strfreev (strv);

	while (g_strrstr (str, "  ") != NULL) {
		/* Now remove double spaces */
		strv = g_strsplit (str, "  ", -1);
		g_free (str);
		str = g_strjoinv (" ", strv);
		g_strfreev (strv);
	}

	/* Now strip leading/trailing white space */
	g_strstrip (str);

	return str;
}

static gchar *
media_art_checksum_for_data (GChecksumType  checksum_type,
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

/**
 * media_art_get_file:
 * @artist: the artist
 * @title: the title
 * @prefix: the prefix for cache files, for example "album"
 * @file: (allow-none): the file or %NULL
 * @cache_file: (out) (transfer full) (allow-none): the location to store
 * a #GFile pointing to the user cache path, or %NULL
 * @local_file: (out) (transfer full) (allow-none): the location to store
 * a #GFile pointing to a cache file in the same filesystem than @file,
 * or %NULL.
 *
 * Gets the files pointing to cache files suitable for storing the media
 * art provided by the @artist, @title and @file arguments. @cache_file
 * will point to a location in the XDG user cache directory, meanwhile
 * @local_file will point to a cache file that resides in the same
 * filesystem than @file.
 *
 * When done, both #GFile<!-- -->s must be freed with g_object_unref() if
 * non-%NULL.
 *
 * Since: 0.2.0
 */
void
media_art_get_file (const gchar  *artist,
                    const gchar  *title,
                    const gchar  *prefix,
		    GFile        *file,
		    GFile       **cache_file,
		    GFile       **local_file)
{
	const gchar *space_checksum = "7215ee9c7d9dc229d2921a40e899ec5f";
	const gchar *a, *b;

	gchar *art_filename;
	gchar *dir, *filename;
	gchar *artist_down, *title_down;
	gchar *artist_stripped, *title_stripped;
	gchar *artist_norm, *title_norm;
	gchar *artist_checksum = NULL, *title_checksum = NULL;

	/* http://live.gnome.org/MediaArtStorageSpec */

	if (cache_file) {
		*cache_file = NULL;
	}

	if (local_file) {
		*local_file = NULL;
	}

	if (!artist && !title) {
		return;
	}

	if (artist) {
		artist_stripped = media_art_strip_invalid_entities (artist);
		artist_norm = g_utf8_normalize (artist_stripped, -1, G_NORMALIZE_NFKD);
		artist_down = g_utf8_strdown (artist_norm, -1);
		artist_checksum = media_art_checksum_for_data (G_CHECKSUM_MD5,
		                                               (const guchar *) artist_down,
		                                               strlen (artist_down));
	}

	if (title) {
		title_stripped = media_art_strip_invalid_entities (title);
		title_norm = g_utf8_normalize (title_stripped, -1, G_NORMALIZE_NFKD);
		title_down = g_utf8_strdown (title_norm, -1);
		title_checksum = media_art_checksum_for_data (G_CHECKSUM_MD5,
		                                              (const guchar *) title_down,
		                                              strlen (title_down));
	}

	dir = g_build_filename (g_get_user_cache_dir (),
	                        "media-art",
	                        NULL);

	if (!g_file_test (dir, G_FILE_TEST_EXISTS)) {
		g_mkdir_with_parents (dir, 0770);
	}

	if (artist) {
		a = artist_checksum;
		b = title ? title_checksum : space_checksum;
	} else {
		a = title_checksum;
		b = space_checksum;
	}

	art_filename = g_strdup_printf ("%s-%s-%s.jpeg", prefix ? prefix : "album", a, b);

	if (artist) {
		g_free (artist_checksum);
		g_free (artist_stripped);
		g_free (artist_down);
		g_free (artist_norm);
	}

	if (title) {
		g_free (title_checksum);
		g_free (title_stripped);
		g_free (title_down);
		g_free (title_norm);
	}

	if (cache_file) {
		filename = g_build_filename (dir, art_filename, NULL);
		*cache_file = g_file_new_for_path (filename);
		g_free (filename);
	}

	if (local_file) {
		GFile *parent;

		parent = g_file_get_parent (file);
		if (parent) {
			filename = g_build_filename (".mediaartlocal", art_filename, NULL);
			*local_file = g_file_resolve_relative_path (parent, filename);
			g_free (filename);

			g_object_unref (parent);
		}
	}

	g_free (dir);
	g_free (art_filename);
}

/**
 * media_art_get_path:
 * @artist: the artist
 * @title: the title
 * @prefix: the prefix, for example "album"
 * @uri: (allow-none): the uri of the file or %NULL
 * @path: (out) (transfer full) (allow-none): the location to store the local
 * path or %NULL
 * @local_uri: (out) (transfer full) (allow-none): the location to store the
 * local uri or %NULL
 *
 * Get the path to media art for a given resource. Newly allocated data in
 * @path and @local_uri must be freed with g_free().
 *
 * Since: 0.2.0
 */
void
media_art_get_path (const gchar  *artist,
                    const gchar  *title,
                    const gchar  *prefix,
                    const gchar  *uri,
                    gchar       **path,
                    gchar       **local_uri)
{
	GFile *file = NULL, *cache_file = NULL, *local_file = NULL;

	if (uri) {
		file = g_file_new_for_uri (uri);
	}

	media_art_get_file (artist, title, prefix, file,
			    (path) ? &cache_file : NULL,
			    (local_uri) ? &local_file : NULL);
	if (path) {
		*path = cache_file ? g_file_get_path (cache_file) : NULL;
	}

	if (local_uri) {
		*local_uri = local_file ? g_file_get_uri (local_file) : NULL;
	}

	if (file) {
		g_object_unref (file);
	}
}

static void
media_art_remove_foreach (gpointer data,
                          gpointer user_data)
{
	gchar *filename = data;
	gboolean total_success = * (gboolean *) user_data;
	gboolean success;

	success = g_unlink (filename) == 0;
	total_success &= success;

	if (!success) {
		g_warning ("Could not delete file '%s'", filename);
	}

	g_free (filename);
}

/**
 * media_art_remove:
 * @artist: artist the media art belongs to
 * @album: (allow-none): album the media art belongs or %NULL
 *
 * Removes media art for given album/artist/etc provided.
 *
 * Returns: #TRUE on success, otherwise #FALSE.
 *
 * Since: 0.2.0
 */
gboolean
media_art_remove (const gchar *artist,
                  const gchar *album)
{
	GError *error = NULL;
	GHashTable *table = NULL;
	const gchar *name;
	GDir *dir;
	gchar *dirname;
	GList *to_remove = NULL;
	gchar *target = NULL;
	gboolean success = TRUE;

	g_return_val_if_fail (artist != NULL && artist[0] != '\0', FALSE);

	dirname = g_build_filename (g_get_user_cache_dir (), "media-art", NULL);

	dir = g_dir_open (dirname, 0, &error);
	if (!dir || error) {
		/* Nothing to do if there is no directory in the first place. */
		g_debug ("Removing media-art for artist:'%s', album:'%s': directory could not be opened, %s",
		         artist, album, error ? error->message : "no error given");

		g_clear_error (&error);
		if (dir) {
			g_dir_close (dir);
		}
		g_free (dirname);

		/* We wanted to remove media art, so if there is no
		 * media art, the caller has achieved what they wanted.
		 */
		return TRUE;
	}

	table = g_hash_table_new_full (g_str_hash,
	                               g_str_equal,
	                               (GDestroyNotify) g_free,
	                               (GDestroyNotify) NULL);

	/* The get_path API does stripping itself */
	media_art_get_path (artist, album, "album", NULL, &target, NULL);
	if (target) {
		g_hash_table_replace (table, target, target);
	}

	/* Add the album path also (to which the symlinks are made) */
	media_art_get_path (NULL, album, "album", NULL, &target, NULL);
	if (target) {
		g_hash_table_replace (table, target, target);
	}

	/* Perhaps we should have an internal list of media art files that we made,
	 * instead of going over all the media art (which could also have been made
	 * by other softwares) */
	for (name = g_dir_read_name (dir); name != NULL; name = g_dir_read_name (dir)) {
		gpointer value;
		gchar *full;

		full = g_build_filename (dirname, name, NULL);
		value = g_hash_table_lookup (table, full);

		if (!value) {
			g_message ("Removing media-art for artist:'%s', album:'%s': deleting file '%s'",
			           artist, album, name);
			to_remove = g_list_prepend (to_remove, (gpointer) full);
		} else {
			g_free (full);
		}
	}

	g_list_foreach (to_remove, media_art_remove_foreach, &success);
	g_list_free (to_remove);

	g_hash_table_unref (table);

	g_dir_close (dir);
	g_free (dirname);

	return success;
}
