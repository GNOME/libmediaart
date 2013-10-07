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

#ifndef __LIBMEDIAART_CACHE_H__
#define __LIBMEDIAART_CACHE_H__

#include <glib.h>
#include <gio/gio.h>

#if !defined (__LIBMEDIAART_INSIDE__) && !defined (LIBMEDIAART_COMPILATION)
#error "Only <libmediaart/mediaart.h> must be included directly."
#endif

G_BEGIN_DECLS

gchar *  media_art_strip_invalid_entities (const gchar          *original);

void     media_art_get_path               (const gchar          *artist,
                                           const gchar          *title,
                                           const gchar          *prefix,
                                           const gchar          *uri,
                                           gchar               **path,
                                           gchar               **local_uri);

void     media_art_get_file               (const gchar          *artist,
                                           const gchar          *title,
                                           const gchar          *prefix,
                                           GFile                *file,
                                           GFile               **cache_file,
                                           GFile               **local_file);

gboolean media_art_remove                 (const gchar          *artist,
                                           const gchar          *album);

G_END_DECLS

#endif /* __LIBMEDIAART_CACHE_H__ */
