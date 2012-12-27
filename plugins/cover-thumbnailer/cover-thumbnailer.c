/* vi:set et ai sw=2 sts=2 ts=2: */
/*-
 * Copyright (c) 2012 Nick Schermer <nick@xfce.org>
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
 *
 * Parts are based on the gsf-office-thumbnailer in libgsf, which is
 * written by Federico Mena-Quintero <federico@novell.com>.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <math.h>

#include <glib.h>
#include <glib/gi18n.h>
#include <glib-object.h>

#include <gdk-pixbuf/gdk-pixbuf.h>
#include <tumbler/tumbler.h>
#include <curl/curl.h>

#include <cover-thumbnailer/cover-thumbnailer.h>



#define SERIES_PATTERN    "\\b((?:s\\d{1,2}e\\d{1,2}|\\d{1,2}x\\d{1,2}))\\b"
#define ABBREV_PATTERN    "\\b(\\w{1,}(?:rip|scr)|r5|hdtv|(?:480|720|1080)p|\\.(?:avi|mpe?g|mkv|ts))\\b"
#define YEAR_PATTERN      "\\b(\\d{4})\\b"

#define OMDBAPI_QUERY_URL "http://www.omdbapi.com/?t="

#define TMDB_BASE_URL     "http://cf2.imgobject.com/t/p/"
#define TMDB_API_KEY      "4be68d7eab1fbd1b6fd8a3b80a65a95e"
#define TMDB_QUERY_URL    "http://api.themoviedb.org/3/search/movie?api_key="



static void cover_thumbnailer_finalize     (GObject                    *object);
static void cover_thumbnailer_set_property (GObject                    *object,
                                            guint                       prop_id,
                                            const GValue               *value,
                                            GParamSpec                 *pspec);
static void cover_thumbnailer_create       (TumblerAbstractThumbnailer *thumbnailer,
                                            GCancellable               *cancellable,
                                            TumblerFileInfo            *info);



struct _CoverThumbnailerClass
{
  TumblerAbstractThumbnailerClass __parent__;
};

struct _CoverThumbnailer
{
  TumblerAbstractThumbnailer __parent__;

  /* themoviedb api key */
  gchar  *api_key;

  /* whitelisted locations */
  GSList *locations;

  /* precompiled */
  GRegex *series_regex;
  GRegex *abbrev_regex;
  GRegex *year_regex;
};

enum
{
  PROP_0,
  PROP_LOCATIONS,
  PROP_API_KEY
};



G_DEFINE_DYNAMIC_TYPE (CoverThumbnailer,
                       cover_thumbnailer,
                       TUMBLER_TYPE_ABSTRACT_THUMBNAILER);



void
cover_thumbnailer_register (TumblerProviderPlugin *plugin)
{
  cover_thumbnailer_register_type (G_TYPE_MODULE (plugin));
}



static void
cover_thumbnailer_class_init (CoverThumbnailerClass *klass)
{
  TumblerAbstractThumbnailerClass *abstractthumbnailer_class;
  GObjectClass                    *gobject_class;

  abstractthumbnailer_class = TUMBLER_ABSTRACT_THUMBNAILER_CLASS (klass);
  abstractthumbnailer_class->create = cover_thumbnailer_create;

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = cover_thumbnailer_finalize;
  gobject_class->set_property = cover_thumbnailer_set_property;

  g_object_class_install_property (gobject_class,
                                   PROP_LOCATIONS,
                                   g_param_spec_boxed ("locations",
                                                       "locations",
                                                       "locations",
                                                       G_TYPE_STRV,
                                                       G_PARAM_CONSTRUCT_ONLY |
                                                       G_PARAM_WRITABLE));

  g_object_class_install_property (gobject_class,
                                   PROP_API_KEY,
                                   g_param_spec_string ("api-key",
                                                        "api-key",
                                                        "api-key",
                                                        NULL,
                                                        G_PARAM_CONSTRUCT_ONLY |
                                                        G_PARAM_WRITABLE));
}



static void
cover_thumbnailer_class_finalize (CoverThumbnailerClass *klass)
{
}



