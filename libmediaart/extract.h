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

#ifndef __LIBMEDIAART_EXTRACT_H__
#define __LIBMEDIAART_EXTRACT_H__

#include <glib.h>
#include <gio/gio.h>

#if !defined (__LIBMEDIAART_INSIDE__) && !defined (LIBMEDIAART_COMPILATION)
#error "Only <libmediaart/mediaart.h> must be included directly."
#endif

G_BEGIN_DECLS

/**
 * MediaArtType:
 * @MEDIA_ART_NONE: No media art is available
 * @MEDIA_ART_ALBUM: Media art is an album
 * @MEDIA_ART_VIDEO: Media art is a movie or video
 *
 * This type categorized the type of media art we're dealing with.
 */
typedef enum {
	MEDIA_ART_NONE,
	MEDIA_ART_ALBUM,
	MEDIA_ART_VIDEO,

	/*< private >*/
	MEDIA_ART_TYPE_COUNT
} MediaArtType;


/**
 * MediaArtError:
 * @MEDIA_ART_ERROR_NOENT: The resource that the was passed (for example a
 * file or URI) does not exist.
 * @MEDIA_ART_ERROR_NO_STORAGE: Storage information is unknown, we
 * have no knowledge about removable media.
 *
 * Enumeration values used in errors returned by the
 * #MediaArtError API.
 *
 * Since: 0.2
 **/
typedef enum {
	MEDIA_ART_ERROR_NOENT,
	MEDIA_ART_ERROR_NO_STORAGE
} MediaArtError;

#define MEDIA_ART_ERROR media_art_error_quark ()

GQuark media_art_error_quark (void) G_GNUC_CONST;


#define MEDIA_ART_TYPE_PROCESS         (media_art_process_get_type())
#define MEDIA_ART_PROCESS(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), MEDIA_ART_TYPE_PROCESS, MediaArtProcess))
#define MEDIA_ART_PROCESS_CLASS(c)     (G_TYPE_CHECK_CLASS_CAST ((c), MEDIA_ART_TYPE_PROCESS, MediaArtProcessClass))
#define MEDIA_ART_IS_PROCESS(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), MEDIA_ART_TYPE_PROCESS))
#define MEDIA_ART_IS_PROCESS_CLASS(c) (G_TYPE_CHECK_CLASS_TYPE ((c),  MEDIA_ART_TYPE_PROCESS))
#define MEDIA_ART_PROCESS_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), MEDIA_ART_TYPE_PROCESS, MediaArtProcessClass))

typedef struct _MediaArtProcess MediaArtProcess;
typedef struct _MediaArtProcessClass MediaArtProcessClass;

/**
 * MediaArtProcess:
 * @parent: parent object
 *
 * A class implementation for processing and extracting media art.
 **/
struct _MediaArtProcess {
	GObject parent;
};

/**
 * MediaArtProcessClass:
 * @parent: parent object class
 *
 * Prototype for the class.
 **/
struct _MediaArtProcessClass {
	GObjectClass parent;
};

GType    media_art_process_type (void) G_GNUC_CONST;
MediaArtProcess *
         media_art_process_new  (GError              **error);

gboolean media_art_process_uri  (MediaArtProcess      *process,
                                 const gchar          *uri,
                                 const unsigned char  *buffer,
                                 size_t                len,
                                 const gchar          *mime,
                                 MediaArtType          type,
                                 const gchar          *artist,
                                 const gchar          *title,
                                 GError              **error);

gboolean media_art_process_file (MediaArtProcess      *process,
                                 GFile                *file,
                                 const guchar         *buffer,
				 gsize                 len,
				 const gchar          *mime,
				 MediaArtType          type,
				 const gchar          *artist,
                                 const gchar          *title,
                                 GError              **error);

G_END_DECLS

#endif /* __LIBMEDIAART_UTILS_H__ */
