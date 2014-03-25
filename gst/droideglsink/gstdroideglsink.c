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
#include <gst/video/gstvideometa.h>
#include "gst/interfaces/nemovideotexture.h"
#include "gst/anativewindowbuffer/gstanativewindowbufferpool.h"
#include <GLES2/gl2ext.h>
#include <EGL/eglext.h>

#define EGL_NATIVE_BUFFER_ANDROID 0x3140
#define EGL_IMAGE_PRESERVED_KHR   0x30D2

#define BUFFER_ALLOC_USAGE GRALLOC_USAGE_SW_READ_OFTEN | GRALLOC_USAGE_SW_WRITE_OFTEN | GRALLOC_USAGE_HW_TEXTURE

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
    GST_STATIC_CAPS (GST_VIDEO_CAPS_MAKE (GST_A_NATIVE_WINDOW_BUFFER_MEMORY_VIDEO_FORMATS) ";"
        GST_VIDEO_CAPS_MAKE_WITH_FEATURES (GST_CAPS_FEATURE_MEMORY_A_NATIVE_WINDOW_BUFFER,
        GST_A_NATIVE_WINDOW_BUFFER_MEMORY_VIDEO_FORMATS)));

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
static gboolean gst_droid_egl_sink_propose_allocation (GstBaseSink * bsink,
    GstQuery * query);
static void gst_droid_egl_sink_get_times (GstBaseSink * bsink, GstBuffer * buf,
    GstClockTime * start, GstClockTime * end);

static void
gst_droid_egl_sink_videotexture_interface_init (NemoGstVideoTextureInterface *
    iface);
static gboolean gst_droid_egl_sink_acquire_frame (NemoGstVideoTexture * bsink);
static gboolean gst_droid_egl_sink_bind_frame (NemoGstVideoTexture * bsink,
    EGLImageKHR * image);
static void gst_droid_egl_sink_unbind_frame (NemoGstVideoTexture * iface);
static void gst_droid_egl_sink_release_frame (NemoGstVideoTexture * iface,
    EGLSyncKHR sync);
static gboolean gst_droid_egl_sink_get_frame_info (NemoGstVideoTexture * iface,
    NemoGstVideoTextureFrameInfo * info);
static const GstStructure
    * gst_droid_egl_sink_get_frame_qdata (NemoGstVideoTexture * iface,
    const GQuark quark);

static gboolean gst_droid_egl_sink_event (GstBaseSink * bsink,
    GstEvent * event);

static gboolean gst_droid_egl_sink_wait_and_destroy_sync (GstDroidEglSink *
    sink, EGLSyncKHR sync);
static gboolean gst_droid_egl_sink_destroy_sync (GstDroidEglSink * sink,
    EGLSyncKHR sync);

#define gst_droid_egl_sink_parent_class parent_class
G_DEFINE_TYPE_WITH_CODE (GstDroidEglSink,  gst_droid_egl_sink,
    GST_TYPE_VIDEO_SINK,
    G_IMPLEMENT_INTERFACE (NEMO_GST_TYPE_VIDEO_TEXTURE,
        gst_droid_egl_sink_videotexture_interface_init));

