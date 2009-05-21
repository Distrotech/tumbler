/* vi:set et ai sw=2 sts=2 ts=2: */
/*-
 * Copyright (c) 2009 Jannis Pohlmann <jannis@xfce.org>
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib/gi18n.h>
#include <glib-object.h>

#include <tumbler/tumbler-enum-types.h>



GType
tumbler_thumbnail_flavor_get_type (void)
{
  GType type = G_TYPE_INVALID;

  if (G_UNLIKELY (type == G_TYPE_INVALID))
    {
      static const GEnumValue values[] = 
      {
        { TUMBLER_THUMBNAIL_FLAVOR_INVALID, "TUMBLER_THUMBNAIL_FLAVOR_INVALID",  N_ ("Invalid"), },
        { TUMBLER_THUMBNAIL_FLAVOR_NORMAL,  "TUMBLER_THUMBNAIL_FLAVOR_NORMAL",   N_ ("Normal"),  },
        { TUMBLER_THUMBNAIL_FLAVOR_LARGE,   "TUMBLER_THUMBNAIL_FLAVOR_LARGE",    N_ ("Large"),   },
        { TUMBLER_THUMBNAIL_FLAVOR_CROPPED, "TUMBLER_THUMBNAIL_FLAVOR_CROPPED",  N_ ("Cropped"), },
        { 0,                                NULL,                                NULL,           },
      };

      type = g_enum_register_static ("TumblerThumbnailFlavor", values);
    }

  return type;
}
