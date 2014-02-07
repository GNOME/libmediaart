/*
 * Copyright (C) 2011, Nokia <ivan.frade@nokia.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 */

#include "config.h"

#include <stdio.h>
#include <stdlib.h>

#include <glib-object.h>

#include <libmediaart/mediaart.h>

struct {
        const gchar *input;
        const gchar *expected_output;
} strip_test_cases [] = {
        { "nothing to strip here", "nothing to strip here" },
        { "Upper Case gOEs dOwN", "upper case goes down"},
        { "o", "o"},
        { "A", "a"},
        { "cool album (CD1)", "cool album"},
        { "cool album [CD1]", "cool album"},
        { "cool album {CD1}", "cool album"},
        { "cool album <CD1>", "cool album"},
        { " ", ""},
        { "     a     ", "a"},
        { "messy #title & stuff?", "messy title stuff"},
        { "Unbalanced [brackets", "unbalanced brackets" },
        { "Unbalanced (brackets", "unbalanced brackets" },
        { "Unbalanced <brackets", "unbalanced brackets" },
        { "Unbalanced brackets)", "unbalanced brackets" },
        { "Unbalanced brackets]", "unbalanced brackets" },
        { "Unbalanced brackets>", "unbalanced brackets" },
        { "Live at *WEMBLEY* dude!", "live at wembley dude" },
        { "met[xX[x]alli]ca", "metallica" },
        { NULL, NULL}
};

struct {
        const gchar *artist;
        const gchar *album;
        const gchar *filename;
} mediaart_test_cases [] = {
        {"Beatles", "Sgt. Pepper",
         "album-2a9ea35253dbec60e76166ec8420fbda-cfba4326a32b44b8760b3a2fc827a634.jpeg"},

        { "", "sgt. pepper",
          "album-d41d8cd98f00b204e9800998ecf8427e-cfba4326a32b44b8760b3a2fc827a634.jpeg"},

        { " ", "sgt. pepper",
          "album-d41d8cd98f00b204e9800998ecf8427e-cfba4326a32b44b8760b3a2fc827a634.jpeg"},

        { NULL, "sgt. pepper",
          "album-cfba4326a32b44b8760b3a2fc827a634-7215ee9c7d9dc229d2921a40e899ec5f.jpeg"}, 

        { "Beatles", NULL,
          "album-2a9ea35253dbec60e76166ec8420fbda-7215ee9c7d9dc229d2921a40e899ec5f.jpeg"},

        { NULL, NULL, NULL }
};

static void
test_mediaart_init (void)
{
	g_assert_true (media_art_init ());
}

static void
test_mediaart_stripping (void)
{
        gint i;
        gchar *result;

        for (i = 0; strip_test_cases[i].input != NULL; i++) {
                result = media_art_strip_invalid_entities (strip_test_cases[i].input);
                g_assert_cmpstr (result, ==, strip_test_cases[i].expected_output);
                g_free (result);
        }

        g_print ("(%d test cases) ", i);
}

static void
test_mediaart_stripping_null (void)
{
}

static void
test_mediaart_stripping_failures_subprocess (void)
{
	g_assert (!media_art_strip_invalid_entities (NULL));
}

static void
test_mediaart_stripping_failures (void)
{
	gchar *stripped = NULL;

	/* a. Return NULL for NULL (subprocess)
	 * b. Return NULL for ""
	 */
        stripped = media_art_strip_invalid_entities ("");
        g_assert (stripped);
        g_assert_cmpstr (stripped, ==, "");

	g_test_trap_subprocess ("/mediaart/stripping_failures/subprocess", 0, 0);
	g_test_trap_assert_failed ();
	g_test_trap_assert_stderr ("*assertion 'original != NULL' failed*");
}

static void
test_mediaart_location (void)
{
        gchar *path = NULL, *local_uri = NULL;
        gchar *expected;
        gint i;
     
        for (i = 0; mediaart_test_cases[i].filename != NULL; i++) {
                media_art_get_path (mediaart_test_cases[i].artist,
                                    mediaart_test_cases[i].album,
                                    "album",
                                    "file:///home/test/a.mp3",
                                    &path,
                                    &local_uri);
                expected = g_build_path (G_DIR_SEPARATOR_S,
                                         g_get_user_cache_dir (),
                                         "media-art",
                                         mediaart_test_cases[i].filename, 
                                         NULL);
                g_assert_cmpstr (path, ==, expected);
                
                g_free (expected);
                g_free (path);
                g_free (local_uri);
        }
        g_print ("(%d test cases) ", i);


}

static void
test_mediaart_location_null (void)
{
        gchar *path = NULL, *local_uri = NULL;

        /* NULL parameters */
        media_art_get_path (NULL, NULL, "album", "file:///a/b/c.mp3", &path, &local_uri);
        g_assert (!path && !local_uri);
}

