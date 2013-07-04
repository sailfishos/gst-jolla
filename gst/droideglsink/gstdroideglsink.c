/*
 * Copyright (C) 2013 Jolla LTD.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#include "gstdroideglsink.h"
#include <gst/video/video.h>
#include "gst/interfaces/nemovideotexture.h"
#include <GLES2/gl2ext.h>
#include <EGL/eglext.h>

#define EGL_NATIVE_BUFFER_ANDROID 0x3140
#define EGL_IMAGE_PRESERVED_KHR   0x30D2
#define HAL_PIXEL_FORMAT_YCbCr_420_SP 0x109

#define BUFFER_LOCK_USAGE GRALLOC_USAGE_SW_READ_RARELY | GRALLOC_USAGE_SW_WRITE_OFTEN
#define BUFFER_ALLOC_USAGE GRALLOC_USAGE_SW_READ_OFTEN | GRALLOC_USAGE_SW_WRITE_OFTEN | GRALLOC_USAGE_HW_TEXTURE

#define container_of(ptr, type, member) ({ \
      const typeof( ((type *)0)->member ) *__mptr = (ptr); (type *)( (char *)__mptr - offsetof(type,member) );})

GST_DEBUG_CATEGORY_STATIC (droideglsink_debug);
#define GST_CAT_DEFAULT droideglsink_debug

enum
{
  PROP_0,
  PROP_EGL_DISPLAY,
};

static GstStaticPadTemplate sink_template = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (GST_NATIVE_BUFFER_NAME ","
        "framerate = (fraction) [ 0, MAX ], "
        "width = (int) [ 1, MAX ], " "height = (int) [ 1, MAX ] ;"
        GST_VIDEO_CAPS_YUV ("{ YV12, NV21, NV12 }")));

static void gst_droid_egl_sink_finalize (GObject * object);
static void gst_droid_egl_sink_set_property (GObject * object,
    guint prop_id, const GValue * value, GParamSpec * pspec);
static void gst_droid_egl_sink_get_property (GObject * object,
    guint prop_id, GValue * value, GParamSpec * pspec);
static GstFlowReturn gst_droid_egl_sink_show_frame (GstVideoSink * bsink,
    GstBuffer * buf);
static gboolean gst_droid_egl_sink_start (GstBaseSink * bsink);
static gboolean gst_droid_egl_sink_stop (GstBaseSink * bsink);
static gboolean gst_droid_egl_sink_set_caps (GstBaseSink * bsink,
    GstCaps * caps);
static void gst_droid_egl_sink_get_times (GstBaseSink * bsink, GstBuffer * buf,
    GstClockTime * start, GstClockTime * end);
GstFlowReturn gst_droid_egl_sink_buffer_alloc (GstBaseSink * bsink,
    guint64 offset, guint size, GstCaps * caps, GstBuffer ** buf);

static buffer_handle_t gst_droid_egl_sink_alloc_handle (GstDroidEglSink * sink,
    int *stride);
static gboolean gst_droid_egl_sink_recycle_buffer (void *data,
    GstNativeBuffer * buffer);
static void gst_droid_egl_sink_destroy_handle (GstDroidEglSink * sink,
    buffer_handle_t handle, GstGralloc * gralloc);

static void gst_droid_egl_sink_do_init (GType type);
static void
gst_droid_egl_sink_implements_interface_init (GstImplementsInterfaceClass *
    klass);
static void
gst_droid_egl_sink_videotexture_interface_init (NemoGstVideoTextureClass *
    iface);
static gboolean gst_droid_egl_sink_interfaces_supported (GstDroidEglSink * sink,
    GType type);
static gboolean gst_droid_egl_sink_acquire_frame (NemoGstVideoTexture * bsink);
static gboolean gst_droid_egl_sink_bind_frame (NemoGstVideoTexture * bsink,
    EGLImageKHR * image);
static void gst_droid_egl_sink_unbind_frame (NemoGstVideoTexture * bsink);
static void gst_droid_egl_sink_release_frame (NemoGstVideoTexture * bsink,
    EGLSyncKHR sync);

static GstDroidEglBuffer
    * gst_droid_egl_sink_find_buffer_unlocked (GstDroidEglSink * sink,
    GstBuffer * buffer);
static GstDroidEglBuffer
    * gst_droid_egl_sink_find_free_buffer_unlocked (GstDroidEglSink * sink);

static void gst_droid_egl_sink_native_buffer_ref (struct android_native_base_t
    *base);
static void gst_droid_egl_sink_native_buffer_unref (struct android_native_base_t
    *base);
static int gst_droid_egl_sink_get_hal_format (GstVideoFormat format);
static GstDroidEglBuffer *gst_droid_egl_sink_alloc_buffer (GstDroidEglSink *
    sink, buffer_handle_t handle, int stride);
static void gst_droid_egl_sink_destroy_buffer (GstDroidEglSink *
    sink, GstDroidEglBuffer * buffer);
static GstDroidEglBuffer *gst_droid_egl_sink_alloc_buffer_empty (GstDroidEglSink
    * sink);
static void gst_droid_egl_sink_set_native_buffer (GstDroidEglBuffer * buffer,
    GstNativeBuffer * native);
static gboolean gst_droid_egl_sink_event (GstBaseSink * bsink,
    GstEvent * event);
static GstDroidEglBuffer
    * gst_droid_egl_sink_create_buffer_unlocked (GstDroidEglSink * sink,
    GstBuffer * buff);

GST_BOILERPLATE_FULL (GstDroidEglSink, gst_droid_egl_sink, GstVideoSink,
    GST_TYPE_VIDEO_SINK, gst_droid_egl_sink_do_init);

static void
gst_droid_egl_sink_base_init (gpointer gclass)
{
  GstElementClass *element_class = GST_ELEMENT_CLASS (gclass);

  gst_element_class_add_static_pad_template (element_class, &sink_template);

  gst_element_class_set_details_simple (element_class,
      "Android native buffers EGL sink",
      "Sink/Video/Device",
      "Render Android native buffers via EGL",
      "Mohammed Hassan <mohammed.hassan@jollamobile.com>");
}

static void
gst_droid_egl_sink_class_init (GstDroidEglSinkClass * klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;
  GstVideoSinkClass *videosink_class = (GstVideoSinkClass *) klass;
  GstBaseSinkClass *basesink_class = (GstBaseSinkClass *) klass;

  gobject_class->finalize = gst_droid_egl_sink_finalize;
  gobject_class->set_property = gst_droid_egl_sink_set_property;
  gobject_class->get_property = gst_droid_egl_sink_get_property;

  videosink_class->show_frame =
      GST_DEBUG_FUNCPTR (gst_droid_egl_sink_show_frame);
  basesink_class->start = GST_DEBUG_FUNCPTR (gst_droid_egl_sink_start);
  basesink_class->stop = GST_DEBUG_FUNCPTR (gst_droid_egl_sink_stop);
  basesink_class->set_caps = GST_DEBUG_FUNCPTR (gst_droid_egl_sink_set_caps);
  basesink_class->get_times = GST_DEBUG_FUNCPTR (gst_droid_egl_sink_get_times);
  basesink_class->buffer_alloc =
      GST_DEBUG_FUNCPTR (gst_droid_egl_sink_buffer_alloc);
  basesink_class->event = GST_DEBUG_FUNCPTR (gst_droid_egl_sink_event);

  g_object_class_install_property (gobject_class, PROP_EGL_DISPLAY,
      g_param_spec_pointer ("egl-display",
          "EGL display ",
          "The application provided EGL display to be used for creating EGLImageKHR objects.",
          G_PARAM_READWRITE));
}

static void
gst_droid_egl_sink_init (GstDroidEglSink * sink, GstDroidEglSinkClass * gclass)
{
  GST_OBJECT_FLAG_SET (sink, GST_ELEMENT_IS_SINK);

  gst_base_sink_set_last_buffer_enabled (GST_BASE_SINK (sink), FALSE);

  sink->gralloc = NULL;
  sink->format = GST_VIDEO_FORMAT_UNKNOWN;
  sink->hal_format = 0;
  sink->dpy = EGL_NO_DISPLAY;

  g_mutex_init (&sink->buffer_lock);

  sink->buffers = g_ptr_array_new ();
}

static void
gst_droid_egl_sink_finalize (GObject * object)
{
  GstDroidEglSink *sink = GST_DROID_EGL_SINK (object);

  g_mutex_clear (&sink->buffer_lock);

  // TODO: free all buffers
  g_ptr_array_free (sink->buffers, TRUE);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gst_droid_egl_sink_set_property (GObject * object,
    guint prop_id, const GValue * value, GParamSpec * pspec)
{
  GstDroidEglSink *sink = GST_DROID_EGL_SINK (object);

  switch (prop_id) {
    case PROP_EGL_DISPLAY:
      sink->dpy = g_value_get_pointer (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_droid_egl_sink_get_property (GObject * object,
    guint prop_id, GValue * value, GParamSpec * pspec)
{
  GstDroidEglSink *sink = GST_DROID_EGL_SINK (object);

  switch (prop_id) {
    case PROP_EGL_DISPLAY:
      g_value_set_pointer (value, sink->dpy);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static GstFlowReturn
gst_droid_egl_sink_show_frame (GstVideoSink * bsink, GstBuffer * buf)
{
  GstDroidEglSink *sink = GST_DROID_EGL_SINK (bsink);
  GstDroidEglBuffer *old_buffer = NULL;
  GstDroidEglBuffer *buffer = NULL;

  GST_DEBUG_OBJECT (sink, "show frame");

  g_mutex_lock (&sink->buffer_lock);

  buffer = gst_droid_egl_sink_find_buffer_unlocked (sink, buf);
  if (!buffer) {
    GST_DEBUG_OBJECT (sink, "no buffer found. Picking any free buffer.");
    buffer = gst_droid_egl_sink_find_free_buffer_unlocked (sink);

    if (!buffer) {
      GST_DEBUG_OBJECT (sink,
          "no free buffer could be found. Creating a new one");
      buffer = gst_droid_egl_sink_create_buffer_unlocked (sink, buf);
    }
  }

  if (!buffer) {
    g_mutex_unlock (&sink->buffer_lock);
    GST_ELEMENT_ERROR (sink, STREAM, FAILED, ("Unknown buffer"), (NULL));
    return GST_FLOW_ERROR;
  }

  old_buffer = sink->last_buffer;
  sink->last_buffer = buffer;

  g_mutex_unlock (&sink->buffer_lock);

  // TODO: destroy foreign
  if (old_buffer) {
    if (old_buffer->sync != EGL_NO_SYNC_KHR) {
      if (sink->eglClientWaitSyncKHR (sink->dpy, old_buffer->sync, 0,
              EGL_FOREVER_KHR)
          != EGL_CONDITION_SATISFIED_KHR) {
        GST_WARNING_OBJECT (sink, "eglClientWaitSyncKHR failed for buffer %p",
            old_buffer);
      }

      if (sink->eglDestroySyncKHR (sink->dpy, old_buffer->sync) != EGL_TRUE) {
        GST_WARNING_OBJECT (sink, "Failed to destroy sync object for buffer %p",
            old_buffer);
      }

      old_buffer->sync = EGL_NO_SYNC_KHR;
    }

    gst_buffer_unref (GST_BUFFER (old_buffer->buff));
  }

  /* We will always use 0. */
  nemo_gst_video_texture_frame_ready (NEMO_GST_VIDEO_TEXTURE (sink), 0);

  return GST_FLOW_OK;
}

