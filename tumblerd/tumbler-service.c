/* vi:set et ai sw=2 sts=2 ts=2: */
/*-
 * Copyright (c) 2009 Jannis Pohlmann <jannis@xfce.org>
 *
 * This program is free software; you can redistribute it and/or 
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of 
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the 
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public 
 * License along with this program; if not, write to the Free 
 * Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib.h>
#include <glib/gi18n.h>
#include <glib-object.h>

#include <dbus/dbus.h>
#include <dbus/dbus-glib.h>
#include <dbus/dbus-glib-lowlevel.h>

#include <tumbler/tumbler.h>

#include <tumblerd/tumbler-naive-scheduler.h>
#include <tumblerd/tumbler-scheduler.h>
#include <tumblerd/tumbler-service.h>
#include <tumblerd/tumbler-service-dbus-bindings.h>
#include <tumblerd/tumbler-threshold-scheduler.h>
#include <tumblerd/tumbler-utils.h>



/* signal identifiers */
enum
{
  SIGNAL_ERROR,
  SIGNAL_FINISHED,
  SIGNAL_READY,
  SIGNAL_STARTED,
  LAST_SIGNAL,
};

/* property identifiers */
enum
{
  PROP_0,
  PROP_CONNECTION,
  PROP_REGISTRY,
};



static void tumbler_service_constructed        (GObject          *object);
static void tumbler_service_finalize           (GObject          *object);
static void tumbler_service_get_property       (GObject          *object,
                                                guint             prop_id,
                                                GValue           *value,
                                                GParamSpec       *pspec);
static void tumbler_service_set_property       (GObject          *object,
                                                guint             prop_id,
                                                const GValue     *value,
                                                GParamSpec       *pspec);
static void tumbler_service_scheduler_error    (TumblerScheduler *scheduler,
                                                guint             handle,
                                                const GStrv       failed_uris,
                                                gint              error_code,
                                                const gchar      *message,
                                                TumblerService   *service);
static void tumbler_service_scheduler_finished (TumblerScheduler *scheduler,
                                                guint             handle,
                                                TumblerService   *service);
static void tumbler_service_scheduler_ready    (TumblerScheduler *scheduler,
                                                const GStrv       uris,
                                                TumblerService   *service);
static void tumbler_service_scheduler_started  (TumblerScheduler *scheduler,
                                                guint             handle,
                                                TumblerService   *service);



struct _TumblerServiceClass
{
  GObjectClass __parent__;
};

struct _TumblerService
{
  GObject __parent__;

  DBusGConnection  *connection;
  TumblerRegistry  *registry;
  TumblerScheduler *scheduler;
  GMutex           *mutex;
};



static guint tumbler_service_signals[LAST_SIGNAL];



G_DEFINE_TYPE (TumblerService, tumbler_service, G_TYPE_OBJECT);



static void
tumbler_service_class_init (TumblerServiceClass *klass)
{
  GObjectClass *gobject_class;

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->constructed = tumbler_service_constructed; 
  gobject_class->finalize = tumbler_service_finalize; 
  gobject_class->get_property = tumbler_service_get_property;
  gobject_class->set_property = tumbler_service_set_property;

  g_object_class_install_property (gobject_class, PROP_CONNECTION,
                                   g_param_spec_pointer ("connection",
                                                         "connection",
                                                         "connection",
                                                         G_PARAM_READWRITE |
                                                         G_PARAM_CONSTRUCT_ONLY));

  g_object_class_install_property (gobject_class, PROP_REGISTRY,
                                   g_param_spec_object ("registry",
                                                        "registry",
                                                        "registry",
                                                        TUMBLER_TYPE_REGISTRY,
                                                        G_PARAM_READWRITE |
                                                        G_PARAM_CONSTRUCT_ONLY));

  tumbler_service_signals[SIGNAL_ERROR] =
    g_signal_new ("error",
                  TUMBLER_TYPE_SERVICE,
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL,
                  NULL,
                  tumbler_marshal_VOID__UINT_POINTER_INT_STRING,
                  G_TYPE_NONE,
                  4,
                  G_TYPE_UINT,
                  G_TYPE_STRV,
                  G_TYPE_INT,
                  G_TYPE_STRING);

  tumbler_service_signals[SIGNAL_FINISHED] =
    g_signal_new ("finished",
                  TUMBLER_TYPE_SERVICE,
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL,
                  NULL,
                  g_cclosure_marshal_VOID__UINT,
                  G_TYPE_NONE,
                  1,
                  G_TYPE_UINT);

  tumbler_service_signals[SIGNAL_READY] =
    g_signal_new ("ready",
                  TUMBLER_TYPE_SERVICE,
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL,
                  NULL,
                  g_cclosure_marshal_VOID__POINTER,
                  G_TYPE_NONE,
                  1,
                  G_TYPE_STRV);

  tumbler_service_signals[SIGNAL_STARTED] =
    g_signal_new ("started",
                  TUMBLER_TYPE_SERVICE,
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL,
                  NULL,
                  g_cclosure_marshal_VOID__UINT,
                  G_TYPE_NONE,
                  1,
                  G_TYPE_UINT);
}



