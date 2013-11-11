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

GST_DEBUG_CATEGORY_STATIC (droideglsink_debug);
#define GST_CAT_DEFAULT droideglsink_debug

#define IS_NATIVE_CAPS(x) (strcmp(gst_structure_get_name (gst_caps_get_structure (x, 0)), GST_NATIVE_BUFFER_NAME) == 0)

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
static GstFlowReturn gst_droid_egl_sink_show_frame (GstVideoSink * vsink,
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
static gboolean gst_droid_egl_sink_free_buffer (void *data,
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
static gboolean gst_droid_egl_sink_get_frame_info (NemoGstVideoTexture * bsink,
    NemoGstVideoTextureFrameInfo * info);

static GstNativeBuffer
    * gst_droid_egl_sink_find_free_buffer_unlocked (GstDroidEglSink * sink);

static int gst_droid_egl_sink_get_hal_format (GstVideoFormat format);
static gboolean gst_droid_egl_sink_event (GstBaseSink * bsink,
    GstEvent * event);

static GstNativeBuffer *gst_droid_egl_sink_alloc_buffer (GstDroidEglSink * sink,
    GstCaps * caps);
static gboolean gst_droid_egl_sink_wait_and_destroy_sync (GstDroidEglSink *
    sink, EGLSyncKHR sync);
static gboolean gst_droid_egl_sink_destroy_sync (GstDroidEglSink * sink,
    EGLSyncKHR sync);

static GstNativeBuffer *gst_droid_egl_sink_copy_buffer (GstDroidEglSink * sink,
    GstBuffer * buf);
static void gst_droid_egl_sink_destroy_buffer (GstNativeBuffer * buffer,
    GstDroidEglSink * sink);

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

  GST_DEBUG_REGISTER_FUNCPTR (gst_droid_egl_sink_show_frame);
  GST_DEBUG_REGISTER_FUNCPTR (gst_droid_egl_sink_start);
  GST_DEBUG_REGISTER_FUNCPTR (gst_droid_egl_sink_stop);
  GST_DEBUG_REGISTER_FUNCPTR (gst_droid_egl_sink_set_caps);
  GST_DEBUG_REGISTER_FUNCPTR (gst_droid_egl_sink_get_times);
  GST_DEBUG_REGISTER_FUNCPTR (gst_droid_egl_sink_buffer_alloc);
  GST_DEBUG_REGISTER_FUNCPTR (gst_droid_egl_sink_event);

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
  sink->sync = EGL_NO_SYNC_KHR;
  sink->image = EGL_NO_IMAGE_KHR;

  g_mutex_init (&sink->buffer_lock);

  sink->buffers = g_ptr_array_new ();
  sink->free_buffers = g_queue_new ();
}

static void
gst_droid_egl_sink_destroy_buffer (GstNativeBuffer * buffer,
    GstDroidEglSink * sink)
{
  GST_DEBUG_OBJECT (sink, "destroy buffer");

  g_ptr_array_remove (sink->buffers, buffer);

  gst_native_buffer_set_finalize_callback (buffer, NULL, NULL);

  gst_droid_egl_sink_destroy_handle (sink,
      *gst_native_buffer_get_handle (buffer),
      gst_native_buffer_get_gralloc (buffer));

  gst_buffer_unref (GST_BUFFER (buffer));
}

static void
gst_droid_egl_sink_finalize (GObject * object)
{
  GstDroidEglSink *sink = GST_DROID_EGL_SINK (object);

  g_mutex_lock (&sink->buffer_lock);
  g_queue_foreach (sink->free_buffers,
      (GFunc) gst_droid_egl_sink_destroy_buffer, sink);
  g_mutex_unlock (&sink->buffer_lock);

  g_queue_free (sink->free_buffers);

  // Unlikely but another element might be still holding a reference to
  // the buffer and we really cannot do anything.
  g_assert (sink->buffers->len == 0);

  g_ptr_array_free (sink->buffers, TRUE);

  g_mutex_clear (&sink->buffer_lock);

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
gst_droid_egl_sink_show_frame (GstVideoSink * vsink, GstBuffer * buf)
{
  GstDroidEglSink *sink = GST_DROID_EGL_SINK (vsink);
  GstNativeBuffer *new_buffer;
  GstNativeBuffer *old_buffer;
  EGLSyncKHR sync = EGL_NO_SYNC_KHR;

  GST_DEBUG_OBJECT (sink, "show frame");

  if (GST_IS_NATIVE_BUFFER (buf)) {
    GST_DEBUG_OBJECT (sink, "using supplied native buffer %p", buf);
    new_buffer = GST_NATIVE_BUFFER (buf);

    if (!gst_native_buffer_unlock (new_buffer)) {
      GST_ELEMENT_ERROR (sink, LIBRARY, FAILED,
          ("Failed to unlock native buffer"), (NULL));

      return GST_FLOW_ERROR;
    }

    gst_buffer_ref (buf);
  } else {
    new_buffer = gst_droid_egl_sink_copy_buffer (sink, buf);
    GST_DEBUG_OBJECT (sink, "using allocated native buffer %p", new_buffer);
    if (!new_buffer) {
      return GST_FLOW_ERROR;
    }
  }

  /* Now we have a buffer */
  g_mutex_lock (&sink->buffer_lock);
  sync = sink->sync;
  sink->sync = EGL_NO_SYNC_KHR;
  g_mutex_unlock (&sink->buffer_lock);

  /* Destroy previous sync */
  gst_droid_egl_sink_wait_and_destroy_sync (sink, sync);

  g_mutex_lock (&sink->buffer_lock);
  old_buffer = sink->last_buffer;
  sink->last_buffer = new_buffer;
  g_mutex_unlock (&sink->buffer_lock);

  if (old_buffer) {
    gst_buffer_unref (GST_BUFFER (old_buffer));
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

  if (sink->sync) {
    gst_droid_egl_sink_wait_and_destroy_sync (sink, sink->sync);
    sink->sync = EGL_NO_SYNC_KHR;
  }

  if (sink->last_buffer) {
    gst_buffer_unref (GST_BUFFER (sink->last_buffer));
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

  if (IS_NATIVE_CAPS (caps)) {
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

  if (!gst_pad_set_caps (GST_BASE_SINK_PAD (bsink), caps)) {
    return FALSE;
  }

  GST_OBJECT_LOCK (sink);

  vsink->width = width;
  vsink->height = height;

  sink->format = format;
  sink->fps_n = fps_n;
  sink->fps_d = fps_d;
  sink->hal_format = hal_format;

  GST_LOG_OBJECT (sink, "hal format: 0x%x", hal_format);

  GST_OBJECT_UNLOCK (sink);

  /* Now let's toss all buffers */
  g_mutex_lock (&sink->buffer_lock);
  while (sink->buffers->len) {
    GstNativeBuffer *buffer =
        g_ptr_array_remove_index_fast (sink->buffers, sink->buffers->len - 1);
    gst_native_buffer_set_finalize_callback (buffer,
        gst_droid_egl_sink_free_buffer, sink);
  }

  while (sink->free_buffers->length) {
    GstNativeBuffer *buffer = g_queue_pop_head (sink->free_buffers);
    gst_buffer_unref (GST_BUFFER (buffer));
  }

  g_mutex_unlock (&sink->buffer_lock);

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
  GstDroidEglSink *sink = GST_DROID_EGL_SINK (bsink);
  GstNativeBuffer *buffer = NULL;
  gboolean is_native = FALSE;

  GST_DEBUG_OBJECT (sink, "buffer alloc");

  GST_LOG_OBJECT (sink, "buffer alloc buffer caps %" GST_PTR_FORMAT, caps);
  GST_LOG_OBJECT (sink, "buffer alloc sink caps %" GST_PTR_FORMAT,
      GST_PAD_CAPS (GST_BASE_SINK_PAD (bsink)));

  g_return_val_if_fail (sink->hal_format != -1, GST_FLOW_ERROR);

  if (!gst_droid_egl_sink_set_caps (bsink, caps)) {
    return GST_FLOW_ERROR;
  }

  g_mutex_lock (&sink->buffer_lock);

  buffer = gst_droid_egl_sink_find_free_buffer_unlocked (sink);
  if (buffer) {
    GST_LOG_OBJECT (sink, "free buffer found %p", buffer);
  }

  g_mutex_unlock (&sink->buffer_lock);

  if (!buffer) {
    buffer = gst_droid_egl_sink_alloc_buffer (sink, caps);
  }

  if (!buffer) {
    return GST_FLOW_ERROR;
  }

  if (IS_NATIVE_CAPS (caps)) {
    is_native = TRUE;
  }

  if (!is_native) {
    if (!gst_native_buffer_lock (buffer, sink->format, BUFFER_LOCK_USAGE)) {

      g_mutex_lock (&sink->buffer_lock);
      gst_droid_egl_sink_destroy_buffer (buffer, sink);
      g_mutex_unlock (&sink->buffer_lock);

      GST_ELEMENT_ERROR (sink, LIBRARY, FAILED,
          ("Could not lock native buffer handle"), (NULL));

      return GST_FLOW_ERROR;
    }
  }

  *buf = GST_BUFFER (buffer);

  gst_buffer_set_caps (*buf, caps);

  GST_DEBUG_OBJECT (sink, "handing buffer %p", buffer);

  return GST_FLOW_OK;
}

/* width object lock */
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

  GST_LOG_OBJECT (sink, "allocated handle %p", handle);

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

  GST_DEBUG_OBJECT (sink, "recycle buffer");

  if (!gst_native_buffer_unlock (buffer)) {
    GST_WARNING_OBJECT (sink, "failed to unlock buffer %p", buffer);

    g_mutex_lock (&sink->buffer_lock);
    g_ptr_array_remove (sink->buffers, buffer);
    g_mutex_unlock (&sink->buffer_lock);

    gst_droid_egl_sink_destroy_handle (sink,
        *gst_native_buffer_get_handle (buffer),
        gst_native_buffer_get_gralloc (buffer));

    return FALSE;
  }

  gst_buffer_ref (GST_BUFFER (buffer));

  g_mutex_lock (&sink->buffer_lock);
  g_queue_push_tail (sink->free_buffers, buffer);
  g_mutex_unlock (&sink->buffer_lock);

  return TRUE;
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
  iface->get_frame_info = gst_droid_egl_sink_get_frame_info;
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

  GST_LOG_OBJECT (sink, "acquire frame");

  g_mutex_lock (&sink->buffer_lock);

  ret = (sink->last_buffer != NULL);

  if (sink->last_buffer) {
    sink->acquired_buffer = sink->last_buffer;
    gst_buffer_ref (GST_BUFFER (sink->last_buffer));
  }

  g_mutex_unlock (&sink->buffer_lock);

  GST_LOG_OBJECT (sink, "acquire frame: %i", ret);

  return ret;
}

static gboolean
gst_droid_egl_sink_bind_frame (NemoGstVideoTexture * bsink, EGLImageKHR * image)
{
  GstDroidEglSink *sink = GST_DROID_EGL_SINK (bsink);
  EGLint eglImgAttrs[] =
      { EGL_IMAGE_PRESERVED_KHR, EGL_TRUE, EGL_NONE, EGL_NONE };

  GST_DEBUG_OBJECT (sink, "bind frame");

  if (sink->dpy == EGL_NO_DISPLAY) {
    return FALSE;
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

  gst_native_buffer_unlock (sink->acquired_buffer);

  sink->image =
      sink->eglCreateImageKHR (sink->dpy,
      EGL_NO_CONTEXT, EGL_NATIVE_BUFFER_ANDROID, (EGLClientBuffer)
      gst_native_buffer_get_native_buffer (sink->acquired_buffer), eglImgAttrs);

  *image = sink->image;

  return TRUE;
}

static void
gst_droid_egl_sink_unbind_frame (NemoGstVideoTexture * bsink)
{
  GstDroidEglSink *sink = GST_DROID_EGL_SINK (bsink);
  GstNativeBuffer *buffer;
  EGLBoolean res = EGL_TRUE;

  GST_DEBUG_OBJECT (sink, "unbind frame");

  buffer = sink->acquired_buffer;

  if (sink->image != EGL_NO_IMAGE_KHR) {
    res = sink->eglDestroyImageKHR (sink->dpy, sink->image);
    sink->image = EGL_NO_IMAGE_KHR;
  }

  if (res != EGL_TRUE) {
    GST_WARNING_OBJECT (sink, "Failed to destroy image for buffer %p", buffer);
  }
}

static void
gst_droid_egl_sink_release_frame (NemoGstVideoTexture * bsink, EGLSyncKHR sync)
{
  GstDroidEglSink *sink = GST_DROID_EGL_SINK (bsink);
  GstNativeBuffer *buffer;
  EGLSyncKHR our_sync = NULL;

  GST_DEBUG_OBJECT (sink, "release frame with sync %p", sync);

  if (sync && sink->dpy == EGL_NO_DISPLAY) {
    GST_WARNING_OBJECT (sink, "fence sync without an EGL display");
  }

  g_mutex_lock (&sink->buffer_lock);
  buffer = sink->acquired_buffer;
  sink->acquired_buffer = NULL;

  if (sync) {
    our_sync = sink->sync;
    sink->sync = sync;
  }

  g_mutex_unlock (&sink->buffer_lock);

  gst_buffer_unref (GST_BUFFER (buffer));

  if (our_sync) {
    gst_droid_egl_sink_destroy_sync (sink, our_sync);
  }
}

static gboolean
gst_droid_egl_sink_get_frame_info (NemoGstVideoTexture * bsink,
    NemoGstVideoTextureFrameInfo * info)
{
  GstDroidEglSink *sink = GST_DROID_EGL_SINK (bsink);

  GST_DEBUG_OBJECT (sink, "get frame info");

  if (!info) {
    return FALSE;
  }

  g_mutex_lock (&sink->buffer_lock);
  if (!sink->acquired_buffer) {
    g_mutex_unlock (&sink->buffer_lock);
    GST_WARNING_OBJECT (sink, "get frame info without acquiring first");
    return FALSE;
  }

  info->timestamp = GST_BUFFER_TIMESTAMP (sink->acquired_buffer);
  info->duration = GST_BUFFER_DURATION (sink->acquired_buffer);
  info->offset = GST_BUFFER_OFFSET (sink->acquired_buffer);
  info->offset_end = GST_BUFFER_OFFSET_END (sink->acquired_buffer);

  g_mutex_unlock (&sink->buffer_lock);

  return TRUE;
}

static GstNativeBuffer *
gst_droid_egl_sink_find_free_buffer_unlocked (GstDroidEglSink * sink)
{
  if (sink->free_buffers->length > 0) {
    return g_queue_pop_head (sink->free_buffers);
  }

  return NULL;
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

static gboolean
gst_droid_egl_sink_event (GstBaseSink * bsink, GstEvent * event)
{
  GstDroidEglSink *sink = GST_DROID_EGL_SINK (bsink);
  GstNativeBuffer *old_buffer;
  EGLSyncKHR sync;
  gboolean handle_event = FALSE;

  GST_DEBUG_OBJECT (sink, "event %" GST_PTR_FORMAT, event);

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_FLUSH_START:
    case GST_EVENT_FLUSH_STOP:
    case GST_EVENT_EOS:
      handle_event = TRUE;
      break;
    default:
      break;
  }

  if (!handle_event) {
    goto out;
  }

  g_mutex_lock (&sink->buffer_lock);
  old_buffer = sink->last_buffer;
  sink->last_buffer = NULL;
  sync = sink->sync;
  sink->sync = EGL_NO_SYNC_KHR;
  g_mutex_unlock (&sink->buffer_lock);

  gst_droid_egl_sink_destroy_sync (sink, sync);
  if (old_buffer) {
    gst_buffer_unref (GST_BUFFER (old_buffer));
  }

  GST_DEBUG_OBJECT (sink, "current buffer %p", sink->acquired_buffer);

  /* We will simply gamble and not touch the acauired_buffer and hope
     application will just release it ASAP. */
  nemo_gst_video_texture_frame_ready (NEMO_GST_VIDEO_TEXTURE (sink), -1);

out:
  if (GST_BASE_SINK_CLASS (parent_class)->event) {
    return GST_BASE_SINK_CLASS (parent_class)->event (bsink, event);
  }

  return TRUE;
}

static GstNativeBuffer *
gst_droid_egl_sink_alloc_buffer (GstDroidEglSink * sink, GstCaps * caps)
{
  GstNativeBuffer *buffer;
  int stride;
  buffer_handle_t handle;
  GstVideoSink *vsink = GST_VIDEO_SINK (sink);

  GST_DEBUG_OBJECT (sink, "alloc buffer");

  if (!sink->gralloc) {
    GST_ELEMENT_ERROR (sink, LIBRARY, FAILED, ("no gralloc"), (NULL));

    return NULL;
  }

  GST_OBJECT_LOCK (sink);

  handle = gst_droid_egl_sink_alloc_handle (sink, &stride);
  if (!handle) {
    GST_OBJECT_UNLOCK (sink);

    GST_ELEMENT_ERROR (sink, LIBRARY, FAILED,
        ("Could not allocate native buffer handle"), (NULL));

    return NULL;
  }

  GST_DEBUG_OBJECT (sink, "allocated handle %p", handle);

  buffer =
      gst_native_buffer_new (handle, sink->gralloc, vsink->width, vsink->height,
      stride, BUFFER_ALLOC_USAGE, sink->hal_format);

  GST_OBJECT_UNLOCK (sink);

  gst_native_buffer_set_finalize_callback (buffer,
      gst_droid_egl_sink_recycle_buffer, sink);

  g_mutex_lock (&sink->buffer_lock);
  g_ptr_array_add (sink->buffers, buffer);
  g_mutex_unlock (&sink->buffer_lock);

  return buffer;
}

static gboolean
gst_droid_egl_sink_wait_and_destroy_sync (GstDroidEglSink * sink,
    EGLSyncKHR sync)
{
  gboolean ret = TRUE;
  EGLint res;

  GST_LOG_OBJECT (sink, "wait and destroy sync");

  if (sync == EGL_NO_SYNC_KHR) {
    GST_DEBUG_OBJECT (sink, "no sync to wait for");
    return TRUE;
  }

  res = sink->eglClientWaitSyncKHR (sink->dpy, sync, 0, EGL_FOREVER_KHR);
  GST_LOG_OBJECT (sink, "waiting for sync returned 0x%x", res);

  /* EGL_TIMEOUT_EXPIRED_KHR is unlikely to be returned because
   * we use EGL_FOREVER_KHR as the timeout */
  if (res == EGL_FALSE) {
    GST_WARNING_OBJECT (sink, "error 0x%x waiting for sync", eglGetError ());
    ret = FALSE;
  }

  if (!gst_droid_egl_sink_destroy_sync (sink, sync)) {
    ret = FALSE;
  }

  return ret;
}

static gboolean
gst_droid_egl_sink_destroy_sync (GstDroidEglSink * sink, EGLSyncKHR sync)
{
  GST_LOG_OBJECT (sink, "destroy sync %p", sync);

  if (sync == EGL_NO_SYNC_KHR) {
    GST_DEBUG_OBJECT (sink, "no sync to destroy");
    return TRUE;
  }

  if (sink->eglDestroySyncKHR (sink->dpy, sync) != EGL_TRUE) {
    GST_WARNING_OBJECT (sink, "Failed to destroy sync object");

    return FALSE;
  }

  return TRUE;
}

static GstNativeBuffer *
gst_droid_egl_sink_copy_buffer (GstDroidEglSink * sink, GstBuffer * buf)
{
  GstNativeBuffer *buffer;

  GST_DEBUG_OBJECT (sink, "copy buffer %p", buf);

  GstFlowReturn ret =
      gst_droid_egl_sink_buffer_alloc (GST_BASE_SINK (sink), 0, 0,
      GST_BUFFER_CAPS (buf),
      (GstBuffer **) & buffer);

  if (ret != GST_FLOW_OK) {
    return NULL;
  }

  /* No need to lock because the buffer should already be locked. */
  memcpy (GST_BUFFER_DATA (GST_BUFFER (buffer)), GST_BUFFER_DATA (buf),
      GST_BUFFER_SIZE (buf));

  if (!gst_native_buffer_unlock (buffer)) {
    GST_ELEMENT_ERROR (sink, LIBRARY, FAILED,
        ("Failed to unlock native buffer"), (NULL));

    gst_buffer_unref (GST_BUFFER (buffer));
    return NULL;
  }

  return buffer;
}

static gboolean
gst_droid_egl_sink_free_buffer (void *data, GstNativeBuffer * buffer)
{
  GstDroidEglSink *sink = (GstDroidEglSink *) data;
  GST_DEBUG_OBJECT (sink, "free buffer");

  gst_droid_egl_sink_destroy_handle (sink,
      *gst_native_buffer_get_handle (buffer),
      gst_native_buffer_get_gralloc (buffer));
  return FALSE;
}