static gboolean
gst_droid_egl_sink_start (GstBaseSink * bsink)
{
  GstDroidEglSink *sink = GST_DROID_EGL_SINK (bsink);

  GST_DEBUG_OBJECT (sink, "start");

  sink->last_buffer = NULL;
  sink->acquired_buffer = NULL;

  sink->gralloc = gst_gralloc_new ();
  if (!sink->gralloc) {
    GST_ELEMENT_ERROR (sink, LIBRARY, INIT, ("Could not initialize gralloc"),
        (NULL));

    return FALSE;
  }

  return TRUE;
}

static gboolean
gst_droid_egl_sink_stop (GstBaseSink * bsink)
{
  GstDroidEglSink *sink = GST_DROID_EGL_SINK (bsink);

  GST_DEBUG_OBJECT (sink, "stop");

  if (sink->gralloc) {
    gst_gralloc_unref (sink->gralloc);
    sink->gralloc = NULL;
  }

  if (sink->last_buffer) {
    gst_buffer_unref (GST_BUFFER (sink->last_buffer->buff));
    sink->last_buffer = NULL;
  }

  return TRUE;
}

static gboolean
gst_droid_egl_sink_set_caps (GstBaseSink * bsink, GstCaps * caps)
{
  int width, height;
  int fps_n, fps_d;
  GstVideoFormat format = GST_VIDEO_FORMAT_UNKNOWN;
  int hal_format = -1;
  gboolean is_native = FALSE;
  GstDroidEglSink *sink = GST_DROID_EGL_SINK (bsink);
  GstVideoSink *vsink = GST_VIDEO_SINK (bsink);

  if (caps
      && gst_caps_is_equal (caps, GST_PAD_CAPS (GST_BASE_SINK_PAD (bsink)))) {
    GST_LOG_OBJECT (sink, "same caps");
    return TRUE;
  }

  GST_DEBUG_OBJECT (sink, "set caps %" GST_PTR_FORMAT, caps);

  if (!strcmp (gst_structure_get_name (gst_caps_get_structure (caps, 0)),
          GST_NATIVE_BUFFER_NAME)) {
    /* We have native buffer. */
    is_native = TRUE;

    GST_DEBUG_OBJECT (sink, "native format");
  }

  if (!gst_video_format_parse_caps (caps, &format, &width, &height)) {
    GST_ELEMENT_ERROR (sink, STREAM, FORMAT, ("Failed to parse caps"), (NULL));
    return FALSE;
  }

  if (width == 0 || height == 0) {
    GST_ELEMENT_ERROR (sink, STREAM, FORMAT, ("Caps without width or height"),
        (NULL));
    return FALSE;
  }

  if (!gst_video_parse_caps_framerate (caps, &fps_n, &fps_d)) {
    GST_ELEMENT_ERROR (sink, STREAM, FORMAT, ("Failed to parse framerate"),
        (NULL));
    return FALSE;
  }

  hal_format = gst_droid_egl_sink_get_hal_format (format);

  if (hal_format == -1) {
    if (!is_native) {
      GST_ELEMENT_ERROR (sink, STREAM, FORMAT, ("Unsupported color format 0x%x",
              format), (NULL));
      return FALSE;
    } else {
      const GstStructure *s = gst_caps_get_structure (caps, 0);
      if (!gst_structure_has_field (s, "format")) {
        GST_ELEMENT_ERROR (sink, STREAM, FORMAT, ("No color format provided"),
            (NULL));
        return FALSE;
      }

      if (!gst_structure_get_int (s, "format", &hal_format) || hal_format < 0) {
        GST_ELEMENT_ERROR (sink, STREAM, FORMAT,
            ("Invalid color format provided"), (NULL));
        return FALSE;
      }
    }
  }

  vsink->width = width;
  vsink->height = height;

  sink->format = format;
  sink->fps_n = fps_n;
  sink->fps_d = fps_d;
  sink->hal_format = hal_format;

  return TRUE;
}

