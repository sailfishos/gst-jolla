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
#include <gst/interfaces/meegovideotexture.h>
#include <GLES2/gl2ext.h>
#include <EGL/eglext.h>

#define EGL_NATIVE_BUFFER_ANDROID 0x3140
#define EGL_IMAGE_PRESERVED_KHR   0x30D2
#define HAL_PIXEL_FORMAT_YCbCr_420_SP 0x109

#define BUFFER_LOCK_USAGE GRALLOC_USAGE_SW_READ_RARELY | GRALLOC_USAGE_SW_WRITE_OFTEN
#define BUFFER_ALLOC_USAGE GRALLOC_USAGE_SW_READ_OFTEN | GRALLOC_USAGE_SW_WRITE_OFTEN | GRALLOC_USAGE_HW_TEXTURE

GST_DEBUG_CATEGORY_STATIC (droideglsink_debug);
#define GST_CAT_DEFAULT droideglsink_debug

static GstStaticPadTemplate sink_template = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (GST_VIDEO_CAPS_YUV ("{ YV12, NV21, NV12 }")));

static void gst_droid_egl_sink_finalize (GObject * object);
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
gst_droid_egl_sink_videotexture_interface_init (MeegoGstVideoTextureClass *
    iface);
static gboolean gst_droid_egl_sink_interfaces_supported (GstDroidEglSink * sink,
    GType type);
static gboolean gst_droid_egl_sink_acquire_frame (MeegoGstVideoTexture * bsink,
    gint frame);
static gboolean gst_droid_egl_sink_bind_frame (MeegoGstVideoTexture * bsink,
    gint target, gint dontcare);
static void gst_droid_egl_sink_release_frame (MeegoGstVideoTexture * bsink,
    int dontcare, gpointer sync);

static int gst_droid_egl_sink_find_buffer_unlocked (GstDroidEglSink * sink,
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
  videosink_class->show_frame =
      GST_DEBUG_FUNCPTR (gst_droid_egl_sink_show_frame);
  basesink_class->start = GST_DEBUG_FUNCPTR (gst_droid_egl_sink_start);
  basesink_class->stop = GST_DEBUG_FUNCPTR (gst_droid_egl_sink_stop);
  basesink_class->set_caps = GST_DEBUG_FUNCPTR (gst_droid_egl_sink_set_caps);
  basesink_class->get_times = GST_DEBUG_FUNCPTR (gst_droid_egl_sink_get_times);
  basesink_class->buffer_alloc =
      GST_DEBUG_FUNCPTR (gst_droid_egl_sink_buffer_alloc);
}

