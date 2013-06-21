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
#include "gstnativebuffer.h"

GST_DEBUG_CATEGORY_STATIC (droideglsink_debug);
#define GST_CAT_DEFAULT droideglsink_debug

#define gst_droid_egl_sink_debug_init(ignored_parameter)					\
  GST_DEBUG_CATEGORY_INIT (droideglsink_debug, "droidegl", 0, "Android native bufer EGL sink"); \

GST_BOILERPLATE_FULL (GstDroidEglSink, gst_droid_egl_sink, GstVideoSink,
    GST_TYPE_VIDEO_SINK, gst_droid_egl_sink_debug_init);

static GstStaticPadTemplate sink_template = GST_STATIC_PAD_TEMPLATE ("sink",
				    GST_PAD_SINK,
				    GST_PAD_ALWAYS,
				    GST_STATIC_CAPS (GST_VIDEO_CAPS_YUV ("{ YV12 }")));


static void gst_droid_egl_sink_finalize (GObject * object);
static GstFlowReturn gst_droid_egl_sink_show_frame (GstVideoSink * bsink,
    GstBuffer * buf);
static gboolean gst_droid_egl_sink_start (GstBaseSink * bsink);
static gboolean gst_droid_egl_sink_stop (GstBaseSink * bsink);
static gboolean gst_droid_egl_sink_set_caps (GstBaseSink * bsink, GstCaps * caps);
static void gst_droid_egl_sink_get_times (GstBaseSink * bsink, GstBuffer * buf,
    GstClockTime * start, GstClockTime * end);
GstFlowReturn gst_droid_egl_sink_buffer_alloc (GstBaseSink * bsink, guint64 offset,
    guint size, GstCaps * caps, GstBuffer ** buf);

static buffer_handle_t gst_droid_egl_sink_alloc_handle (GstDroidEglSink * sink,
    int *stride);
static gboolean gst_droid_egl_sink_destroy_buffer (void *data,
    GstNativeBuffer * buffer);
static void gst_droid_egl_sink_destroy_handle (GstDroidEglSink * sink, buffer_handle_t handle, GstGralloc * gralloc);

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
  videosink_class->show_frame = GST_DEBUG_FUNCPTR (gst_droid_egl_sink_show_frame);
  basesink_class->start = GST_DEBUG_FUNCPTR (gst_droid_egl_sink_start);
  basesink_class->stop = GST_DEBUG_FUNCPTR (gst_droid_egl_sink_stop);
  basesink_class->set_caps = GST_DEBUG_FUNCPTR (gst_droid_egl_sink_set_caps);
  basesink_class->get_times = GST_DEBUG_FUNCPTR (gst_droid_egl_sink_get_times);
  basesink_class->buffer_alloc = GST_DEBUG_FUNCPTR (gst_droid_egl_sink_buffer_alloc);
}

static void
gst_droid_egl_sink_init (GstDroidEglSink * sink, GstDroidEglSinkClass * gclass)
{
  GST_OBJECT_FLAG_SET (sink, GST_ELEMENT_IS_SINK);

  gst_base_sink_set_last_buffer_enabled (GST_BASE_SINK (sink), FALSE);

  sink->gralloc = NULL;
  sink->format = GST_VIDEO_FORMAT_UNKNOWN;

  g_mutex_init (&sink->buffer_lock);
}

