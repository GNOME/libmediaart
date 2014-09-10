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

typedef struct {
	const gchar *test_name;
	const gchar *input1;
	const gchar *input2;
	const gchar *expected;
} TestInfo;

static TestInfo strip_test_cases [] = {
	{ "nothing-to-strip", "nothing to strip here", NULL, "nothing to strip here" },
        { "case-strip", "Upper Case gOEs dOwN", NULL, "upper case goes down" },
	{ "single-char", "o", NULL, "o" },
        { "single-char-case", "A", NULL, "a" },
        { "remove-parenthesis-round", "cool album (CD1)", NULL, "cool album" },
	{ "remove-parenthesis-square", "cool album [CD1]", NULL, "cool album" },
        { "remove-parenthesis-squirly", "cool album {CD1}", NULL, "cool album" },
        { "remove-parenthesis-gt-lt", "cool album <CD1>", NULL, "cool album" },
        { "whitespace", " ", NULL, "" },
        { "whitespace-with-content", "     a     ", NULL, "a" },
        { "messy-title", "messy #title & stuff?", NULL, "messy title stuff" },
	{ "unbalanced-brackets-square-start", "Unbalanced [brackets", NULL, "unbalanced brackets" },
        { "unbalanced-brackets-round-start", "Unbalanced (brackets", NULL, "unbalanced brackets" },
        { "unbalanced-brackets-gt-lt-start", "Unbalanced <brackets", NULL, "unbalanced brackets" },
        { "unbalanced-brackets-round-end", "Unbalanced brackets)", NULL, "unbalanced brackets" },
        { "unbalanced-brackets-square-end", "Unbalanced brackets]", NULL, "unbalanced brackets" },
        { "unbalanced-brackets-gt-lt-end", "Unbalanced brackets>", NULL, "unbalanced brackets" },
        { "messy-title-punctuation", "Live at *WEMBLEY* dude!", NULL, "live at wembley dude" },
	{ "crap-brackets-everywhere", "met[xX[x]alli]ca", NULL, "metallica" },
	{ NULL, NULL, NULL, NULL }
};

static TestInfo location_test_cases [] = {
	{ "normal-case",
	  "Beatles", "Sgt. Pepper",
         "album-2a9ea35253dbec60e76166ec8420fbda-cfba4326a32b44b8760b3a2fc827a634.jpeg" },
	{ "empty-artist",
	  "", "sgt. pepper",
          "album-d41d8cd98f00b204e9800998ecf8427e-cfba4326a32b44b8760b3a2fc827a634.jpeg" },
	{ "whitespace-artist",
	  " ", "sgt. pepper",
          "album-d41d8cd98f00b204e9800998ecf8427e-cfba4326a32b44b8760b3a2fc827a634.jpeg" },
	{ "null-artist",
	  NULL, "sgt. pepper",
          "album-cfba4326a32b44b8760b3a2fc827a634-7215ee9c7d9dc229d2921a40e899ec5f.jpeg" },
	{ "null-title",
	  "Beatles", NULL,
          "album-2a9ea35253dbec60e76166ec8420fbda-7215ee9c7d9dc229d2921a40e899ec5f.jpeg" },
        { NULL, NULL, NULL, NULL }
};

static void
setup (TestInfo      *info,
       gconstpointer  context)
{
	TestInfo *this_test = (gpointer) context;
	if (this_test) {
		*info = *this_test;
	}
}

static void
teardown (TestInfo      *info,
          gconstpointer  context)
{
}

static void
test_mediaart_new (TestInfo      *test_info,
                   gconstpointer  context)
{
	MediaArtProcess *process;
	GError *error = NULL;

	/* Test: creation of object */
	process = media_art_process_new (&error);
	g_assert_no_error (error);
	g_assert (process != NULL);

	g_object_unref (process);
}

