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

#include "gsthwcsink.h"
#include <gst/video/video.h>
#include <stdlib.h>
#include <system/graphics.h>
#include "gstnativebuffer.h"

GST_DEBUG_CATEGORY_STATIC (hwcsink_debug);
#define GST_CAT_DEFAULT hwcsink_debug

#define gst_hwc_sink_debug_init(ignored_parameter)					\
  GST_DEBUG_CATEGORY_INIT (hwcsink_debug, "hwcsink", 0, "HW compositor sink element"); \

#define NUM_LAYERS 2

GST_BOILERPLATE_FULL (GstHwcSink, gst_hwc_sink, GstVideoSink,
    GST_TYPE_VIDEO_SINK, gst_hwc_sink_debug_init);

static void gst_hwc_sink_finalize (GObject * object);
static GstFlowReturn gst_hwc_sink_show_frame (GstVideoSink * bsink,
    GstBuffer * buf);
static gboolean gst_hwc_sink_start (GstBaseSink * bsink);
static gboolean gst_hwc_sink_stop (GstBaseSink * bsink);
static gboolean gst_hwc_sink_set_caps (GstBaseSink * bsink, GstCaps * caps);

static gboolean gst_hwc_sink_open_hwc (GstHwcSink * sink);
static void gst_hwc_sink_close_hwc (GstHwcSink * sink);

static gboolean gst_hwc_sink_open_fb (GstHwcSink * sink);
static void gst_hwc_sink_close_fb (GstHwcSink * sink);

static buffer_handle_t gst_hwc_sink_setup_handle (GstHwcSink * sink,
    GstBuffer * buffer);
static void gst_hwc_sink_destroy_handle (GstHwcSink * sink,
    buffer_handle_t handle);

static GstFlowReturn gst_hwc_sink_show_handle (GstHwcSink * sink,
    buffer_handle_t handle);
static void gst_hwc_sink_get_times (GstBaseSink * bsink, GstBuffer * buf,
    GstClockTime * start, GstClockTime * end);
GstFlowReturn gst_hwc_sink_buffer_alloc (GstBaseSink * bsink, guint64 offset,
    guint size, GstCaps * caps, GstBuffer ** buf);
static buffer_handle_t gst_hwc_sink_alloc_handle (GstHwcSink * sink,
    int *stride);

static gboolean gst_hwc_sink_destroy_buffer (void *data,
    GstNativeBuffer * buffer);

#define BUFFER_ALLOC_USAGE GRALLOC_USAGE_SW_READ_NEVER | GRALLOC_USAGE_SW_WRITE_RARELY | GRALLOC_USAGE_HW_COMPOSER | GRALLOC_USAGE_HW_FB | GRALLOC_USAGE_EXTERNAL_DISP

static GstPadTemplate *sink_template;

static void
gst_hwc_sink_base_init (gpointer gclass)
{
  GstElementClass *element_class = GST_ELEMENT_CLASS (gclass);

  GstCaps *caps = gst_caps_new_empty ();
  gst_caps_append_structure (caps,
      gst_structure_new ("video/x-raw-yuv",
          "format", GST_TYPE_FOURCC, GST_MAKE_FOURCC ('Y', 'V', '1', '2'),
          "width", GST_TYPE_INT_RANGE, 1, G_MAXINT,
          "height", GST_TYPE_INT_RANGE, 1, G_MAXINT,
          "framerate", GST_TYPE_FRACTION_RANGE, 0, 1, 100, 1, NULL));
  gst_caps_append_structure (caps,
      gst_structure_new (GST_NATIVE_BUFFER_NAME, NULL));

  sink_template = gst_pad_template_new ("sink",
      GST_PAD_SINK, GST_PAD_ALWAYS, caps);

  gst_element_class_add_pad_template (element_class, sink_template);
  gst_object_unref (sink_template);


  gst_element_class_set_details_simple (element_class,
      "Hardware compositor sink",
      "Sink/Video/Device",
      "Android hardware compositor sink",
      "Mohammed Hassan <mohammed.hassan@jollamobile.com>");
}

static void
gst_hwc_sink_class_init (GstHwcSinkClass * klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;
  GstVideoSinkClass *videosink_class = (GstVideoSinkClass *) klass;
  GstBaseSinkClass *basesink_class = (GstBaseSinkClass *) klass;

  gobject_class->finalize = gst_hwc_sink_finalize;
  videosink_class->show_frame = GST_DEBUG_FUNCPTR (gst_hwc_sink_show_frame);
  basesink_class->start = GST_DEBUG_FUNCPTR (gst_hwc_sink_start);
  basesink_class->stop = GST_DEBUG_FUNCPTR (gst_hwc_sink_stop);
  basesink_class->set_caps = GST_DEBUG_FUNCPTR (gst_hwc_sink_set_caps);
  basesink_class->get_times = GST_DEBUG_FUNCPTR (gst_hwc_sink_get_times);
  basesink_class->buffer_alloc = GST_DEBUG_FUNCPTR (gst_hwc_sink_buffer_alloc);
}