static void
gst_droid_egl_sink_get_times (GstBaseSink * bsink, GstBuffer * buf,
    GstClockTime * start, GstClockTime * end)
{
  GstDroidEglSink *sink = GST_DROID_EGL_SINK (bsink);

  GST_DEBUG_OBJECT (sink, "get times");

  if (GST_BUFFER_TIMESTAMP_IS_VALID (buf)) {
    *start = GST_BUFFER_TIMESTAMP (buf);
    if (GST_BUFFER_DURATION_IS_VALID (buf)) {
      *end = *start + GST_BUFFER_DURATION (buf);
    } else {
      if (sink->fps_n > 0) {
        *end = *start +
            gst_util_uint64_scale_int (GST_SECOND, sink->fps_d, sink->fps_n);
      }
    }
  }
}

GstFlowReturn
gst_droid_egl_sink_buffer_alloc (GstBaseSink * bsink, guint64 offset,
    guint size, GstCaps * caps, GstBuffer ** buf)
{
  int stride;
  buffer_handle_t handle;
  GstDroidEglSink *sink = GST_DROID_EGL_SINK (bsink);
  void *data = NULL;
  GstVideoSink *vsink = GST_VIDEO_SINK (sink);
  GstDroidEglBuffer *buffer;

  GST_DEBUG_OBJECT (sink, "buffer alloc");

  // TODO:
  g_assert (sink->hal_format != -1);

  if (!sink->gralloc) {
    GST_ELEMENT_ERROR (sink, LIBRARY, FAILED, ("no gralloc"), (NULL));

    return GST_FLOW_ERROR;
  }

  if (!gst_droid_egl_sink_set_caps (bsink, caps)) {
    return GST_FLOW_ERROR;
  }

  g_mutex_lock (&sink->buffer_lock);

  buffer = gst_droid_egl_sink_find_free_buffer_unlocked (sink);
  if (buffer) {
    GST_LOG_OBJECT (sink, "free buffer found %p", buffer);
    buffer->free = FALSE;
  }

  g_mutex_unlock (&sink->buffer_lock);

  if (buffer) {
    gboolean res =
        gst_native_buffer_lock (buffer->buff, sink->format, vsink->width,
        vsink->height, BUFFER_LOCK_USAGE);
    if (!res) {
      g_mutex_lock (&sink->buffer_lock);
      buffer->free = TRUE;
      g_mutex_unlock (&sink->buffer_lock);

      GST_ELEMENT_ERROR (sink, LIBRARY, FAILED,
          ("Could not lock native buffer handle"), (NULL));

      return GST_FLOW_ERROR;
    } else {
      g_mutex_lock (&sink->buffer_lock);
      buffer->locked = TRUE;
      g_mutex_unlock (&sink->buffer_lock);

      *buf = GST_BUFFER (buffer->buff);
      return GST_FLOW_OK;
    }
  }

  handle = gst_droid_egl_sink_alloc_handle (sink, &stride);

  if (!handle) {
    GST_ELEMENT_ERROR (sink, LIBRARY, FAILED,
        ("Could not allocate native buffer handle"), (NULL));

    return GST_FLOW_ERROR;
  }

  if (sink->gralloc->gralloc->lock (sink->gralloc->gralloc, handle,
          BUFFER_LOCK_USAGE, 0, 0, vsink->width, vsink->height, &data) != 0) {

    gst_droid_egl_sink_destroy_handle (sink, handle, sink->gralloc);

    GST_ELEMENT_ERROR (sink, LIBRARY, FAILED,
        ("Could not lock native buffer handle"), (NULL));

    return GST_FLOW_ERROR;
  }

  buffer = gst_droid_egl_sink_alloc_buffer (sink, handle, stride);

  GST_BUFFER_DATA (buffer->buff) = data;
  GST_BUFFER_SIZE (buffer->buff) =
      gst_video_format_get_size (sink->format, vsink->width, vsink->height);

  *buf = GST_BUFFER (buffer->buff);

  g_mutex_lock (&sink->buffer_lock);
  g_ptr_array_add (sink->buffers, buffer);
  g_mutex_unlock (&sink->buffer_lock);

  GST_DEBUG_OBJECT (sink, "pushed buffer %p upstream", *buf);

  return GST_FLOW_OK;
}