static void
test_mediaart_stripping (TestInfo      *test_info,
                         gconstpointer  context)
{
        gchar *result;

        result = media_art_strip_invalid_entities (test_info->input1);
        g_assert_cmpstr (result, ==, test_info->expected);
        g_free (result);
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
test_mediaart_location (TestInfo      *test_info,
                        gconstpointer  context)
{
        gchar *path = NULL, *local_uri = NULL;
        gchar *expected;

        media_art_get_path (test_info->input1,
                            test_info->input2,
                            "album",
                            "file:///home/test/a.mp3",
                            &path,
                            &local_uri);
        expected = g_build_path (G_DIR_SEPARATOR_S,
                                 g_get_user_cache_dir (),
                                 "media-art",
                                 test_info->expected,
                                 NULL);
        g_assert_cmpstr (path, ==, expected);
                
        g_free (expected);
        g_free (path);
        g_free (local_uri);
}

static void
test_mediaart_location_null (void)
{
        gchar *path = NULL, *local_uri = NULL;

        /* NULL parameters */
        media_art_get_path (NULL, "some-title", "album", "file:///a/b/c.mp3", &path, &local_uri);
        g_assert (path != NULL);
        g_assert (local_uri != NULL);

        media_art_get_path ("some-artist", NULL, "album", "file:///a/b/c.mp3", &path, &local_uri);
        g_assert (path != NULL);
        g_assert (local_uri != NULL);
}

static void
test_mediaart_location_path (void)
{
        gchar *path = NULL, *local_uri = NULL;
        gchar *expected;

        /* Use path instead of URI */
        media_art_get_path (location_test_cases[0].input1,
                            location_test_cases[0].input2,
                            "album",
                            "/home/test/a.mp3",
                            &path,
                            &local_uri);
        expected = g_build_path (G_DIR_SEPARATOR_S, 
                                 g_get_user_cache_dir (),
                                 "media-art",
                                 location_test_cases[0].expected,
                                 NULL);
        g_assert_cmpstr (path, ==, expected);
                
        g_free (expected);
        g_free (path);
        g_free (local_uri);
}

static void
test_mediaart_embedded_mp3 (void)
{
	MediaArtProcess *process;
	GError *error = NULL;
	GFile *file = NULL;
	gchar *path;
	gboolean retval;

	path = g_build_filename (G_DIR_SEPARATOR_S, TOP_SRCDIR, "tests", "543249_King-Kilo---Radium.mp3", NULL);
	file = g_file_new_for_path (path);
	g_free (path);

	process = media_art_process_new (&error);
	g_assert_no_error (error);

	retval = media_art_process_file (process,
	                                 MEDIA_ART_ALBUM,
	                                 MEDIA_ART_PROCESS_FLAGS_FORCE,
	                                 file,
	                                 "King Kilo", /* artist */
	                                 "Radium",    /* title */
	                                 &error);

	g_assert_no_error (error);
	g_assert_true (retval);

	g_object_unref (file);

	g_object_unref (process);
}

static void
test_mediaart_process_buffer (void)
{
	MediaArtProcess *process;
	GError *error = NULL;
	GFile *file = NULL;
	gchar *path;
	gchar *out_path = NULL;
	gchar *out_uri = NULL;
	gchar *expected;
	gboolean retval;

	path = g_build_filename (G_DIR_SEPARATOR_S, TOP_SRCDIR, "tests", "cover.png", NULL);
	file = g_file_new_for_path (path);

	process = media_art_process_new (&error);
	g_assert_no_error (error);

	/* Check data is not cached currently */
	media_art_get_path ("Lanedo", /* artist / title */
	                    NULL,     /* album */
	                    NULL,     /* prefix */
                            path,
                            &out_path,
                            &out_uri);
	g_assert (g_file_test (out_path, G_FILE_TEST_EXISTS) == FALSE);
	g_free (out_path);
	g_free (out_uri);

	/* Process data */
	retval = media_art_process_file (process,
	                                 MEDIA_ART_ALBUM,
	                                 MEDIA_ART_PROCESS_FLAGS_NONE,
	                                 file,
	                                 NULL,        /* album */
	                                 "Lanedo",    /* title */
	                                 &error);
	g_assert_no_error (error);
	g_assert_true (retval);

	/* Check cache exists */
	media_art_get_path ("Lanedo", /* artist / title */
	                    NULL,     /* album */
	                    NULL,     /* prefix */
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
        g_assert (g_file_test (out_path, G_FILE_TEST_EXISTS) == TRUE);

        g_free (out_path);
        g_free (out_uri);
        g_free (expected);

        /* Remove album art */
        retval = media_art_remove ("Lanedo", "");
        g_assert_true (retval);

        retval = media_art_remove ("Lanedo", NULL);

        g_object_unref (file);
        g_free (path);

	g_object_unref (process);
}

static void
test_mediaart_process_failures (void)
{
	MediaArtProcess *process;
	GError *error = NULL;

	g_test_trap_subprocess ("/mediaart/process_failures/subprocess", 0, 0 /*G_TEST_SUBPROCESS_INHERIT_STDOUT | G_TEST_SUBPROCESS_INHERIT_STDERR*/);
	g_test_trap_assert_failed ();
	g_test_trap_assert_stderr ("*assertion 'uri != NULL' failed*");

	process = media_art_process_new (&error);
	g_assert_no_error (error);

	/* Test: invalid file */
	g_assert (!media_art_process_uri (process,
	                                  MEDIA_ART_ALBUM,
	                                  MEDIA_ART_PROCESS_FLAGS_NONE,
	                                  "file:///invalid/path.png",
	                                  "Foo",       /* album */
	                                  "Bar",       /* title */
	                                  &error));
	
	g_assert_error (error, g_io_error_quark(), G_IO_ERROR_NOT_FOUND);
	g_clear_error (&error);

	/* Test: Invalid mime type */
	/* g_assert (!media_art_process_uri (process, */
	/*                                   "file:///invalid/path.png", */
	/*                                   NULL, */
	/*                                   0, */
	/*                                  "image/png", /\* mime *\/ */
	/*                                  MEDIA_ART_ALBUM, */
	/*                                  "Foo",       /\* album *\/ */
	/*                                  "Bar",       /\* title *\/ */
	/*                                  &error)); */

	/* g_message ("code:%d, domain:%d, error:'%s'\n", error->code, error->domain, error->message); */

	g_object_unref (process);
}

static void
test_mediaart_process_failures_subprocess (void)
{
	MediaArtProcess *process;
	GError *error = NULL;

	process = media_art_process_new (&error);
	g_assert_no_error (error);

	g_assert (!media_art_process_uri (process,
	                                  MEDIA_ART_ALBUM,
	                                  MEDIA_ART_PROCESS_FLAGS_NONE,
	                                  NULL,
	                                  "Foo",       /* album */
	                                  "Bar",       /* title */
	                                  &error));
	g_assert_no_error (error);

	g_object_unref (process);
}

int
main (int argc, char **argv)
{
	const gchar *cache_home_originally;
	const gchar *test_dir;
	gint success = EXIT_SUCCESS;
	gint i;

        g_test_init (&argc, &argv, NULL);

	test_dir = g_test_get_dir (G_TEST_BUILT);
	cache_home_originally = g_getenv ("XDG_CACHE_HOME");
	g_setenv ("XDG_CACHE_HOME", test_dir, TRUE);

        g_test_add ("/mediaart/new", TestInfo, NULL, setup, test_mediaart_new, teardown);

	for (i = 0; strip_test_cases[i].test_name; i++) {
		gchar *testpath;

		testpath = g_strconcat ("/mediaart/stripping/", strip_test_cases[i].test_name, NULL);
		g_test_add (testpath, TestInfo, &strip_test_cases[i], setup, test_mediaart_stripping, teardown);
		g_free (testpath);
	}

        g_test_add_func ("/mediaart/stripping_failures", test_mediaart_stripping_failures);
        g_test_add_func ("/mediaart/stripping_failures/subprocess", test_mediaart_stripping_failures_subprocess);

	for (i = 0; location_test_cases[i].test_name; i++) {
		gchar *testpath;

		testpath = g_strconcat ("/mediaart/location/", location_test_cases[i].test_name, NULL);
		g_test_add (testpath, TestInfo, &location_test_cases[i], setup, test_mediaart_location, teardown);
		g_free (testpath);
	}

        g_test_add_func ("/mediaart/location_null", test_mediaart_location_null);
        g_test_add_func ("/mediaart/location_path", test_mediaart_location_path);
        g_test_add_func ("/mediaart/embedded_mp3", test_mediaart_embedded_mp3);
        g_test_add_func ("/mediaart/process_buffer", test_mediaart_process_buffer);
        g_test_add_func ("/mediaart/process_failures", test_mediaart_process_failures);
        g_test_add_func ("/mediaart/process_failures/subprocess", test_mediaart_process_failures_subprocess);

        success = g_test_run ();

        if (cache_home_originally) {
	        g_setenv ("XDG_CACHE_HOME", cache_home_originally, TRUE);
        } else {
	        g_unsetenv ("XDG_CACHE_HOME");
        }

        return success;
}
