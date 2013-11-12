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

#include "gstdroidbufferunpack.h"
#include <gst/gstnativebuffer.h>
#include <gst/video/video.h>

GST_DEBUG_CATEGORY_STATIC (droidbufferunpack_debug);
#define GST_CAT_DEFAULT droidbufferunpack_debug

#define gst_droid_buffer_unpack_debug_init(ignored_parameter)                                             \
  GST_DEBUG_CATEGORY_INIT (droidbufferunpack_debug, "droidbufferunpack", 0, "droidbufferunpack element"); \

GST_BOILERPLATE_FULL (GstDroidBufferUnpack, gst_droid_buffer_unpack,
    GstBaseTransform, GST_TYPE_BASE_TRANSFORM,
    gst_droid_buffer_unpack_debug_init);

static GstStaticPadTemplate src_template = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (GST_VIDEO_CAPS_YUV ("{ NV12 }")));

static GstStaticPadTemplate sink_template = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (GST_NATIVE_BUFFER_NAME ","
        "framerate = (fraction) [ 0, MAX ], "
        "width = (int) [ 1, MAX ], " "height = (int) [ 1, MAX ]"));

static void gst_droid_buffer_unpack_finalize (GObject * object);
static GstCaps *gst_droid_buffer_unpack_transform_caps (GstBaseTransform *
    trans, GstPadDirection direction, GstCaps * caps);
//static gboolean gst_droid_buffer_unpack_get_unit_size (GstBaseTransform * trans,
//    GstCaps * caps, guint * size);
static gboolean gst_droid_buffer_unpack_set_caps (GstBaseTransform * trans,
    GstCaps * incaps, GstCaps * outcaps);
static gboolean gst_droid_buffer_unpack_start (GstBaseTransform * trans);
static gboolean gst_droid_buffer_unpack_stop (GstBaseTransform * trans);
static GstFlowReturn gst_droid_buffer_unpack_transform (GstBaseTransform *
    trans, GstBuffer * inbuf, GstBuffer * outbuf);
//static GstFlowReturn gst_droid_buffer_unpack_prepare_output_buffer (GstBaseTransform *
//    trans, GstBuffer * input, gint size, GstCaps * caps, GstBuffer ** buf);
static void gst_droid_buffer_unpack_fixate_caps (GstBaseTransform * trans,
    GstPadDirection direction, GstCaps * caps, GstCaps * othercaps);

static void
gst_droid_buffer_unpack_base_init (gpointer gclass)
{
  GstElementClass *element_class = GST_ELEMENT_CLASS (gclass);

  gst_element_class_set_details_simple (element_class,
      "Unpack Android native buffers",
      "Filter/Converter/Video",
      "Unpack Android native buffers to YUV data",
      "Mohammed Hassan <mohammed.hassan@jolla.com>");

  gst_element_class_add_static_pad_template (element_class, &src_template);
  gst_element_class_add_static_pad_template (element_class, &sink_template);
}

static void
gst_droid_buffer_unpack_class_init (GstDroidBufferUnpackClass * klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;
  GstBaseTransformClass *trans_class = (GstBaseTransformClass *) klass;

  gobject_class->finalize = gst_droid_buffer_unpack_finalize;
  trans_class->transform_caps =
      GST_DEBUG_FUNCPTR (gst_droid_buffer_unpack_transform_caps);
  //  trans_class->get_unit_size = GST_DEBUG_FUNCPTR (gst_droid_buffer_unpack_get_unit_size);
  trans_class->set_caps = GST_DEBUG_FUNCPTR (gst_droid_buffer_unpack_set_caps);
  trans_class->start = GST_DEBUG_FUNCPTR (gst_droid_buffer_unpack_start);
  trans_class->stop = GST_DEBUG_FUNCPTR (gst_droid_buffer_unpack_stop);
  trans_class->transform =
      GST_DEBUG_FUNCPTR (gst_droid_buffer_unpack_transform);
  //  trans_class->prepare_output_buffer =
  //      GST_DEBUG_FUNCPTR (gst_droid_buffer_unpack_prepare_output_buffer);
  trans_class->fixate_caps =
      GST_DEBUG_FUNCPTR (gst_droid_buffer_unpack_fixate_caps);
}