static void
gst_droid_egl_sink_class_init (GstDroidEglSinkClass * klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;
  GstVideoSinkClass *videosink_class = (GstVideoSinkClass *) klass;
  GstBaseSinkClass *basesink_class = (GstBaseSinkClass *) klass;
  GstElementClass *element_class = (GstElementClass *)  klass;

  gobject_class->finalize = gst_droid_egl_sink_finalize;
  gobject_class->set_property = gst_droid_egl_sink_set_property;
  gobject_class->get_property = gst_droid_egl_sink_get_property;

  videosink_class->show_frame =
      GST_DEBUG_FUNCPTR (gst_droid_egl_sink_show_frame);
  basesink_class->start = GST_DEBUG_FUNCPTR (gst_droid_egl_sink_start);
  basesink_class->stop = GST_DEBUG_FUNCPTR (gst_droid_egl_sink_stop);
  basesink_class->set_caps = GST_DEBUG_FUNCPTR (gst_droid_egl_sink_set_caps);
  basesink_class->get_times = GST_DEBUG_FUNCPTR (gst_droid_egl_sink_get_times);
  basesink_class->propose_allocation =
      GST_DEBUG_FUNCPTR (gst_droid_egl_sink_propose_allocation);
  basesink_class->event = GST_DEBUG_FUNCPTR (gst_droid_egl_sink_event);

  GST_DEBUG_REGISTER_FUNCPTR (gst_droid_egl_sink_show_frame);
  GST_DEBUG_REGISTER_FUNCPTR (gst_droid_egl_sink_start);
  GST_DEBUG_REGISTER_FUNCPTR (gst_droid_egl_sink_stop);
  GST_DEBUG_REGISTER_FUNCPTR (gst_droid_egl_sink_set_caps);
  GST_DEBUG_REGISTER_FUNCPTR (gst_droid_egl_sink_get_times);
  GST_DEBUG_REGISTER_FUNCPTR (gst_droid_egl_sink_event);

  g_object_class_install_property (gobject_class, PROP_EGL_DISPLAY,
      g_param_spec_pointer ("egl-display",
          "EGL display ",
          "The application provided EGL display to be used for creating EGLImageKHR objects.",
          G_PARAM_READWRITE));

  gst_element_class_set_static_metadata (element_class,
      "Android native buffers EGL sink",
      "Sink/Video/Device",
      "Render Android native buffers via EGL",
      "Mohammed Hassan <mohammed.hassan@jollamobile.com>");

  gst_element_class_add_pad_template (element_class,
      gst_static_pad_template_get (&sink_template));

  GST_DEBUG_CATEGORY_INIT (droideglsink_debug, "droideglsink", 0,
      "Android native bufer EGL sink");
}

static void
gst_droid_egl_sink_init (GstDroidEglSink * sink)
{
  g_object_set (sink, "enable-last-sample", FALSE, NULL);

  sink->pool = NULL;
  sink->caps = NULL;
  sink->dpy = EGL_NO_DISPLAY;
  sink->sync = EGL_NO_SYNC_KHR;
  sink->image = EGL_NO_IMAGE_KHR;
  sink->last_buffer = NULL;
  sink->acquired_buffer = NULL;

  g_mutex_init (&sink->buffer_lock);
}