static void
cover_thumbnailer_init (CoverThumbnailer *thumbnailer)
{
  /* prepare the regular expressions */
  thumbnailer->series_regex = g_regex_new (SERIES_PATTERN, G_REGEX_CASELESS, 0, NULL);
  thumbnailer->abbrev_regex = g_regex_new (ABBREV_PATTERN, G_REGEX_CASELESS, 0, NULL);
  thumbnailer->year_regex = g_regex_new (YEAR_PATTERN, 0, 0, NULL);
}



static void
cover_thumbnailer_set_property (GObject      *object,
                                guint         prop_id,
                                const GValue *value,
                                GParamSpec   *pspec)
{
  CoverThumbnailer     *cover = COVER_THUMBNAILER (object);
  guint                i;
  const gchar * const *locations;
  GFile               *gfile;

  switch (prop_id)
    {
    case PROP_API_KEY:
      cover->api_key = g_value_dup_string (value);
      break;

    case PROP_LOCATIONS:
      locations = g_value_get_boxed (value);
      g_assert (locations != NULL);
      for (i = 0; locations[i] != NULL; i++)
        {
          gfile = g_file_new_for_commandline_arg (locations[i]);
          cover->locations = g_slist_prepend (cover->locations, gfile);
        }
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static void
cover_thumbnailer_finalize (GObject *object)
{
  CoverThumbnailer *cover = COVER_THUMBNAILER (object);

  /* cleanup */
  g_regex_unref (cover->series_regex);
  g_regex_unref (cover->abbrev_regex);
  g_regex_unref (cover->year_regex);

  g_free (cover->api_key);

  g_slist_foreach (cover->locations, (GFunc) g_object_unref, NULL);
  g_slist_free (cover->locations);

  (*G_OBJECT_CLASS (cover_thumbnailer_parent_class)->finalize) (object);
}



static void
cover_thumbnailer_size_prepared (GdkPixbufLoader        *loader,
                                 gint                    source_width,
                                 gint                    source_height,
                                 TumblerThumbnailFlavor *flavor)
{

  gint    dest_width;
  gint    dest_height;
  gdouble hratio;
  gdouble wratio;

  g_return_if_fail (GDK_IS_PIXBUF_LOADER (loader));
  g_return_if_fail (TUMBLER_IS_THUMBNAIL_FLAVOR (flavor));

  /* get the destination size */
  tumbler_thumbnail_flavor_get_size (flavor, &dest_width, &dest_height);

  if (source_width <= dest_width && source_height <= dest_height)
    {
      /* do not scale the image */
      dest_width = source_width;
      dest_height = source_height;
    }
  else
    {
      /* determine which axis needs to be scaled down more */
      wratio = (gdouble) source_width / (gdouble) dest_width;
      hratio = (gdouble) source_height / (gdouble) dest_height;

      /* adjust the other axis */
      if (hratio > wratio)
        dest_width = rint (source_width / hratio);
     else
        dest_height = rint (source_height / wratio);
    }

  gdk_pixbuf_loader_set_size (loader, MAX (dest_width, 1), MAX (dest_height, 1));
}



static size_t
cover_thumbnailer_load_pixbuf_write (gpointer data,
                                     size_t   size,
                                     size_t   nmemb,
                                     gpointer user_data)
{
  GdkPixbufLoader *loader = GDK_PIXBUF_LOADER (user_data);
  size_t           len = size * nmemb;
  GError          *err = NULL;

  g_return_val_if_fail (GDK_IS_PIXBUF_LOADER (loader), 0);

  /* write to the loader */
  if (!gdk_pixbuf_loader_write (loader, data, len, &err))
    {
      g_critical ("Failed to write to pixbuf loader: %s", err->message);
      g_error_free (err);
    }

  return len;
}



static size_t
cover_thumbnailer_load_contents_write (gpointer data,
                                       size_t   size,
                                       size_t   nmemb,
                                       gpointer user_data)
{
  GString *contents = user_data;
  size_t   len = size * nmemb;

  g_string_append_len (contents, data, len);

  return len;
}



static gint
cover_thumbnailer_check_progress (gpointer user_data,
                                  gdouble  dltotal,
                                  gdouble  dlnow,
                                  gdouble  ultotal,
                                  gdouble  ulnow)
{
  GCancellable *cancellable = G_CANCELLABLE (user_data);

  g_return_val_if_fail (G_IS_CANCELLABLE (cancellable), 1);

  /* return 1 to stop the download, 0 continue */
  return g_cancellable_is_cancelled (cancellable);
}



static CURL *
cover_thumbnailer_load_prepare (const gchar  *url,
                                GCancellable *cancellable)
{
  CURL *curl_handle;

  g_return_val_if_fail (g_str_has_prefix (url, "http://"), NULL);
  g_return_val_if_fail (G_IS_CANCELLABLE (cancellable), NULL);

  /* curl downloader */
  curl_handle = curl_easy_init ();
  curl_easy_setopt (curl_handle, CURLOPT_URL, url);
  curl_easy_setopt (curl_handle, CURLOPT_USERAGENT, PACKAGE_NAME "/" PACKAGE_VERSION);

  /* cancellable check */
  curl_easy_setopt (curl_handle, CURLOPT_PROGRESSFUNCTION, cover_thumbnailer_check_progress);
  curl_easy_setopt (curl_handle, CURLOPT_PROGRESSDATA, cancellable);
  curl_easy_setopt (curl_handle, CURLOPT_NOPROGRESS, FALSE);

  return curl_handle;
}



static GdkPixbuf *
cover_thumbnailer_load_pixbuf (const gchar             *url,
                               TumblerThumbnailFlavor  *flavor,
                               GCancellable            *cancellable,
                               GError                 **error)
{
  CURL            *curl_handle;
  GdkPixbufLoader *loader;
  GdkPixbuf       *pixbuf = NULL;
  gint             ret;

  g_return_val_if_fail (url != NULL, NULL);
  g_return_val_if_fail (G_IS_CANCELLABLE (cancellable), NULL);
  g_return_val_if_fail (error == NULL || *error == NULL, NULL);
  g_return_val_if_fail (TUMBLER_IS_THUMBNAIL_FLAVOR (flavor), NULL);

  if (g_cancellable_is_cancelled (cancellable))
    return NULL;

  /* create a pixbuf loader */
  loader = gdk_pixbuf_loader_new ();
  g_signal_connect (loader, "size-prepared",
                    G_CALLBACK (cover_thumbnailer_size_prepared), flavor);

  /* download the image into a pixbuf loader */
  curl_handle = cover_thumbnailer_load_prepare (url, cancellable);
  curl_easy_setopt (curl_handle, CURLOPT_WRITEFUNCTION, cover_thumbnailer_load_pixbuf_write);
  curl_easy_setopt (curl_handle, CURLOPT_WRITEDATA, loader);
  ret = curl_easy_perform (curl_handle);
  curl_easy_cleanup (curl_handle);

  /* check if everything went fine */
  if (gdk_pixbuf_loader_close (loader, error)
      && ret == 0
      && !g_cancellable_is_cancelled (cancellable))
    {
      /* take the pixbuf */
      pixbuf = gdk_pixbuf_loader_get_pixbuf (loader);
      if (G_LIKELY (pixbuf != NULL))
        g_object_ref (pixbuf);
    }
  else if (ret != 0 && error == NULL)
    {
      /* curl error */
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                   _("Failed to load the poster image \"%s\""), url);
    }

  g_object_unref (loader);

  return pixbuf;
}



static gchar *
cover_thumbnailer_load_contents (const gchar   *uri,
                                 GCancellable  *cancellable,
                                 GError       **error)
{
  GString *contents;
  CURL    *curl_handle;
  gint     ret;

  if (g_cancellable_is_cancelled (cancellable))
    return NULL;

  /* prepare buffer */
  contents = g_string_new (NULL);

  /* download metadata */
  curl_handle = cover_thumbnailer_load_prepare (uri, cancellable);
  curl_easy_setopt (curl_handle, CURLOPT_WRITEFUNCTION, cover_thumbnailer_load_contents_write);
  curl_easy_setopt (curl_handle, CURLOPT_WRITEDATA, contents);
  ret = curl_easy_perform (curl_handle);
  curl_easy_cleanup (curl_handle);

  if (ret != 0)
    {
      /* curl error */
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                   _("Failed to load the metadata from \"%s\""), uri);
    }

  return g_string_free (contents, ret != 0);
}