static buffer_handle_t
gst_droid_egl_sink_alloc_handle (GstDroidEglSink * sink, int *stride)
{
  GstVideoSink *vsink = GST_VIDEO_SINK (sink);

  GST_DEBUG_OBJECT (sink, "alloc handle");

  buffer_handle_t handle =
      gst_gralloc_allocate (sink->gralloc, vsink->width, vsink->height,
      sink->hal_format, BUFFER_ALLOC_USAGE, stride);

  if (!handle) {
    GST_ELEMENT_ERROR (sink, LIBRARY, FAILED,
        ("Could not allocate native buffer handle"), (NULL));
    return NULL;
  }

  return handle;
}

static void
gst_droid_egl_sink_destroy_handle (GstDroidEglSink * sink,
    buffer_handle_t handle, GstGralloc * gralloc)
{
  GST_DEBUG_OBJECT (sink, "destroy handle");

  gst_gralloc_free (gralloc, handle);
}

static gboolean
gst_droid_egl_sink_recycle_buffer (void *data, GstNativeBuffer * buffer)
{
  GstDroidEglSink *sink = (GstDroidEglSink *) data;
  GstDroidEglBuffer *buff;

  GST_DEBUG_OBJECT (sink, "recycle buffer");

  g_mutex_lock (&sink->buffer_lock);

  buff = gst_droid_egl_sink_find_buffer_unlocked (sink, GST_BUFFER (buffer));
  g_assert (buff);

  g_mutex_unlock (&sink->buffer_lock);

  if (buff->locked) {
    gst_native_buffer_unlock (buffer);
    buff->locked = FALSE;
  }
  // TODO: Do we destroy handle?
  if (buff->drop) {
    if (buff->extra_buffer) {
      gst_buffer_unref (buff->extra_buffer);
    }

    g_mutex_lock (&sink->buffer_lock);
    g_ptr_array_remove (sink->buffers, buff);
    g_mutex_unlock (&sink->buffer_lock);
    gst_droid_egl_sink_destroy_buffer (sink, buff);

    gst_object_unref (sink);

    return FALSE;
  } else {
    g_mutex_lock (&sink->buffer_lock);

    buff->free = TRUE;

    gst_buffer_ref (GST_BUFFER (buff->buff));

    g_mutex_unlock (&sink->buffer_lock);

    return TRUE;
  }
}