static void
gst_droid_buffer_unpack_init (GstDroidBufferUnpack * conv,
    GstDroidBufferUnpackClass * gclass)
{
  GstBaseTransform *trans = GST_BASE_TRANSFORM (conv);

  gst_base_transform_set_passthrough (trans, FALSE);
  gst_base_transform_set_in_place (trans, FALSE);

  GST_BASE_TRANSFORM_CLASS (gclass)->passthrough_on_same_caps = FALSE;
}

static void
gst_droid_buffer_unpack_finalize (GObject * object)
{
  GstDroidBufferUnpack *conv = GST_DROID_BUFFER_UNPACK (object);

  GST_DEBUG_OBJECT (conv, "finalize");

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static GstCaps *
gst_droid_buffer_unpack_transform_caps (GstBaseTransform * trans,
    GstPadDirection direction, GstCaps * caps)
{
  GstCaps *out_caps = NULL;

  GST_DEBUG_OBJECT (trans, "transform caps %" GST_PTR_FORMAT, caps);

  switch (direction) {
    case GST_PAD_SRC:
      out_caps = gst_caps_make_writable (gst_static_pad_template_get_caps
          (&sink_template));
      break;

    case GST_PAD_SINK:
      out_caps = gst_caps_make_writable (gst_static_pad_template_get_caps
          (&src_template));
      break;

    default:
      GST_WARNING_OBJECT (trans, "unknown pad direction %i", direction);
      return NULL;
  }

  GST_LOG_OBJECT (trans, "returning caps %" GST_PTR_FORMAT, out_caps);

  return out_caps;
}

#if 0
static gboolean
gst_droid_buffer_unpack_get_unit_size (GstBaseTransform * trans,
    GstCaps * caps, guint * size)
{
  GstVideoFormat fmt;
  int width, height;

  GST_DEBUG_OBJECT (trans, "get unit size");

  if (!gst_video_format_parse_caps (caps, &fmt, &width, &height)) {
    GST_WARNING_OBJECT (trans, "failed to parse caps %" GST_PTR_FORMAT, caps);
    return FALSE;
  }

  /* TODO: we output non-standard YUV */
  *size = gst_video_format_get_size (fmt, width, height);

  return TRUE;
}
#endif

static gboolean
gst_droid_buffer_unpack_set_caps (GstBaseTransform * trans,
    GstCaps * incaps, GstCaps * outcaps)
{
  GST_DEBUG_OBJECT (trans, "set caps");
  GST_LOG_OBJECT (trans, "in %" GST_PTR_FORMAT, incaps);
  GST_LOG_OBJECT (trans, "out %" GST_PTR_FORMAT, outcaps);


  /* TODO: */

  return TRUE;
}

static gboolean
gst_droid_buffer_unpack_start (GstBaseTransform * trans)
{
  GstDroidBufferUnpack *conv = GST_DROID_BUFFER_UNPACK (trans);

  GST_DEBUG_OBJECT (conv, "start");

  return TRUE;
}

static gboolean
gst_droid_buffer_unpack_stop (GstBaseTransform * trans)
{
  GstDroidBufferUnpack *conv = GST_DROID_BUFFER_UNPACK (trans);

  GST_DEBUG_OBJECT (conv, "stop");

  return TRUE;
}

static GstFlowReturn
gst_droid_buffer_unpack_transform (GstBaseTransform * trans,
    GstBuffer * inbuf, GstBuffer * outbuf)
{
  gboolean locked;
  GstNativeBuffer *in;
  GstDroidBufferUnpack *conv = GST_DROID_BUFFER_UNPACK (trans);

  GST_DEBUG_OBJECT (conv, "transform");

  in = GST_NATIVE_BUFFER (inbuf);
  locked = gst_native_buffer_is_locked (in);

  if (!locked) {
    if (!gst_native_buffer_lock (in, GST_VIDEO_FORMAT_NV12,
            GRALLOC_USAGE_SW_READ_OFTEN | GRALLOC_USAGE_SW_WRITE_NEVER)) {
      GST_ELEMENT_ERROR (conv, LIBRARY, FAILED,
          ("Could not lock native buffer handle"), (NULL));
      return GST_FLOW_ERROR;
    }
  }

  GST_DEBUG_OBJECT (trans, "in %" GST_PTR_FORMAT, inbuf);
  GST_DEBUG_OBJECT (trans, "out %" GST_PTR_FORMAT, outbuf);

  memcpy (GST_BUFFER_DATA (outbuf), GST_BUFFER_DATA (inbuf),
      GST_BUFFER_SIZE (outbuf));

  if (locked) {
    gst_native_buffer_unlock (in);
  }

  return GST_FLOW_OK;
}

#if 0
static GstFlowReturn
gst_droid_buffer_unpack_prepare_output_buffer (GstBaseTransform *
    trans, GstBuffer * input, gint size, GstCaps * caps, GstBuffer ** buf)
{
  gboolean out_native;
  GstNativeBuffer *in;
  GstNativeBuffer *out;
  GstGralloc *gralloc;
  int width;
  int height;
  int format;
  int usage;
  int stride;
  buffer_handle_t handle;
  GstVideoFormat fmt = GST_VIDEO_FORMAT_UNKNOWN;

  GST_DEBUG_OBJECT (trans, "prepare output buffer %" GST_PTR_FORMAT, caps);

  out_native = IS_NATIVE_CAPS (caps);

  if (out_native) {
    /* We just ref the buffer because we will push it as it is. */
    *buf = gst_buffer_ref (input);
    return GST_FLOW_OK;
  }

  if (!gst_video_format_parse_caps (caps, &fmt, &width, &height)) {
    GST_ELEMENT_ERROR (trans, STREAM, FORMAT, ("Failed to get video format"),
        (NULL));
    return FALSE;
  }

  /* We will have to allocate a new buffer */
  in = GST_NATIVE_BUFFER (input);
  gralloc = gst_native_buffer_get_gralloc (in);
  width = gst_native_buffer_get_width (in);
  height = gst_native_buffer_get_height (in);
  format = gst_native_buffer_get_format (in);
  usage = gst_native_buffer_get_usage (in);

  handle =
      gst_gralloc_allocate (gralloc, width, height, format, usage, &stride);
  if (!handle) {
    GST_ELEMENT_ERROR (trans, LIBRARY, FAILED,
        ("Could not allocate native buffer handle"), (NULL));
    return GST_FLOW_ERROR;
  }

  out =
      gst_native_buffer_new (handle, gralloc, width, height, stride, usage,
      format);
  gst_native_buffer_set_finalize_callback (out,
      gst_droid_buffer_unpack_destroy_native_buffer, gst_object_ref (trans));

  if (!gst_native_buffer_lock (out, fmt, BUFFER_LOCK_USAGE)) {
    gst_buffer_unref (GST_BUFFER (out));

    GST_ELEMENT_ERROR (trans, LIBRARY, FAILED,
        ("Could not lock native buffer handle"), (NULL));
    *buf = NULL;
    return GST_FLOW_ERROR;
  }

  *buf = GST_BUFFER (out);

  gst_buffer_set_caps (*buf, caps);

  return GST_FLOW_OK;
}
#endif

static void
gst_droid_buffer_unpack_fixate_caps (GstBaseTransform * trans,
    GstPadDirection direction, GstCaps * caps, GstCaps * othercaps)
{
  GstStructure *in;
  GstStructure *out;
  int width;
  int height;
  int fps_n;
  int fps_d;

  GST_DEBUG_OBJECT (trans, "fixate caps");

  GST_LOG_OBJECT (trans, "caps %" GST_PTR_FORMAT, caps);
  GST_LOG_OBJECT (trans, "othercaps %" GST_PTR_FORMAT, othercaps);

  /* We care about width, height, format and framerate */
  in = gst_caps_get_structure (caps, 0);
  out = gst_caps_get_structure (othercaps, 0);

  if (gst_structure_get_int (in, "width", &width)) {
    gst_structure_set (out, "width", G_TYPE_INT, width, NULL);
  }

  if (gst_structure_get_int (in, "height", &height)) {
    gst_structure_set (out, "height", G_TYPE_INT, height, NULL);
  }

  if (gst_structure_get_fraction (in, "framerate", &fps_n, &fps_d)) {
    gst_structure_set (out, "framerate", GST_TYPE_FRACTION, fps_n, fps_d, NULL);
  }

  GST_LOG_OBJECT (trans, "caps after fixating %" GST_PTR_FORMAT, othercaps);
}
