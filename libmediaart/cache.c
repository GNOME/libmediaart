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

#include <glib.h>
#include <glib/gstdio.h>

#include <libtracker-sparql/tracker-sparql.h>

#include "utils.h"

/**
 * SECTION:media-art
 * @title: Media art management
 * @short_description: Media art request and management.
 * @include: libmediaart/mediaart.h
 *
 * This is a convenience API using D-Bus to talk to the media management service.
 **/

/**
 * media_art_remove():
 * @artist: Artist the media art belongs to
 * @album: Album the media art belongs to (optional)
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
	gchar *album_path = NULL;

	g_return_if_fail (artist != NULL && artist[0] != '\0');

	dirname = g_build_filename (g_get_user_cache_dir (), "media-art", NULL);

	if (!g_file_test (dirname, G_FILE_TEST_EXISTS)) {
		/* Ignore this and just quit the function */
		g_debug ("Nothing to do, media-art cache directory '%s' doesn't exist", dirname);
		g_free (dirname);
		return TRUE;
	}

	dir = g_dir_open (dirname, 0, &error);

	if (error) {
		g_critical ("Call to g_dir_open() failed, %s", error->message);

		g_error_free (error);
		g_free (dirname);

		if (dir) {
			g_dir_close (dir);
		}

		return FALSE;
	}

	table = g_hash_table_new_full (g_str_hash,
	                               g_str_equal,
	                               (GDestroyNotify) g_free,
	                               (GDestroyNotify) NULL);

	/* The get_path API does stripping itself */
	media_art_get_path (artist, album, "album", NULL, &target, NULL);
	g_hash_table_replace (table, target, target);

	/* Also add the file to which the symlinks are made */
	media_art_get_path (NULL, album, "album", NULL, &album_path, NULL);
	g_hash_table_replace (table, album_path, album_path);

	/* Perhaps we should have an internal list of media art files that we made,
	 * instead of going over all the media art (which could also have been made
	 * by other softwares) */
	for (name = g_dir_read_name (dir); name != NULL; name = g_dir_read_name (dir)) {
		gpointer value;
		gchar *full;

		full = g_build_filename (dirname, name, NULL);

		value = g_hash_table_lookup (table, full);

		if (!value) {
			g_message ("Removing media-art file '%s': no album exists with songs for this media-art cache", name);
			to_remove = g_list_prepend (to_remove, (gpointer) full);
		} else {
			g_free (full);
		}
	}

	if (dir) {
		g_dir_close (dir);
	}

	g_free (dirname);

	g_list_foreach (to_remove, (GFunc) g_unlink, NULL);
	g_list_foreach (to_remove, (GFunc) g_free, NULL);
	g_list_free (to_remove);

	if (table) {
		g_hash_table_unref (table);
	}

	return TRUE;
}