static GstDroidEglBuffer *
gst_droid_egl_sink_find_buffer_unlocked (GstDroidEglSink * sink,
    GstBuffer * buffer)
{
  int x;
  GstDroidEglBuffer *buff;

  for (x = 0; x < sink->buffers->len; x++) {
    buff = sink->buffers->pdata[x];
    if (GST_BUFFER (buff->buff) == buffer) {
      return buff;
    }
  }

  return NULL;
}

static void
gst_droid_egl_sink_do_init (GType type)
{
  GST_DEBUG_CATEGORY_INIT (droideglsink_debug, "droideglsink", 0,
      "Android native bufer EGL sink");

  static const GInterfaceInfo implements_iface_info = {
    (GInterfaceInitFunc) gst_droid_egl_sink_implements_interface_init,
    NULL,
    NULL,
  };

  static const GInterfaceInfo vt_iface_info = {
    (GInterfaceInitFunc) gst_droid_egl_sink_videotexture_interface_init,
    NULL,
    NULL,
  };

  g_type_add_interface_static (type, GST_TYPE_IMPLEMENTS_INTERFACE,
      &implements_iface_info);

  g_type_add_interface_static (type, NEMO_GST_TYPE_VIDEO_TEXTURE,
      &vt_iface_info);
}

static void
gst_droid_egl_sink_implements_interface_init (GstImplementsInterfaceClass *
    klass)
{
  klass->supported =
      (gboolean (*)(GstImplementsInterface *,
          GType)) gst_droid_egl_sink_interfaces_supported;
}