static void
test_mediaart_location_path (void)
{
        gchar *path = NULL, *local_uri = NULL;
        gchar *expected;

        /* Use path instead of URI */
        media_art_get_path (mediaart_test_cases[0].artist,
                            mediaart_test_cases[0].album,
                            "album",
                            "/home/test/a.mp3",
                            &path,
                            &local_uri);
        expected = g_build_path (G_DIR_SEPARATOR_S, 
                                 g_get_user_cache_dir (),
                                 "media-art",
                                 mediaart_test_cases[0].filename, 
                                 NULL);
        g_assert_cmpstr (path, ==, expected);
                
        g_free (expected);
        g_free (path);
        g_free (local_uri);
}

static void
test_mediaart_embedded_mp3 (void)
{
	GError *error = NULL;
	GFile *file = NULL;
	gchar *dir, *path;
	gboolean retval;

	/* FIXME: Handle 'buffer' AND 'file/path', is broken currently */

	dir = g_get_current_dir ();
	path = g_build_filename (G_DIR_SEPARATOR_S, dir, "543249_King-Kilo---Radium.mp3", NULL);
	file = g_file_new_for_path (path);
	g_free (path);

	retval = media_art_process_file (NULL,
	                                 0,
	                                 "audio/mp3", /* mime */
	                                 MEDIA_ART_ALBUM,
	                                 "King Kilo", /* artist */
	                                 "Lanedo", /* title */
	                                 file);

	g_assert_true (retval);

	g_object_unref (file);
	g_free (dir);
}

static void
test_mediaart_png (void)
{
	GError *error = NULL;
	GFile *file = NULL;
	gchar *dir, *path;
	gchar *out_path = NULL;
	gchar *out_uri = NULL;
	gchar *expected;
	gboolean retval;

	dir = g_get_current_dir ();
	path = g_build_filename (G_DIR_SEPARATOR_S, dir, "LanedoIconHKS43-64².png", NULL);
	file = g_file_new_for_path (path);
	g_free (dir);

	/* Check data is not cached currently */
	media_art_get_path ("Lanedo", /* artist / title */
	                    NULL, /* album */
	                    NULL, /* prefix */
                            path,
                            &out_path,
                            &out_uri);
	g_assert (g_file_test (out_path, G_FILE_TEST_EXISTS) == FALSE);
	g_free (out_path);
	g_free (out_uri);

	/* Process data */
	retval = media_art_process_file (NULL,
	                                 0,
	                                 "image/png", /* mime */
	                                 MEDIA_ART_ALBUM,
	                                 NULL, /* album */
	                                 "Lanedo", /* title */
	                                 file);

	g_assert_true (retval);

	/* Check cache exists */
	media_art_get_path ("Lanedo", /* artist / title */
	                    NULL, /* album */
	                    NULL, /* prefix */
                            path,
                            &out_path,
                            &out_uri);

        expected = g_build_path (G_DIR_SEPARATOR_S,
                                 g_get_user_cache_dir (),
                                 "media-art",
                                 "album-be60c84852d9762b0a896ba9ba24245e-7215ee9c7d9dc229d2921a40e899ec5f.jpeg",
                                 NULL);
        g_assert_cmpstr (out_path, ==, expected);
        /* FIXME: Why is out_uri NULL? */
        /* FIXME: Why does this next test fail - i.e. file does not
         * exist if we've processed it?
         */
        /* g_assert (g_file_test (out_path, G_FILE_TEST_EXISTS) == TRUE); */

        g_free (out_path);
        g_free (out_uri);
        g_free (expected);

        /* Remove album art */
        retval = media_art_remove ("Lanedo", "");
        g_assert_true (retval);

        retval = media_art_remove ("Lanedo", NULL);

        g_object_unref (file);
        g_free (path);
}

int
main (int argc, char **argv)
{
	int success = EXIT_SUCCESS;

        g_test_init (&argc, &argv, NULL);

        g_test_add_func ("/mediaart/init",
                         test_mediaart_init);
        g_test_add_func ("/mediaart/stripping",
                         test_mediaart_stripping);
        g_test_add_func ("/mediaart/stripping_failures",
                         test_mediaart_stripping_failures);
        g_test_add_func ("/mediaart/stripping_failures/subprocess",
                         test_mediaart_stripping_failures_subprocess);
        g_test_add_func ("/mediaart/location",
                         test_mediaart_location);
        g_test_add_func ("/mediaart/location_null",
                         test_mediaart_location_null);
        g_test_add_func ("/mediaart/location_path",
                         test_mediaart_location_path);
        g_test_add_func ("/mediaart/embedded_mp3",
                         test_mediaart_embedded_mp3);
        g_test_add_func ("/mediaart/png",
                         test_mediaart_png);

        success = g_test_run ();

        media_art_shutdown ();

        return success;
}