static gboolean
cover_thumbnailer_get_title (CoverThumbnailer  *cover,
                             GFile            *gfile,
                             gchar           **ret_title,
                             gchar           **ret_year)
{
  gchar       *basename;
  gboolean     is_series;
  GMatchInfo  *match_info;
  gint         start_pos;
  gint         end_pos;
  gchar       *year = NULL;
  GString     *title;
  const gchar *p;
  gboolean     append_space;
  gunichar     uchar;
  gboolean     succeed;
  gchar       *temp;

  g_return_val_if_fail (G_IS_FILE (gfile), FALSE);
  g_return_val_if_fail (ret_title != NULL, FALSE);
  g_return_val_if_fail (ret_year != NULL, FALSE);

  /* get the basename */
  basename = g_file_get_basename (gfile);

  /* check if the title looks like a serie */
  is_series = g_regex_match (cover->series_regex, basename, 0, &match_info);

  /* if this is not a serie, look for other filename crap */
  if (is_series
      || g_regex_match (cover->abbrev_regex, basename, 0, &match_info))
    {
      /* remove series or abbrev suffix from the filename */
      if (g_match_info_fetch_pos (match_info, 0, &start_pos, NULL)
          && start_pos > 0)
        basename[start_pos] = '\0';
      g_match_info_free (match_info);

      if (!is_series)
        {
          /* for non-series, look for a year in the title */
          if (g_regex_match (cover->year_regex, basename, 0, &match_info))
            {
              /* store year and remove the suffix from the title */
              if (g_match_info_fetch_pos (match_info, 0, &start_pos, &end_pos)
                  && start_pos >= 0
                  && end_pos > start_pos)
                {
                  year = g_strndup (basename + start_pos, end_pos - start_pos);

                  if (start_pos == 0)
                    {
                      temp = g_strdup (basename + end_pos);
                      g_free (basename);
                      basename = temp;
                    }
                  else
                    {
                      basename[start_pos] = '\0';
                    }
                }
              g_match_info_free (match_info);
            }
        }
    }

  /* append the possible title part of the filename */
  title = g_string_sized_new (strlen (basename));
  for (p = basename, append_space = FALSE; *p != '\0'; p = g_utf8_next_char (p))
    {
      uchar = g_utf8_get_char (p);
      if (g_unichar_isalnum (uchar)
          || uchar == '\'')
        {
          if (append_space)
            {
              g_string_append_c (title, '+');
              append_space = FALSE;
            }

          /* append the char */
          g_string_append_unichar (title, uchar);
        }
      else if (title->len > 0)
        {
          /* start with a space next time we append a char */
          append_space = TRUE;
        }
    }

  /* finalize */
  g_free (basename);
  succeed = title->len > 1;
  *ret_title = g_string_free (title, !succeed);
  *ret_year = year;

  return succeed;
}