static void
gst_droid_egl_sink_finalize (GObject * object)
{
  GstDroidEglSink *sink = GST_DROID_EGL_SINK (object);

  g_mutex_clear (&sink->buffer_lock);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static GstFlowReturn
gst_droid_egl_sink_show_frame (GstVideoSink * bsink, GstBuffer * buf)
{
  GstDroidEglSink *sink = GST_DROID_EGL_SINK (bsink);

  GST_DEBUG_OBJECT (sink, "show frame");

  g_mutex_lock (&sink->buffer_lock);

  if (sink->last_buffer) {
    gst_buffer_unref (sink->last_buffer);
  }

  sink->last_buffer = buf;
  gst_buffer_ref (sink->last_buffer);

  g_mutex_unlock (&sink->buffer_lock);

  return GST_FLOW_OK;
}

static gboolean
gst_droid_egl_sink_start (GstBaseSink * bsink)
{
  GstDroidEglSink *sink = GST_DROID_EGL_SINK (bsink);

  GST_DEBUG_OBJECT (sink, "start");

  sink->last_buffer = NULL;

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
    gst_buffer_unref (sink->last_buffer);
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

  GstDroidEglSink *sink = GST_DROID_EGL_SINK (bsink);
  GstVideoSink *vsink = GST_VIDEO_SINK (bsink);

  if (caps && gst_caps_is_equal (caps, GST_PAD_CAPS (GST_BASE_SINK_PAD (bsink)))) {
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

  if (format != GST_VIDEO_FORMAT_YV12) {
    GST_ELEMENT_ERROR (sink, STREAM, FORMAT, ("Can only handle YV12"), (NULL));
    return FALSE;
  }

  vsink->width = width;
  vsink->height = height;

  sink->format = format;
  sink->fps_n = fps_n;
  sink->fps_d = fps_d;

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
gst_droid_egl_sink_buffer_alloc (GstBaseSink * bsink, guint64 offset, guint size,
    GstCaps * caps, GstBuffer ** buf)
{
  int stride;
  buffer_handle_t handle;
  GstDroidEglSink *sink = GST_DROID_EGL_SINK (bsink);
  void *data = NULL;
  int usage = GRALLOC_USAGE_SW_READ_RARELY | GRALLOC_USAGE_SW_WRITE_OFTEN;
  GstVideoSink *vsink = GST_VIDEO_SINK (sink);

  GST_DEBUG_OBJECT (sink, "buffer alloc");

  if (!sink->gralloc) {
    return GST_FLOW_ERROR;
  }

  if (!gst_droid_egl_sink_set_caps (bsink, caps)) {
    return GST_FLOW_ERROR;
  }

  handle = gst_droid_egl_sink_alloc_handle (sink, &stride);

  if (!handle) {
    GST_ELEMENT_ERROR (sink, LIBRARY, FAILED,
        ("Could not allocate native buffer handle"), (NULL));

    return GST_FLOW_ERROR;
  }

  if (sink->gralloc->gralloc->lock(sink->gralloc->gralloc, handle, usage, 0, 0, vsink->width,
				   vsink->height, &data) != 0) {

    gst_droid_egl_sink_destroy_handle (sink, handle, sink->gralloc);

    GST_ELEMENT_ERROR (sink, LIBRARY, FAILED,
        ("Could not lock native buffer handle"), (NULL));

    return GST_FLOW_ERROR;
  }

  GstNativeBuffer *buffer =
      gst_native_buffer_new (handle, sink->gralloc, stride);
  buffer->finalize_callback_data = gst_object_ref (sink);
  buffer->finalize_callback = gst_droid_egl_sink_destroy_buffer;

  GST_BUFFER_DATA (buffer) = data;
  GST_BUFFER_SIZE (buffer) = gst_video_format_get_size (sink->format, vsink->width, vsink->height);

  *buf = GST_BUFFER (buffer);

  return GST_FLOW_OK;
}

static buffer_handle_t
gst_droid_egl_sink_alloc_handle (GstDroidEglSink * sink,
				 int *stride)
{
  int format = HAL_PIXEL_FORMAT_YV12;
  int usage = GRALLOC_USAGE_SW_READ_NEVER | GRALLOC_USAGE_SW_WRITE_RARELY
      | GRALLOC_USAGE_HW_COMPOSER | GRALLOC_USAGE_HW_FB
      | GRALLOC_USAGE_EXTERNAL_DISP;

  GstVideoSink *vsink = GST_VIDEO_SINK (sink);

  GST_DEBUG_OBJECT (sink, "alloc handle");

  buffer_handle_t handle =
      gst_gralloc_allocate (sink->gralloc, vsink->width, vsink->height,
      format, usage, stride);

  if (!handle) {
    GST_ELEMENT_ERROR (sink, LIBRARY, FAILED,
        ("Could not allocate native buffer handle"), (NULL));
    return NULL;
  }

  return handle;
}

static void
gst_droid_egl_sink_destroy_handle (GstDroidEglSink * sink, buffer_handle_t handle, GstGralloc * gralloc)
{
  GST_DEBUG_OBJECT (sink, "destroy handle");

  gst_gralloc_free (gralloc, handle);
}

static gboolean
gst_droid_egl_sink_destroy_buffer (void *data, GstNativeBuffer * buffer)
{
  /* TODO: cache and reuse the buffer. */
  GstDroidEglSink *sink = (GstDroidEglSink *) data;

  GST_DEBUG_OBJECT (sink, "destroy buffer");

  buffer->gralloc->gralloc->unlock (buffer->gralloc->gralloc, buffer->handle);
  gst_droid_egl_sink_destroy_handle (sink, buffer->handle, buffer->gralloc);

  gst_object_unref (sink);

  return FALSE;
}