static void
gst_droid_egl_sink_init (GstDroidEglSink * sink, GstDroidEglSinkClass * gclass)
{
  GST_OBJECT_FLAG_SET (sink, GST_ELEMENT_IS_SINK);

  gst_base_sink_set_last_buffer_enabled (GST_BASE_SINK (sink), FALSE);

  sink->gralloc = NULL;
  sink->format = GST_VIDEO_FORMAT_UNKNOWN;
  sink->hal_format = 0;

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

static GstFlowReturn
gst_droid_egl_sink_show_frame (GstVideoSink * bsink, GstBuffer * buf)
{
  int x;
  GstDroidEglSink *sink = GST_DROID_EGL_SINK (bsink);
  GstDroidEglBuffer *old_buffer;
  GstDroidEglBuffer *buffer;

  GST_DEBUG_OBJECT (sink, "show frame");

  g_mutex_lock (&sink->buffer_lock);

  x = gst_droid_egl_sink_find_buffer_unlocked (sink, buf);
  g_assert (x != -1);

  buffer = sink->buffers->pdata[x];

  old_buffer = sink->last_buffer;
  sink->last_buffer = buffer;
  gst_buffer_ref (GST_BUFFER (sink->last_buffer->buff));

  g_mutex_unlock (&sink->buffer_lock);

  if (old_buffer) {
    gst_buffer_unref (GST_BUFFER (old_buffer->buff));
  }

  meego_gst_video_texture_frame_ready (MEEGO_GST_VIDEO_TEXTURE (sink), x);

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

  GstDroidEglSink *sink = GST_DROID_EGL_SINK (bsink);
  GstVideoSink *vsink = GST_VIDEO_SINK (bsink);

  if (caps
      && gst_caps_is_equal (caps, GST_PAD_CAPS (GST_BASE_SINK_PAD (bsink)))) {
    GST_LOG_OBJECT (sink, "same caps");
    return TRUE;
  }

  GST_DEBUG_OBJECT (sink, "set caps %" GST_PTR_FORMAT, caps);

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
    GST_ELEMENT_ERROR (sink, STREAM, FORMAT, ("Unsupported color format 0x%x",
            format), (NULL));
    return FALSE;
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
    int err = sink->gralloc->gralloc->lock (sink->gralloc->gralloc,
        buffer->buff->handle, BUFFER_LOCK_USAGE, 0, 0, vsink->width,
        vsink->height, &data);
    if (err != 0) {

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

      GST_BUFFER_DATA (buffer->buff) = data;
      GST_BUFFER_SIZE (buffer->buff) =
          gst_video_format_get_size (sink->format, vsink->width, vsink->height);

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
  int x;

  GST_DEBUG_OBJECT (sink, "destroy buffer");

  g_mutex_lock (&sink->buffer_lock);

  x = gst_droid_egl_sink_find_buffer_unlocked (sink, GST_BUFFER (buffer));
  g_assert (x != -1);

  buff = sink->buffers->pdata[x];
  g_mutex_unlock (&sink->buffer_lock);

  if (buff->locked) {
    buffer->gralloc->gralloc->unlock (buffer->gralloc->gralloc, buffer->handle);
    buff->locked = FALSE;
  }

  if (buff->drop) {
    g_mutex_lock (&sink->buffer_lock);
    g_ptr_array_remove (sink->buffers, buff);
    g_mutex_unlock (&sink->buffer_lock);
    gst_droid_egl_sink_destroy_buffer (sink, buff);

    return FALSE;
  } else {
    g_mutex_lock (&sink->buffer_lock);

    buff->free = TRUE;

    gst_buffer_ref (GST_BUFFER (buff->buff));

    g_mutex_unlock (&sink->buffer_lock);

    return TRUE;
  }
}

static int
gst_droid_egl_sink_find_buffer_unlocked (GstDroidEglSink * sink,
    GstBuffer * buffer)
{
  int x;
  GstDroidEglBuffer *buff;

  for (x = 0; x < sink->buffers->len; x++) {
    buff = sink->buffers->pdata[x];
    if (GST_BUFFER (buff->buff) == buffer) {
      return x;
    }
  }

  return -1;
}

static void
gst_droid_egl_sink_do_init (GType type)
{
  GST_DEBUG_CATEGORY_INIT (droideglsink_debug, "droidegl", 0,
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

  g_type_add_interface_static (type, MEEGO_GST_TYPE_VIDEO_TEXTURE,
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
gst_droid_egl_sink_videotexture_interface_init (MeegoGstVideoTextureClass *
    iface)
{
  iface->acquire_frame = gst_droid_egl_sink_acquire_frame;
  iface->bind_frame = gst_droid_egl_sink_bind_frame;
  iface->release_frame = gst_droid_egl_sink_release_frame;
}

static gboolean
gst_droid_egl_sink_interfaces_supported (GstDroidEglSink * sink, GType type)
{
  return (type == MEEGO_GST_TYPE_VIDEO_TEXTURE);
}

static gboolean
gst_droid_egl_sink_acquire_frame (MeegoGstVideoTexture * bsink, gint frame)
{
  GstDroidEglSink *sink = GST_DROID_EGL_SINK (bsink);
  gboolean ret;

  GST_DEBUG_OBJECT (sink, "acquire frame");

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
gst_droid_egl_sink_bind_frame (MeegoGstVideoTexture * bsink, gint target,
    gint dontcare)
{
  GstDroidEglSink *sink = GST_DROID_EGL_SINK (bsink);
  GstDroidEglBuffer *buffer;

  GST_DEBUG_OBJECT (sink, "bind frame: 0x%x %d", target, dontcare);

  if (dontcare == -1) {
    glBindTexture (target, 0);
    return TRUE;
  }

  buffer = sink->acquired_buffer;
  if (buffer->locked) {
    buffer->buff->gralloc->gralloc->unlock (buffer->buff->gralloc->gralloc,
        buffer->buff->handle);
    buffer->locked = FALSE;
  }

  if (!buffer->texture) {
    EGLint eglImgAttrs[] =
        { EGL_IMAGE_PRESERVED_KHR, EGL_TRUE, EGL_NONE, EGL_NONE };

    glGenTextures (1, &buffer->texture);
    glBindTexture (target, buffer->texture);
    glTexParameteri (target, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri (target, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    if (!sink->eglCreateImageKHR) {
      sink->eglCreateImageKHR =
          (PFNEGLCREATEIMAGEKHRPROC) eglGetProcAddress ("eglCreateImageKHR");
    }

    buffer->image =
        sink->eglCreateImageKHR (eglGetDisplay (EGL_DEFAULT_DISPLAY),
        EGL_NO_CONTEXT, EGL_NATIVE_BUFFER_ANDROID,
        (EGLClientBuffer) buffer->native, eglImgAttrs);

    if (!sink->glEGLImageTargetTexture2DOES) {
      sink->glEGLImageTargetTexture2DOES = (PFNGLEGLIMAGETARGETTEXTURE2DOESPROC)
          eglGetProcAddress ("glEGLImageTargetTexture2DOES");
    }

    sink->glEGLImageTargetTexture2DOES (target, buffer->image);
  } else {
    glBindTexture (target, buffer->texture);
  }

  return TRUE;
}

static void
gst_droid_egl_sink_release_frame (MeegoGstVideoTexture * bsink, int dontcare,
    gpointer sync)
{
  GstDroidEglSink *sink = GST_DROID_EGL_SINK (bsink);

  GST_DEBUG_OBJECT (sink, "release frame");

  g_mutex_lock (&sink->buffer_lock);

  sink->acquired_buffer->acquired = FALSE;
  gst_buffer_unref (GST_BUFFER (sink->acquired_buffer->buff));

  sink->acquired_buffer = NULL;

  g_mutex_unlock (&sink->buffer_lock);
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

}

static void
gst_droid_egl_sink_native_buffer_unref (struct android_native_base_t *base)
{

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
gst_droid_egl_sink_alloc_buffer (GstDroidEglSink * sink, buffer_handle_t handle,
    int stride)
{
  GstVideoSink *vsink = GST_VIDEO_SINK (sink);
  GstDroidEglBuffer *buffer;

  GST_DEBUG_OBJECT (sink, "alloc buffer");

  buffer = g_malloc (sizeof (GstDroidEglBuffer));
  buffer->native = g_malloc (sizeof (struct ANativeWindowBuffer));
  buffer->native->common.magic = ANDROID_NATIVE_BUFFER_MAGIC;
  buffer->native->common.version = sizeof (struct ANativeWindowBuffer);
  buffer->native->width = vsink->width;
  buffer->native->height = vsink->height;
  buffer->native->stride = stride;
  buffer->native->handle = handle;
  buffer->native->format = sink->hal_format;
  buffer->native->usage = BUFFER_ALLOC_USAGE;

  buffer->native->common.incRef = gst_droid_egl_sink_native_buffer_ref;
  buffer->native->common.decRef = gst_droid_egl_sink_native_buffer_unref;

  memset (buffer->native->common.reserved, 0,
      sizeof (buffer->native->common.reserved));

  buffer->buff =
      gst_native_buffer_new (handle, sink->gralloc, stride, BUFFER_ALLOC_USAGE);
  buffer->buff->finalize_callback_data = gst_object_ref (sink);
  buffer->buff->finalize_callback = gst_droid_egl_sink_recycle_buffer;

  buffer->texture = 0;
  buffer->image = EGL_NO_IMAGE_KHR;
  buffer->free = FALSE;
  buffer->locked = TRUE;
  buffer->acquired = FALSE;
  buffer->drop = FALSE;

  return buffer;
}

static void
gst_droid_egl_sink_destroy_buffer (GstDroidEglSink *
    sink, GstDroidEglBuffer * buffer)
{
  GST_DEBUG_OBJECT (sink, "destroy buffer");

  if (buffer->texture) {
    // TODO:
  }

  if (buffer->image) {
    // TODO:
  }

  g_free (buffer->native);
  g_free (buffer);
}