static void
tumbler_service_init (TumblerService *service)
{
  service->mutex = g_mutex_new ();
}



static void
tumbler_service_constructed (GObject *object)
{
  TumblerService *service = TUMBLER_SERVICE (object);

#if 0
  service->scheduler = tumbler_naive_scheduler_new ();
#else
  service->scheduler = tumbler_threshold_scheduler_new ();
#endif

  g_signal_connect (service->scheduler, "error",
                    G_CALLBACK (tumbler_service_scheduler_error), service);
  g_signal_connect (service->scheduler, "finished", 
                    G_CALLBACK (tumbler_service_scheduler_finished), service);
  g_signal_connect (service->scheduler, "ready", 
                    G_CALLBACK (tumbler_service_scheduler_ready), service);
  g_signal_connect (service->scheduler, "started", 
                    G_CALLBACK (tumbler_service_scheduler_started), service);
}



static void
tumbler_service_finalize (GObject *object)
{
  TumblerService *service = TUMBLER_SERVICE (object);

  g_object_unref (service->scheduler);
  g_object_unref (service->registry);

  dbus_g_connection_unref (service->connection);

  g_mutex_free (service->mutex);

  (*G_OBJECT_CLASS (tumbler_service_parent_class)->finalize) (object);
}



static void
tumbler_service_get_property (GObject    *object,
                              guint       prop_id,
                              GValue     *value,
                              GParamSpec *pspec)
{
  TumblerService *service = TUMBLER_SERVICE (object);

  switch (prop_id)
    {
    case PROP_CONNECTION:
      g_value_set_pointer (value, service->connection);
      break;
    case PROP_REGISTRY:
      g_value_set_object (value, service->registry);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static void
tumbler_service_set_property (GObject      *object,
                              guint         prop_id,
                              const GValue *value,
                              GParamSpec   *pspec)
{
  TumblerService *service = TUMBLER_SERVICE (object);

  switch (prop_id)
    {
    case PROP_CONNECTION:
      service->connection = dbus_g_connection_ref (g_value_get_pointer (value));
      break;
    case PROP_REGISTRY:
      service->registry = g_value_dup_object (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static void
tumbler_service_scheduler_error (TumblerScheduler *scheduler,
                                 guint             handle,
                                 const GStrv       failed_uris,
                                 gint              error_code,
                                 const gchar      *message,
                                 TumblerService   *service)
{
  g_signal_emit (service, tumbler_service_signals[SIGNAL_ERROR], 0, handle, failed_uris, 
                 error_code, message);
}



static void
tumbler_service_scheduler_finished (TumblerScheduler *scheduler,
                                    guint             handle,
                                    TumblerService   *service)
{
  g_signal_emit (service, tumbler_service_signals[SIGNAL_FINISHED], 0, handle);
}



static void
tumbler_service_scheduler_ready (TumblerScheduler *scheduler,
                                 const GStrv       uris,
                                 TumblerService   *service)
{
  g_signal_emit (service, tumbler_service_signals[SIGNAL_READY], 0, uris);
}



static void
tumbler_service_scheduler_started (TumblerScheduler *scheduler,
                                   guint             handle,
                                   TumblerService   *service)
{
  g_signal_emit (service, tumbler_service_signals[SIGNAL_STARTED], 0, handle);
}



TumblerService *
tumbler_service_new (DBusGConnection *connection,
                     TumblerRegistry *registry)
{
  return g_object_new (TUMBLER_TYPE_SERVICE, "connection", connection, 
                       "registry", registry, NULL);
}



gboolean
tumbler_service_start (TumblerService *service,
                       GError        **error)
{
  DBusConnection *connection;
  DBusError       dbus_error;
  gint            result;

  g_return_val_if_fail (TUMBLER_IS_SERVICE (service), FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  g_mutex_lock (service->mutex);

  dbus_error_init (&dbus_error);

  /* get the native D-Bus connection */
  connection = dbus_g_connection_get_connection (service->connection);

  /* request ownership for the generic thumbnailer interface */
  result = dbus_bus_request_name (connection, "org.xfce.thumbnails.Thumbnailer",
                                  DBUS_NAME_FLAG_DO_NOT_QUEUE, &dbus_error);

  /* check if that failed */
  if (result != DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER)
    {
      /* propagate the D-Bus error */
      if (dbus_error_is_set (&dbus_error))
        {
          if (error != NULL)
            dbus_set_g_error (error, &dbus_error);

          dbus_error_free (&dbus_error);
        }
      else if (error != NULL)
        {
          g_set_error (error, DBUS_GERROR, DBUS_GERROR_FAILED,
                       _("Another generic thumbnailer is already running"));
        }

      g_mutex_unlock (service->mutex);

      return FALSE;
    }

  /* everything is fine, install the generic thumbnailer D-Bus info */
  dbus_g_object_type_install_info (G_OBJECT_TYPE (service),
                                   &dbus_glib_tumbler_service_object_info);

  /* register the service instance as a handler of this interface */
  dbus_g_connection_register_g_object (service->connection, 
                                       "/org/xfce/thumbnails/Thumbnailer", 
                                       G_OBJECT (service));

  g_mutex_unlock (service->mutex);

  return TRUE;
}



void
tumbler_service_queue (TumblerService        *service,
                       const GStrv            uris,
                       const GStrv            mime_hints,
                       guint                  handle_to_unqueue,
                       DBusGMethodInvocation *context)
{
  TumblerSchedulerRequest *scheduler_request;
  TumblerThumbnailer     **thumbnailers;
  gint                     num_thumbnailers;
  guint                    handle;

  dbus_async_return_if_fail (TUMBLER_IS_SERVICE (service), context);
  dbus_async_return_if_fail (uris != NULL, context);
  dbus_async_return_if_fail (mime_hints != NULL, context);

  g_mutex_lock (service->mutex);

  /* unqueue the request with the given unqueue handle */
  if (handle_to_unqueue != 0)
    tumbler_scheduler_unqueue (service->scheduler, handle_to_unqueue);

  /* get an array with one thumbnailer for each URI in the request */
  thumbnailers = tumbler_registry_get_thumbnailer_array (service->registry,
                                                         uris, mime_hints, 
                                                         &num_thumbnailers);

  /* allocate a scheduler request */
  scheduler_request = tumbler_scheduler_request_new (uris, mime_hints, thumbnailers,
                                                     num_thumbnailers);

  /* get the request handle */
  handle = scheduler_request->handle;

  /* push the request to the scheduler */
  tumbler_scheduler_push (service->scheduler, scheduler_request);
  
  /* free the thumbnailer array */
  tumbler_thumbnailer_array_free (thumbnailers, num_thumbnailers);

  g_mutex_unlock (service->mutex);

  dbus_g_method_return (context, handle);
}



void
tumbler_service_unqueue (TumblerService        *service,
                         guint                  handle,
                         DBusGMethodInvocation *context)
{
  dbus_async_return_if_fail (TUMBLER_IS_SERVICE (service), context);

  g_mutex_lock (service->mutex);

  /* unqueue the request with the given unqueue handle */
  if (handle != 0)
    tumbler_scheduler_unqueue (service->scheduler, handle);

  g_mutex_unlock (service->mutex);

  dbus_g_method_return (context);
}