static gchar *
cover_thumbnailer_poster_url (CoverThumbnailer        *cover,
                              const gchar             *title,
                              const gchar             *year,
                              TumblerThumbnailFlavor  *flavor,
                              GCancellable            *cancellable,
                              GError                 **error)
{
  gchar       *query;
  const gchar *needle;
  const gchar *p;
  const gchar *k = NULL;
  gchar       *url_part;
  gchar       *url = NULL;
  gchar       *data;
  gint         dest_width;

  g_return_val_if_fail (TUMBLER_IS_THUMBNAIL_FLAVOR (flavor), NULL);
  g_return_val_if_fail (IS_COVER_THUMBNAILER (cover), NULL);

  if (G_LIKELY (cover->api_key == NULL))
    {
      needle = "\"Poster\":\"http://";
      query = g_strconcat (OMDBAPI_QUERY_URL, title,
                           year != NULL ? "&y=" : NULL, year,
                           NULL);
    }
  else
    {
      needle = "\"poster_path\":\"/";
      query = g_strconcat (TMDB_QUERY_URL, cover->api_key,
                           "&query=", title,
                           year != NULL ? "&year=" : NULL, year,
                           NULL);
    }

  data = cover_thumbnailer_load_contents (query, cancellable, error);
  g_free (query);

  if (data != NULL)
    {
      p = strstr (data, needle);
      if (p != NULL)
        {
          p += strlen (needle);
          k = strstr (p, ".jpg\"");
        }

      if (p != NULL && k != NULL)
        {
          /* extract poster data from the contents and build a working uri */
          url_part = g_strndup (p, k - p);
          if (cover->api_key == NULL)
            {
              /* imdb image location */
              url = g_strconcat ("http://", url_part, ".jpg", NULL);
            }
          else
            {
              /* see http://api.themoviedb.org/3/configuration?api_key= for the values */
              tumbler_thumbnail_flavor_get_size (flavor, &dest_width, NULL);
              url = g_strconcat (TMDB_BASE_URL,
                                 dest_width <= 154 ? "w154" : "w342", /* optimize for 128 or 256 */
                                 "/", url_part, ".jpg", NULL);
            }
          g_free (url_part);
        }
      else
        {
          /* check for api key problems */
          if (cover->api_key != NULL
              && strstr (data, "Invalid API key") != NULL)
            {
              g_printerr ("\n%s.\n\n",
                          _("Invalid API key, you must be granted a valid "
                            "key. The Movie DB backend will be disabled."));

              g_free (cover->api_key);
              cover->api_key = NULL;
            }

          g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                               _("No poster key found in metadata"));
        }

      g_free (data);
    }

  return url;
}