static void
gst_droid_egl_sink_finalize (GObject * object)
{
  GstDroidEglSink *sink = GST_DROID_EGL_SINK (object);

  gst_object_unref (sink->pool);
  gst_caps_unref (sink->caps);

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
  GstMemory *mem;
  GstBuffer *old_buffer = NULL;
  EGLSyncKHR sync = EGL_NO_SYNC_KHR;
  GstFlowReturn ret;

  GST_DEBUG_OBJECT (sink, "show frame");

  if (gst_buffer_n_memory (buf) != 1
      || !(mem = gst_buffer_peek_memory ((buf), 0))
      || g_strcmp0 (mem->allocator->mem_type,
          GST_A_NATIVE_WINDOW_BUFFER_MEMORY_TYPE) != 0) {
    GstBufferPool *pool;
    GstBuffer *buffer_copy;
    GstVideoFrame src, dest;
    gboolean copied = FALSE;

    g_mutex_lock (&sink->buffer_lock);
    if (!sink->pool) {
      GST_ERROR_OBJECT (sink, "No buffers allocated.");
      return GST_FLOW_ERROR;
    }
    pool = sink->pool;
    gst_object_ref (GST_OBJECT (pool));
    g_mutex_unlock (&sink->buffer_lock);

    ret = gst_buffer_pool_acquire_buffer (pool, &buffer_copy, NULL);
    gst_object_unref (pool);
    if (ret != GST_FLOW_OK) {
      GST_ERROR_OBJECT (sink, "Failed to allocate buffer");
      return ret;
    }

    if (gst_video_frame_map (&dest, &sink->video_info, buffer_copy,
        GST_MAP_WRITE)) {
      if (gst_video_frame_map (&src, &sink->video_info, buf, GST_MAP_READ)) {
        copied = gst_video_frame_copy (&dest, &src);
        gst_video_frame_unmap (&src);
      }
      gst_video_frame_unmap (&dest);
    }

    if (!copied) {
        GST_ERROR_OBJECT (sink, "Failed to copy buffer contents");
        gst_buffer_unref (buffer_copy);
        return GST_FLOW_ERROR;
    }
    buf = buffer_copy;
  } else {
    gst_buffer_ref (buf);
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
  sink->last_buffer = buf;
  g_mutex_unlock (&sink->buffer_lock);

  if (old_buffer) {
    gst_buffer_unref (old_buffer);
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


  return TRUE;
}

static gboolean
gst_droid_egl_sink_stop (GstBaseSink * bsink)
{
  GstDroidEglSink *sink = GST_DROID_EGL_SINK (bsink);

  GST_DEBUG_OBJECT (sink, "stop");

  if (sink->sync) {
    gst_droid_egl_sink_wait_and_destroy_sync (sink, sink->sync);
    sink->sync = EGL_NO_SYNC_KHR;
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
  GstDroidEglSink *sink = GST_DROID_EGL_SINK (bsink);
  GstVideoSink *vsink = GST_VIDEO_SINK (bsink);
  GstVideoInfo video_info;
  GstBufferPool *pool, *old_pool;
  GstStructure *config;

  if (sink->caps && gst_caps_is_equal (caps, sink->caps)) {
    GST_LOG_OBJECT (sink, "same caps");
    return TRUE;
  }

  GST_DEBUG_OBJECT (sink, "set caps %" GST_PTR_FORMAT, caps);

  if (!gst_video_info_from_caps (&video_info, caps)) {
    GST_ELEMENT_ERROR (sink, STREAM, FORMAT, ("Failed to parse caps"), (NULL));
    return FALSE;
  }

  if (video_info.width == 0 || video_info.height == 0) {
    GST_ELEMENT_ERROR (sink, STREAM, FORMAT, ("Caps without width or height"),
        (NULL));
    return FALSE;
  }

  pool = gst_a_native_window_buffer_pool_new ();

  config = gst_buffer_pool_get_config (pool);
  gst_buffer_pool_config_set_params (config, caps, video_info.size, 2, 0);
  gst_structure_set (config, "usage", G_TYPE_INT, BUFFER_ALLOC_USAGE, NULL);
  if (!gst_buffer_pool_set_config (pool, config)) {
    gst_object_unref (pool);
    pool = NULL;

    GST_ERROR_OBJECT (sink, "Failed to set buffer pool configuration");
    if  (video_info.finfo->format != GST_VIDEO_FORMAT_ENCODED)
      return FALSE;
  }

  GST_OBJECT_LOCK (sink);

  g_mutex_lock (&sink->buffer_lock);
  old_pool = sink->pool;
  sink->pool = pool;
  g_mutex_unlock (&sink->buffer_lock);

  vsink->width = video_info.width;
  vsink->height = video_info.height;

  sink->video_info = video_info;

  if (sink->caps)
    gst_caps_unref(sink->caps);
  sink->caps = caps;
  gst_caps_ref (sink->caps);

  GST_OBJECT_UNLOCK (sink);

  if (old_pool) {
    gst_object_unref (old_pool);
  }

  return TRUE;
}

static gboolean
gst_droid_egl_sink_propose_allocation (GstBaseSink * bsink, GstQuery * query)
{
  GstDroidEglSink *sink = GST_DROID_EGL_SINK (bsink);
  GstBufferPool *pool;
  GstStructure *config;
  GstCaps *caps;
  guint size;
  gboolean need_pool;

  gst_query_parse_allocation (query, &caps, &need_pool);

  if (!caps) {
    GST_ERROR_OBJECT (sink, "No query caps");
    return FALSE;
  }

  g_mutex_lock (&sink->buffer_lock);
  pool = sink->pool;
  if (pool)
    gst_object_ref (GST_OBJECT (pool));
  g_mutex_unlock (&sink->buffer_lock);

  if (pool) {
    GstCaps *pool_caps;

    config = gst_buffer_pool_get_config (pool);
    gst_buffer_pool_config_get_params (config, &pool_caps, &size, NULL, NULL);

    if (!gst_caps_is_equal (caps, pool_caps)) {
      GST_DEBUG_OBJECT (sink, "Pool caps don't match %" GST_PTR_FORMAT
          " %" GST_PTR_FORMAT, caps, pool_caps);
      gst_object_unref (pool);
      pool = NULL;
    }
    gst_structure_free (config);
  }

  if (!pool && need_pool) {
    GstVideoInfo video_info;

    if (!gst_video_info_from_caps (&video_info, caps)) {
      GST_ERROR_OBJECT (sink, "Unable to get video caps");
      return FALSE;
    }

    pool = gst_a_native_window_buffer_pool_new ();
    size = video_info.finfo->format == GST_VIDEO_FORMAT_ENCODED
        ? video_info.size : 1;


    config = gst_buffer_pool_get_config (pool);
    gst_buffer_pool_config_set_params (config, caps, size, 2, 0);
    gst_structure_set (config, "usage", G_TYPE_INT, BUFFER_ALLOC_USAGE, NULL);
    if (!gst_buffer_pool_set_config (pool, config)) {
      GST_ERROR_OBJECT (sink, "Failed to set buffer pool configuration");
      gst_object_unref (pool);
      return FALSE;
    }
  }

  if (pool) {
    gst_query_add_allocation_pool (query, pool, size, 2, 0);
    gst_object_unref (pool);
  }

  gst_query_add_allocation_meta (query, GST_VIDEO_META_API_TYPE, NULL);

  GST_DEBUG_OBJECT (sink, "proposed allocation");

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
      if (sink->video_info.fps_n > 0) {
        *end = *start + gst_util_uint64_scale_int (GST_SECOND,
            sink->video_info.fps_d, sink->video_info.fps_n);
      }
    }
  }
}

static void
gst_droid_egl_sink_videotexture_interface_init (NemoGstVideoTextureInterface *
    iface)
{
  iface->acquire_frame = gst_droid_egl_sink_acquire_frame;
  iface->bind_frame = gst_droid_egl_sink_bind_frame;
  iface->unbind_frame = gst_droid_egl_sink_unbind_frame;
  iface->release_frame = gst_droid_egl_sink_release_frame;
  iface->get_frame_info = gst_droid_egl_sink_get_frame_info;
  iface->get_frame_qdata = gst_droid_egl_sink_get_frame_qdata;
}

static gboolean
gst_droid_egl_sink_acquire_frame (NemoGstVideoTexture * bsink)
{
  GstDroidEglSink *sink = GST_DROID_EGL_SINK (bsink);
  GstBuffer *acquired_buffer = NULL;
  gboolean ret;

  GST_LOG_OBJECT (sink, "acquire frame");

  if (sink->dpy == EGL_NO_DISPLAY) {
    return FALSE;
  }

  g_mutex_lock (&sink->buffer_lock);

  ret = (sink->last_buffer != NULL);

  if (sink->last_buffer) {
    acquired_buffer = sink->acquired_buffer;
    sink->acquired_buffer = sink->last_buffer;
    gst_buffer_ref (sink->acquired_buffer);
  }

  g_mutex_unlock (&sink->buffer_lock);

  if (acquired_buffer)
    gst_buffer_unref (acquired_buffer);

  return ret;
}

static gboolean
gst_droid_egl_sink_bind_frame (NemoGstVideoTexture * iface, EGLImageKHR * image)
{
  GstDroidEglSink *sink = GST_DROID_EGL_SINK (iface);
  EGLint eglImgAttrs[] =
      { EGL_IMAGE_PRESERVED_KHR, EGL_TRUE, EGL_NONE, EGL_NONE };
  ANativeWindowBuffer_t *native_buffer;
  GstMemory *mem;

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

  if (gst_buffer_n_memory (sink->acquired_buffer) != 1
      || !(mem = gst_buffer_peek_memory (sink->acquired_buffer, 0))
      || g_strcmp0 (mem->allocator->mem_type,
          GST_A_NATIVE_WINDOW_BUFFER_MEMORY_TYPE) != 0) {
    GST_ERROR_OBJECT (sink, "Frame not allocated by buffer pool");
    return FALSE;
  }

  native_buffer = gst_a_native_window_buffer_memory_get_buffer (mem);

  sink->image =
      sink->eglCreateImageKHR (sink->dpy, EGL_NO_CONTEXT,
          EGL_NATIVE_BUFFER_ANDROID, (EGLClientBuffer) native_buffer,
          eglImgAttrs);

  *image = sink->image;

  return TRUE;
}

static void
gst_droid_egl_sink_unbind_frame (NemoGstVideoTexture * bsink)
{
  GstDroidEglSink *sink = GST_DROID_EGL_SINK (bsink);
  EGLBoolean res = EGL_TRUE;

  GST_DEBUG_OBJECT (sink, "unbind frame");

  if (sink->image != EGL_NO_IMAGE_KHR) {
    res = sink->eglDestroyImageKHR (sink->dpy, sink->image);
    sink->image = EGL_NO_IMAGE_KHR;
  }

  if (res != EGL_TRUE) {
    GST_WARNING_OBJECT (sink, "Failed to destroy image for buffer %p", sink->acquired_buffer);
  }
}

static void
gst_droid_egl_sink_release_frame (NemoGstVideoTexture * bsink, EGLSyncKHR sync)
{
  GstDroidEglSink *sink = GST_DROID_EGL_SINK (bsink);
  GstBuffer *buffer;
  EGLSyncKHR our_sync;

  GST_DEBUG_OBJECT (sink, "release frame with sync %p", sync);

  g_mutex_lock (&sink->buffer_lock);
  buffer = sink->acquired_buffer;
  sink->acquired_buffer = NULL;
  our_sync = sink->sync;
  sink->sync = sync;
  g_mutex_unlock (&sink->buffer_lock);

  gst_buffer_unref (buffer);

  gst_droid_egl_sink_destroy_sync (sink, our_sync);
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

static const GstStructure *
gst_droid_egl_sink_get_frame_qdata (NemoGstVideoTexture * bsink,
    const GQuark quark)
{
  GstDroidEglSink *sink = GST_DROID_EGL_SINK (bsink);
  const GstStructure *qdata = NULL;

  GST_DEBUG_OBJECT (sink, "get frame qdata");

  g_mutex_lock (&sink->buffer_lock);
  if (!sink->acquired_buffer) {
    g_mutex_unlock (&sink->buffer_lock);
    GST_WARNING_OBJECT (sink, "get frame qdata without acquiring first");
    return NULL;
  }

  if (quark == g_quark_from_string ("GstDroidCamSrcCropData")) {
    GstVideoCropMeta *meta = gst_buffer_get_video_crop_meta (
        sink->acquired_buffer);
    if (meta) {
        qdata = gst_structure_new ("GstDroidCamSrcCropData",
            "left", G_TYPE_INT, meta->x,
            "top", G_TYPE_INT, meta->y,
            "right", G_TYPE_INT, meta->x + meta->width,
            "bottom", G_TYPE_INT, meta->y + meta->height,
            NULL);
    }
  }

  g_mutex_unlock (&sink->buffer_lock);

  return qdata;
}

static gboolean
gst_droid_egl_sink_event (GstBaseSink * bsink, GstEvent * event)
{
  GstDroidEglSink *sink = GST_DROID_EGL_SINK (bsink);
  GstBuffer *old_buffer;
  EGLSyncKHR sync;

  GST_DEBUG_OBJECT (sink, "event %" GST_PTR_FORMAT, event);

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_FLUSH_START:
    case GST_EVENT_FLUSH_STOP:
    case GST_EVENT_EOS:
      break;
    default:
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
    gst_buffer_unref (old_buffer);
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
