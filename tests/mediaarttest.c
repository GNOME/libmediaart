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
        // FIXME: Decide what is the expected behaviour here...
        //   a. Return NULL
        //   b. Return ""
        //g_assert (!mediaart_strip_invalid_entities (NULL));
}

typedef struct {
        const gchar *prefix;
        const gchar *artist;
        const gchar *title;
        const gchar *filename;
} MediaArtTestCase;

MediaArtTestCase mediaart_test_cases [] = {
        { "album", "Beatles", "Sgt. Pepper",
          "album-2a9ea35253dbec60e76166ec8420fbda-cfba4326a32b44b8760b3a2fc827a634.jpeg"},

        { "album", "", "sgt. pepper",
          "album-d41d8cd98f00b204e9800998ecf8427e-cfba4326a32b44b8760b3a2fc827a634.jpeg"},

        { "album", " ", "sgt. pepper",
          "album-d41d8cd98f00b204e9800998ecf8427e-cfba4326a32b44b8760b3a2fc827a634.jpeg"},

        { "album", NULL, "sgt. pepper",
          "album-7215ee9c7d9dc229d2921a40e899ec5f-cfba4326a32b44b8760b3a2fc827a634.jpeg"},

        { "album", "Beatles", NULL,
          "album-2a9ea35253dbec60e76166ec8420fbda-7215ee9c7d9dc229d2921a40e899ec5f.jpeg"},

        { "album", NULL, NULL, NULL },

        { "podcast", NULL, "Podcast example",
          "podcast-10ca71a13bbd1a2af179f6d5a4dea118-7215ee9c7d9dc229d2921a40e899ec5f.jpeg"},

        { "radio", NULL, "Radio Free Europe",
          "radio-79577732dda605d0f953f6479ff1f42e-7215ee9c7d9dc229d2921a40e899ec5f.jpeg"},

        { "radio", "Artist is ignored", NULL, NULL },

        { "x-video", NULL, "Test extension of spec",
          "x-video-51110ae14ce4bbeb68335366289acdd1-7215ee9c7d9dc229d2921a40e899ec5f.jpeg"},

        /* Currently we raise a warning to advise the user that we don't
         * support this part of the media art spec, which isn't tested here. */
        /*{ "track", "Ignored", "Ignored", NULL},*/
};

static void
test_mediaart_location (void)
{
        gchar *path = NULL, *local_uri = NULL;
        gchar *expected;
        gint i;

        for (i = 0; i < G_N_ELEMENTS(mediaart_test_cases); i++) {
                MediaArtTestCase *testcase = &mediaart_test_cases[i];

                media_art_get_path (testcase->artist,
                                    testcase->title,
                                    testcase->prefix,
                                    "file:///home/test/a.mp3",
                                    &path,
                                    &local_uri);

                if (testcase->filename == NULL) {
                    expected = NULL;
                } else {
                    expected = g_build_path (G_DIR_SEPARATOR_S,
                                             g_get_user_cache_dir (),
                                             "media-art",
                                             testcase->filename,
                                             NULL);
                }

                if (g_strcmp0(path, expected) != 0) {
                    g_error ("Incorrect path for prefix: '%s' artist: '%s' "
                             "title: '%s'. Expected %s, got %s",
                             testcase->prefix,
                             testcase->artist,
                             testcase->title,
                             expected,
                             path);
                }

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
                            mediaart_test_cases[0].title,
                            mediaart_test_cases[0].prefix,
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

gint
main (gint argc, gchar **argv)
{
        g_test_init (&argc, &argv, NULL);

        g_test_add_func ("/mediaart/stripping",
                         test_mediaart_stripping);
        g_test_add_func ("/mediaart/stripping_null",
                         test_mediaart_stripping_null);
        g_test_add_func ("/mediaart/location",
                         test_mediaart_location);
        g_test_add_func ("/mediaart/location_null",
                         test_mediaart_location_null);
        g_test_add_func ("/mediaart/location_path",
                         test_mediaart_location_path);

        return g_test_run ();
}