static void
gst_droid_egl_sink_videotexture_interface_init (NemoGstVideoTextureClass *
    iface)
{
  iface->acquire_frame = gst_droid_egl_sink_acquire_frame;
  iface->bind_frame = gst_droid_egl_sink_bind_frame;
  iface->unbind_frame = gst_droid_egl_sink_unbind_frame;
  iface->release_frame = gst_droid_egl_sink_release_frame;
}

static gboolean
gst_droid_egl_sink_interfaces_supported (GstDroidEglSink * sink, GType type)
{
  return (type == NEMO_GST_TYPE_VIDEO_TEXTURE);
}

static gboolean
gst_droid_egl_sink_acquire_frame (NemoGstVideoTexture * bsink)
{
  GstDroidEglSink *sink = GST_DROID_EGL_SINK (bsink);
  gboolean ret;

  GST_DEBUG_OBJECT (sink, "acquire frame");

  if (sink->dpy == EGL_NO_DISPLAY) {
    return FALSE;
  }

  g_mutex_lock (&sink->buffer_lock);

  ret = (sink->last_buffer != NULL);

  if (sink->last_buffer) {
    sink->acquired_buffer = sink->last_buffer;
    sink->last_buffer->acquired = TRUE;
    gst_buffer_ref (GST_BUFFER (sink->last_buffer->buff));
  }

  g_mutex_unlock (&sink->buffer_lock);

  return ret;
}

static gboolean
gst_droid_egl_sink_bind_frame (NemoGstVideoTexture * bsink, EGLImageKHR * image)
{
  GstDroidEglSink *sink = GST_DROID_EGL_SINK (bsink);
  GstDroidEglBuffer *buffer;
  EGLint eglImgAttrs[] =
      { EGL_IMAGE_PRESERVED_KHR, EGL_TRUE, EGL_NONE, EGL_NONE };

  GST_DEBUG_OBJECT (sink, "bind frame");

  if (sink->dpy == EGL_NO_DISPLAY) {
    return FALSE;
  }

  buffer = sink->acquired_buffer;
  if (buffer->locked) {
    gst_native_buffer_unlock (buffer->buff);
    buffer->locked = FALSE;
  }

  if (!sink->eglCreateImageKHR) {
    sink->eglCreateImageKHR =
        (PFNEGLCREATEIMAGEKHRPROC) eglGetProcAddress ("eglCreateImageKHR");
  }

  if (!sink->eglDestroyImageKHR) {
    sink->eglDestroyImageKHR =
        (PFNEGLDESTROYIMAGEKHRPROC) eglGetProcAddress ("eglDestroyImageKHR");
  }

  if (!sink->eglDestroySyncKHR) {
    sink->eglDestroySyncKHR =
        (PFNEGLDESTROYSYNCKHRPROC) eglGetProcAddress ("eglDestroySyncKHR");
  }

  if (!sink->eglClientWaitSyncKHR) {
    sink->eglClientWaitSyncKHR = (PFNEGLCLIENTWAITSYNCKHRPROC)
        eglGetProcAddress ("eglClientWaitSyncKHR");
  }

  if (buffer->image == EGL_NO_IMAGE_KHR) {
    GST_DEBUG_OBJECT (sink, "creating EGLImage KHR for buffer %p", buffer);
    buffer->image =
        sink->eglCreateImageKHR (sink->dpy,
        EGL_NO_CONTEXT, EGL_NATIVE_BUFFER_ANDROID,
        (EGLClientBuffer) & buffer->native, eglImgAttrs);
  }

  *image = buffer->image;

  return TRUE;
}

static void
gst_droid_egl_sink_unbind_frame (NemoGstVideoTexture * bsink)
{
  GstDroidEglSink *sink = GST_DROID_EGL_SINK (bsink);
  GstDroidEglBuffer *buffer;
  EGLBoolean res = EGL_TRUE;

  GST_DEBUG_OBJECT (sink, "unbind frame");

  buffer = sink->acquired_buffer;

  if (buffer->image != EGL_NO_IMAGE_KHR) {
    res = sink->eglDestroyImageKHR (sink->dpy, buffer->image);
    buffer->image = EGL_NO_IMAGE_KHR;
  }

  if (res != EGL_TRUE) {
    GST_WARNING_OBJECT (sink, "Failed to destroy image for buffer %p", buffer);
  }
}