static void
gst_hwc_sink_init (GstHwcSink * sink, GstHwcSinkClass * gclass)
{
  sink->gralloc = NULL;
  sink->fb = NULL;
  sink->hwc = NULL;
  sink->hwc_display = NULL;

  GST_OBJECT_FLAG_SET (sink, GST_ELEMENT_IS_SINK);
}

static void
gst_hwc_sink_finalize (GObject * object)
{
  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static buffer_handle_t
gst_hwc_sink_alloc_handle (GstHwcSink * sink, int *stride)
{
  int format = HAL_PIXEL_FORMAT_YV12;

  GstVideoSink *vsink = GST_VIDEO_SINK (sink);

  buffer_handle_t handle =
      gst_gralloc_allocate (sink->gralloc, vsink->width, vsink->height,
      format, BUFFER_ALLOC_USAGE, stride);

  if (!handle) {
    GST_ELEMENT_ERROR (sink, LIBRARY, FAILED,
        ("Could not allocate native buffer handle"), (NULL));
    return NULL;
  }

  return handle;
}

static GstFlowReturn
gst_hwc_sink_show_frame (GstVideoSink * bsink, GstBuffer * buf)
{
  GstHwcSink *sink = GST_HWC_SINK (bsink);
  GstFlowReturn ret;
  GstStructure *s;
  buffer_handle_t handle;

  GST_DEBUG_OBJECT (sink, "show frame");

  if (GST_IS_NATIVE_BUFFER (buf)) {
    GST_DEBUG_OBJECT (sink, "got native buffer");
    return gst_hwc_sink_show_handle (sink,
        gst_native_buffer_get_handle (GST_NATIVE_BUFFER (buf)));
  }

  s = gst_caps_get_structure (buf->caps, 0);
  if (!strcmp (gst_structure_get_name (s), GST_NATIVE_BUFFER_NAME)
      && GST_BUFFER_SIZE (buf) == sizeof (buffer_handle_t)
      && GST_BUFFER_DATA (buf)) {
    handle = (void *) GST_BUFFER_DATA (buf);
    return gst_hwc_sink_show_handle (sink, handle);
  }

  handle = gst_hwc_sink_setup_handle (sink, buf);
  if (!handle) {
    return GST_FLOW_ERROR;
  }

  ret = gst_hwc_sink_show_handle (sink, handle);

  gst_hwc_sink_destroy_handle (sink, handle);

  return ret;
}

static gboolean
gst_hwc_sink_start (GstBaseSink * bsink)
{
  GstHwcSink *sink = GST_HWC_SINK (bsink);

  sink->gralloc = gst_gralloc_new ();
  if (!sink->gralloc) {
    GST_ELEMENT_ERROR (sink, LIBRARY, INIT, ("Could not initialize gralloc"),
        (NULL));
    return FALSE;
  }

  if (!gst_hwc_sink_open_fb (sink)) {
    return FALSE;
  }

  if (!gst_hwc_sink_open_hwc (sink)) {
    gst_hwc_sink_close_fb (sink);
    return FALSE;
  }

  return TRUE;
}

static gboolean
gst_hwc_sink_stop (GstBaseSink * bsink)
{
  GstHwcSink *sink = GST_HWC_SINK (bsink);

  gst_hwc_sink_close_hwc (sink);

  gst_hwc_sink_close_fb (sink);

  if (sink->gralloc) {
    gst_gralloc_unref (sink->gralloc);
    sink->gralloc = NULL;
  }

  return TRUE;
}

static gboolean
gst_hwc_sink_open_hwc (GstHwcSink * sink)
{
  hw_module_t *hwmod;
  int err = 0;

  GST_DEBUG_OBJECT (sink, "open hwc");

  err = hw_get_module (HWC_HARDWARE_MODULE_ID, (const hw_module_t **) &hwmod);
  if (err != 0) {
    GST_ELEMENT_ERROR (sink, LIBRARY, INIT, ("Could not load hwc: %s",
            strerror (-err)), (NULL));
    gst_hwc_sink_close_hwc (sink);
    return FALSE;
  }

  err = hwc_open_1 (hwmod, &sink->hwc);
  if (err != 0) {
    GST_ELEMENT_ERROR (sink, LIBRARY, INIT, ("Could not open hwc: %s",
            strerror (-err)), (NULL));
    gst_hwc_sink_close_hwc (sink);
    return FALSE;
  }

  err = sink->hwc->blank (sink->hwc, 0, 0);
  if (err != 0) {
    GST_ELEMENT_WARNING (sink, LIBRARY, INIT, ("Could not unblank screen: %s",
            strerror (-err)), (NULL));
  }

  ssize_t size = sizeof (hwc_display_contents_1_t)
      + (NUM_LAYERS * sizeof (hwc_layer_1_t));

  sink->hwc_display = malloc (size);
  memset (sink->hwc_display, 0x0, size);

  sink->hwc_display->flags = 0;
  sink->hwc_display->retireFenceFd = -1;
  sink->hwc_display->numHwLayers = NUM_LAYERS;

  int x;
  for (x = 0; x < NUM_LAYERS; x++) {
    hwc_layer_1_t *layer = &sink->hwc_display->hwLayers[x];

    layer->compositionType = HWC_FRAMEBUFFER_TARGET;
    layer->hints = 0;
    layer->flags = 0;
    layer->handle = 0;
    layer->transform = HWC_TRANSFORM_ROT_90;
    layer->blending = HWC_BLENDING_NONE;
    // sourceCrop will be reset anyway
    layer->sourceCrop = sink->src_rect;
    layer->displayFrame = sink->screen_rect;
    layer->visibleRegionScreen.numRects = 1;
    layer->visibleRegionScreen.rects = &layer->displayFrame;
    layer->acquireFenceFd = -1;
    layer->releaseFenceFd = -1;
  }

  GST_DEBUG_OBJECT (sink, "opened hwc");

  return TRUE;
}

static void
gst_hwc_sink_close_hwc (GstHwcSink * sink)
{
  GST_DEBUG_OBJECT (sink, "close hwc");

  if (sink->hwc) {
    hwc_close_1 (sink->hwc);
  }

  if (sink->hwc_display) {
    free (sink->hwc_display);
  }

  sink->hwc = NULL;
  sink->hwc_display = NULL;

  GST_DEBUG_OBJECT (sink, "closed hwc");
}

static gboolean
gst_hwc_sink_open_fb (GstHwcSink * sink)
{
  int err = 0;

  GST_DEBUG_OBJECT (sink, "open fb");

  err = framebuffer_open ((hw_module_t *) sink->gralloc->gralloc, &sink->fb);
  if (err != 0) {
    GST_ELEMENT_ERROR (sink, LIBRARY, INIT, ("Could not open fb: %s",
            strerror (-err)), (NULL));
    return FALSE;
  }

  sink->screen_rect.left = 0;
  sink->screen_rect.top = 0;
  sink->screen_rect.right = sink->fb->width;
  sink->screen_rect.bottom = sink->fb->height;

  GST_DEBUG_OBJECT (sink, "opened fb");

  return TRUE;
}

static void
gst_hwc_sink_close_fb (GstHwcSink * sink)
{
  GST_DEBUG_OBJECT (sink, "close fb");

  if (sink->fb) {
    framebuffer_close (sink->fb);
    sink->fb = NULL;
  }

  GST_DEBUG_OBJECT (sink, "closed fb");
}

static gboolean
gst_hwc_sink_set_caps (GstBaseSink * bsink, GstCaps * caps)
{
  int width, height;
  int fps_n, fps_d;
  GstVideoFormat format = GST_VIDEO_FORMAT_UNKNOWN;

  GstHwcSink *sink = GST_HWC_SINK (bsink);
  GstVideoSink *vsink = GST_VIDEO_SINK (bsink);

  GstStructure *s = gst_caps_get_structure (caps, 0);

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

  if (!strcmp (gst_structure_get_name (s), GST_NATIVE_BUFFER_NAME)) {
    /* Nothing */
  } else if (format != GST_VIDEO_FORMAT_YV12) {
    GST_ELEMENT_ERROR (sink, STREAM, FORMAT, ("Can only handle YV12"), (NULL));
    return FALSE;
  }

  vsink->width = width;
  vsink->height = height;

  sink->src_rect.left = 0;
  sink->src_rect.top = 0;
  sink->src_rect.right = width;
  sink->src_rect.bottom = height;

  sink->hwc_display->flags = HWC_GEOMETRY_CHANGED;

  sink->fps_n = fps_n;
  sink->fps_d = fps_d;

  int x;
  for (x = 0; x < NUM_LAYERS; x++) {
    hwc_layer_1_t *layer = &sink->hwc_display->hwLayers[x];
    layer->sourceCrop = sink->src_rect;
  }

  return TRUE;
}

static buffer_handle_t
gst_hwc_sink_setup_handle (GstHwcSink * sink, GstBuffer * buffer)
{
  GstVideoSink *vsink = GST_VIDEO_SINK (sink);
  int stride;
  buffer_handle_t handle = gst_hwc_sink_alloc_handle (sink, &stride);

  if (!handle) {
    return NULL;
  }

  void *addr = NULL;

  int err = sink->gralloc->gralloc->lock (sink->gralloc->gralloc, handle,
      GRALLOC_USAGE_SW_WRITE_RARELY,
      0, 0, vsink->width, vsink->height, &addr);
  if (err != 0) {
    GST_ELEMENT_ERROR (sink, LIBRARY, FAILED, ("Could not lock buffer: %s",
            strerror (-err)), (NULL));
    gst_hwc_sink_destroy_handle (sink, handle);
    return NULL;
  }

  memcpy (addr, GST_BUFFER_DATA (buffer), GST_BUFFER_SIZE (buffer));

  err = sink->gralloc->gralloc->unlock (sink->gralloc->gralloc, handle);
  if (err != 0) {
    GST_ELEMENT_ERROR (sink, LIBRARY, FAILED,
        ("Could not unlock buffer: %s", strerror (-err)), (NULL));
    gst_hwc_sink_destroy_handle (sink, handle);
    return NULL;
  }

  return handle;
}

static void
gst_hwc_sink_destroy_handle (GstHwcSink * sink, buffer_handle_t handle)
{
  gst_gralloc_free (sink->gralloc, handle);
}

static GstFlowReturn
gst_hwc_sink_show_handle (GstHwcSink * sink, buffer_handle_t handle)
{
  int x;

  for (x = 0; x < NUM_LAYERS; x++) {
    hwc_layer_1_t *layer = &sink->hwc_display->hwLayers[x];
    layer->handle = handle;
  }

  int err = sink->hwc->prepare (sink->hwc, 1, &sink->hwc_display);
  if (err != 0) {
    GST_ELEMENT_ERROR (sink, LIBRARY, FAILED, ("Could not prepare buffer: %s",
            strerror (-err)), (NULL));

    return GST_FLOW_ERROR;
  }
  // Reset flag
  sink->hwc_display->flags = 0;

  err = sink->hwc->set (sink->hwc, 1, &sink->hwc_display);
  if (err != 0) {
    GST_ELEMENT_ERROR (sink, LIBRARY, FAILED, ("Could not render buffer: %s",
            strerror (-err)), (NULL));

    return GST_FLOW_ERROR;
  }

  return GST_FLOW_OK;
}

static void
gst_hwc_sink_get_times (GstBaseSink * bsink, GstBuffer * buf,
    GstClockTime * start, GstClockTime * end)
{
  GstHwcSink *sink = GST_HWC_SINK (bsink);

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
gst_hwc_sink_buffer_alloc (GstBaseSink * bsink, guint64 offset, guint size,
    GstCaps * caps, GstBuffer ** buf)
{
  int stride;
  buffer_handle_t handle;
  GstHwcSink *sink = GST_HWC_SINK (bsink);
  GstVideoSink *vsink = GST_VIDEO_SINK (bsink);

  /* TODO: We need to add format and usage to the sink caps. */

  GST_DEBUG_OBJECT (sink, "buffer alloc");

  if (!sink->gralloc || !sink->fb || !sink->hwc || !sink->hwc_display) {
    return GST_FLOW_WRONG_STATE;
  }

  if (!gst_hwc_sink_set_caps (GST_BASE_SINK (sink), caps)) {
    return GST_FLOW_ERROR;
  }

  handle = gst_hwc_sink_alloc_handle (sink, &stride);

  if (!handle) {
    return GST_FLOW_ERROR;
  }
  // TODO: We are passing 0 as the format
  GstNativeBuffer *buffer =
      gst_native_buffer_new (handle, sink->gralloc, vsink->width, vsink->height,
      stride, BUFFER_ALLOC_USAGE, 0);
  gst_native_buffer_set_finalize_callback (buffer, gst_hwc_sink_destroy_buffer,
      gst_object_ref (sink));

  *buf = GST_BUFFER (buffer);

  return GST_FLOW_OK;
}

static gboolean
gst_hwc_sink_destroy_buffer (void *data, GstNativeBuffer * buffer)
{
  /* TODO: cache and reuse the buffer. */
  GstHwcSink *sink = (GstHwcSink *) data;

  gst_hwc_sink_destroy_handle (sink, gst_native_buffer_get_handle (buffer));

  gst_object_unref (sink);

  return FALSE;
}