static void
cover_thumbnailer_create (TumblerAbstractThumbnailer *thumbnailer,
                          GCancellable               *cancellable,
                          TumblerFileInfo            *info)
{
  CoverThumbnailer       *cover = COVER_THUMBNAILER (thumbnailer);
  const gchar            *uri;
  TumblerThumbnail       *thumbnail;
  gchar                  *year;
  gchar                  *title;
  GdkPixbuf              *pixbuf = NULL;
  gchar                  *poster_url;
  GError                 *error = NULL;
  TumblerImageData        data;
  TumblerThumbnailFlavor *flavor;
  GFile                  *gfile;
  GSList                 *lp;

  /* source file */
  uri = tumbler_file_info_get_uri (info);
  gfile = g_file_new_for_uri (uri);

  /* check if file is in allowed destinations */
  for (lp = cover->locations; lp != NULL; lp = lp->next)
    if (g_file_has_prefix (gfile, lp->data))
      break;

  if (lp == NULL)
    {
      /* location not white-listed */
      g_object_unref (gfile);
      g_signal_emit_by_name (thumbnailer, "error", uri,
                             TUMBLER_ERROR_UNSUPPORTED,
                             _("Location is not whitelisted in rc file"));
      return;
    }

  /* target data */
  thumbnail = tumbler_file_info_get_thumbnail (info);
  flavor = tumbler_thumbnail_get_flavor (thumbnail);

  /* extract title from filename */
  if (cover_thumbnailer_get_title (cover, gfile, &title, &year))
    {
      /* request online metadata and return the poster url */
      poster_url = cover_thumbnailer_poster_url (cover, title, year, flavor, cancellable, &error);

      g_free (title);
      g_free (year);

      if (poster_url != NULL)
        {
          /* download poster and load it in a pixbuf */
          pixbuf = cover_thumbnailer_load_pixbuf (poster_url, flavor, cancellable, &error);
          g_free (poster_url);
        }
    }
  else
    {
      g_set_error_literal (&error, G_IO_ERROR, G_IO_ERROR_INVALID_FILENAME,
                           _("Movie title is too short"));
    }

  if (pixbuf != NULL)
    {
      /* prepare the image data */
      data.data = gdk_pixbuf_get_pixels (pixbuf);
      data.has_alpha = gdk_pixbuf_get_has_alpha (pixbuf);
      data.bits_per_sample = gdk_pixbuf_get_bits_per_sample (pixbuf);
      data.width = gdk_pixbuf_get_width (pixbuf);
      data.height = gdk_pixbuf_get_height (pixbuf);
      data.rowstride = gdk_pixbuf_get_rowstride (pixbuf);
      data.colorspace = (TumblerColorspace) gdk_pixbuf_get_colorspace (pixbuf);

      tumbler_thumbnail_save_image_data (thumbnail, &data,
                                         tumbler_file_info_get_mtime (info),
                                         cancellable, &error);

      g_object_unref (pixbuf);
    }

  /* return the status */
  if (error != NULL)
    {
      g_signal_emit_by_name (thumbnailer, "error", uri,
                             error->code, error->message);
      g_error_free (error);
    }
  else
    {
      g_signal_emit_by_name (thumbnailer, "ready", uri);
    }

  g_object_unref (thumbnail);
  g_object_unref (flavor);
  g_object_unref (gfile);
}