static void
gst_droid_egl_sink_release_frame (NemoGstVideoTexture * bsink, EGLSyncKHR sync)
{
  GstDroidEglSink *sink = GST_DROID_EGL_SINK (bsink);
  GstDroidEglBuffer *buffer;

  GST_DEBUG_OBJECT (sink, "release frame");

  g_mutex_lock (&sink->buffer_lock);
  buffer = sink->acquired_buffer;
  sink->acquired_buffer = NULL;
  g_mutex_unlock (&sink->buffer_lock);

  buffer->acquired = FALSE;
  gst_buffer_unref (GST_BUFFER (buffer->buff));

  // TODO: sync
}

static GstDroidEglBuffer *
gst_droid_egl_sink_find_free_buffer_unlocked (GstDroidEglSink * sink)
{
  int x;
  GstDroidEglBuffer *buff;

  for (x = 0; x < sink->buffers->len; x++) {
    buff = sink->buffers->pdata[x];
    if (buff->free) {
      return buff;
    }
  }

  return NULL;
}

static void
gst_droid_egl_sink_native_buffer_ref (struct android_native_base_t *base)
{
  struct ANativeWindowBuffer *win =
      container_of (base, struct ANativeWindowBuffer, common);
  GstDroidEglBuffer *buffer = container_of (win, GstDroidEglBuffer, native);

  gst_buffer_ref (GST_BUFFER (buffer->buff));
}

static void
gst_droid_egl_sink_native_buffer_unref (struct android_native_base_t *base)
{
  struct ANativeWindowBuffer *win =
      container_of (base, struct ANativeWindowBuffer, common);
  GstDroidEglBuffer *buffer = container_of (win, GstDroidEglBuffer, native);

  gst_buffer_unref (GST_BUFFER (buffer->buff));
}

static int
gst_droid_egl_sink_get_hal_format (GstVideoFormat format)
{
  switch (format) {
    case GST_VIDEO_FORMAT_YV12:
      return HAL_PIXEL_FORMAT_YV12;
    case GST_VIDEO_FORMAT_NV21:
      return HAL_PIXEL_FORMAT_YCrCb_420_SP;
    case GST_VIDEO_FORMAT_NV12:
      return HAL_PIXEL_FORMAT_YCbCr_420_SP;
    default:
      return -1;
  }
}

static GstDroidEglBuffer *
gst_droid_egl_sink_alloc_buffer_empty (GstDroidEglSink * sink)
{
  GstVideoSink *vsink = GST_VIDEO_SINK (sink);
  GstDroidEglBuffer *buffer;

  GST_DEBUG_OBJECT (sink, "alloc buffer empty");

  buffer = g_malloc (sizeof (GstDroidEglBuffer));
  buffer->sync = EGL_NO_SYNC_KHR;
  buffer->native.common.magic = ANDROID_NATIVE_BUFFER_MAGIC;
  buffer->native.common.version = sizeof (struct ANativeWindowBuffer);
  buffer->native.width = vsink->width;
  buffer->native.height = vsink->height;
  buffer->native.format = sink->hal_format;
  buffer->native.usage = BUFFER_ALLOC_USAGE;

  buffer->native.common.incRef = gst_droid_egl_sink_native_buffer_ref;
  buffer->native.common.decRef = gst_droid_egl_sink_native_buffer_unref;

  memset (buffer->native.common.reserved, 0,
      sizeof (buffer->native.common.reserved));

  buffer->buff = NULL;

  buffer->image = EGL_NO_IMAGE_KHR;
  buffer->free = FALSE;
  buffer->locked = TRUE;
  buffer->acquired = FALSE;
  buffer->drop = FALSE;
  buffer->foreign = FALSE;
  buffer->extra_buffer = NULL;

  return buffer;
}

static void
gst_droid_egl_sink_set_native_buffer (GstDroidEglBuffer * buffer,
    GstNativeBuffer * native)
{
  g_assert (buffer->buff == NULL);

  buffer->buff = native;
  buffer->native.stride = gst_native_buffer_get_stride (native);
  buffer->native.usage = gst_native_buffer_get_usage (native);
  buffer->native.handle = gst_native_buffer_get_handle (native);
}

static GstDroidEglBuffer *
gst_droid_egl_sink_alloc_buffer (GstDroidEglSink * sink, buffer_handle_t handle,
    int stride)
{
  GstDroidEglBuffer *buffer;
  GstNativeBuffer *buff;

  GST_DEBUG_OBJECT (sink, "alloc buffer");

  buffer = gst_droid_egl_sink_alloc_buffer_empty (sink);
  buff =
      gst_native_buffer_new (handle, sink->gralloc, stride, BUFFER_ALLOC_USAGE);
  gst_droid_egl_sink_set_native_buffer (buffer, buff);

  gst_native_buffer_set_finalize_callback (buff,
      gst_droid_egl_sink_recycle_buffer, gst_object_ref (sink));

  return buffer;
}

