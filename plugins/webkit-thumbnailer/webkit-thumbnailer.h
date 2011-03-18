/* vi:set et ai sw=2 sts=2 ts=2: */
/*-
 * Copyright (c) 2009 Jannis Pohlmann <jannis@xfce.org>
 * Copyright (c) 2011 Jérôme Guelfucci <jeromeg@xfce.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General
 * Public License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifndef __WEBKIT_THUMBNAILER_H__
#define __WEBKIT_THUMBNAILER_H__

#include <glib-object.h>

G_BEGIN_DECLS;

#define TYPE_WEBKIT_THUMBNAILER            (webkit_thumbnailer_get_type ())
#define WEBKIT_THUMBNAILER(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), TYPE_WEBKIT_THUMBNAILER, WebkitThumbnailer))
#define WEBKIT_THUMBNAILER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), TYPE_WEBKIT_THUMBNAILER, WebkitThumbnailerClass))
#define IS_WEBKIT_THUMBNAILER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), TYPE_WEBKIT_THUMBNAILER))
#define IS_WEBKIT_THUMBNAILER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), TYPE_WEBKIT_THUMBNAILER)
#define WEBKIT_THUMBNAILER_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), TYPE_WEBKIT_THUMBNAILER, WebkitThumbnailerClass))

typedef struct _WebkitThumbnailerClass   WebkitThumbnailerClass;
typedef struct _WebkitThumbnailer        WebkitThumbnailer;

GType webkit_thumbnailer_get_type (void) G_GNUC_CONST;
void  webkit_thumbnailer_register (TumblerProviderPlugin *plugin);

G_END_DECLS;

#endif /* !__WEBKIT_THUMBNAILER_H__ */