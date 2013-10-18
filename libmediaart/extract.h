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

gboolean media_art_init     (void);
void     media_art_shutdown (void);

gboolean media_art_process  (const unsigned char  *buffer,
                             size_t                len,
                             const gchar          *mime,
                             MediaArtType          type,
                             const gchar          *artist,
                             const gchar          *title,
                             const gchar          *uri);

gboolean media_art_process_file (const guchar *buffer,
				 gsize         len,
				 const gchar  *mime,
				 MediaArtType  type,
				 const gchar  *artist,
				 const gchar  *title,
				 GFile        *file);

G_END_DECLS

#endif /* __LIBMEDIAART_UTILS_H__ */