static void
gst_droid_egl_sink_destroy_buffer (GstDroidEglSink *
    sink, GstDroidEglBuffer * buffer)
{
  EGLBoolean res = EGL_TRUE;

  GST_DEBUG_OBJECT (sink, "destroy buffer %p (%p)", buffer, buffer->native);

  if (buffer->image != EGL_NO_IMAGE_KHR) {
    res = sink->eglDestroyImageKHR (sink->dpy, buffer->image);
    buffer->image = EGL_NO_IMAGE_KHR;
  }

  if (res != EGL_TRUE) {
    GST_WARNING_OBJECT (sink, "Failed to destroy image for buffer %p", buffer);
  }

  if (buffer->sync) {
    if (sink->eglDestroySyncKHR (sink->dpy, buffer->sync) != EGL_TRUE) {
      GST_WARNING_OBJECT (sink, "Failed to destroy sync object for buffer %p",
          buffer);
    }
  }

  g_free (buffer);
}

static gboolean
gst_droid_egl_sink_event (GstBaseSink * bsink, GstEvent * event)
{
  GstDroidEglSink *sink = GST_DROID_EGL_SINK (bsink);
  GstDroidEglBuffer *old_buffer;

  GST_DEBUG_OBJECT (sink, "event %" GST_PTR_FORMAT, event);

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_FLUSH_START:
    case GST_EVENT_FLUSH_STOP:
    case GST_EVENT_EOS:
      break;
    default:
      return FALSE;
  }

  g_mutex_lock (&sink->buffer_lock);
  old_buffer = sink->last_buffer;
  sink->last_buffer = NULL;

  g_mutex_unlock (&sink->buffer_lock);

  if (old_buffer) {
    gst_buffer_unref (GST_BUFFER (old_buffer->buff));
  }

  GST_DEBUG_OBJECT (sink, "current buffer %p", sink->acquired_buffer);

  /* We will simply gamble and not touch the acauired_buffer and hope
     application will just release it ASAP. */
  nemo_gst_video_texture_frame_ready (NEMO_GST_VIDEO_TEXTURE (sink), -1);

  return TRUE;
}

static GstDroidEglBuffer *
gst_droid_egl_sink_create_buffer_unlocked (GstDroidEglSink * sink,
    GstBuffer * buff)
{
  GstDroidEglBuffer *buffer;

  GST_DEBUG_OBJECT (sink, "create buffer unlocked");

  buffer = gst_droid_egl_sink_alloc_buffer_empty (sink);

  if (GST_IS_NATIVE_BUFFER (buff)) {
    GstNativeBuffer *buf = GST_NATIVE_BUFFER (buff);
    GstNativeBuffer *native =
        gst_native_buffer_new (gst_native_buffer_get_handle (buf),
        gst_native_buffer_get_gralloc (buf),
        gst_native_buffer_get_stride (buf),
        gst_native_buffer_get_usage (buf));
    gst_droid_egl_sink_set_native_buffer (buffer, native);
    gst_native_buffer_set_finalize_callback (native,
        gst_droid_egl_sink_recycle_buffer, gst_object_ref (sink));

    buffer->free = FALSE;
    buffer->foreign = FALSE;
    buffer->extra_buffer = gst_buffer_ref (buff);
    buffer->drop = TRUE;
  } else {
    int stride;
    buffer_handle_t handle;
    void *data = NULL;
    GstVideoSink *vsink = GST_VIDEO_SINK (sink);
    GstNativeBuffer *native;

    handle = gst_droid_egl_sink_alloc_handle (sink, &stride);
    if (!handle) {
      GST_ELEMENT_ERROR (sink, LIBRARY, FAILED,
          ("Could not allocate native buffer handle"), (NULL));
      return NULL;
    }

    if (sink->gralloc->gralloc->lock (sink->gralloc->gralloc, handle,
            BUFFER_LOCK_USAGE, 0, 0, vsink->width, vsink->height, &data) != 0) {

      gst_droid_egl_sink_destroy_handle (sink, handle, sink->gralloc);

      GST_ELEMENT_ERROR (sink, LIBRARY, FAILED,
          ("Could not lock native buffer handle"), (NULL));

      return NULL;
    }

    memcpy (data, GST_BUFFER_DATA (buff), GST_BUFFER_SIZE (buff));
    sink->gralloc->gralloc->unlock (sink->gralloc->gralloc, handle);
    native =
        gst_native_buffer_new (handle, sink->gralloc, stride,
        BUFFER_ALLOC_USAGE);
    gst_droid_egl_sink_set_native_buffer (buffer, native);
    gst_native_buffer_set_finalize_callback (native,
        gst_droid_egl_sink_recycle_buffer, gst_object_ref (sink));
    buffer->free = FALSE;
    buffer->foreign = FALSE;
  }

  g_ptr_array_add (sink->buffers, buffer);

  return buffer;
}
