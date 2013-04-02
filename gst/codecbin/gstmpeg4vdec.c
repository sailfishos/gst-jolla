/*!
 * Copyright (c) 2013 Jolla Ltd.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */
#include "gstmpeg4vdec.h"

GST_BOILERPLATE (GstMpeg4vDec, gst_mpeg4v_dec, GstCodecBin, GST_TYPE_CODEC_BIN);

static GstStaticPadTemplate sink_factory = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS
    ("video/mpeg, mpegversion = (int) 4, parsed = (boolean) true"));

static GstStaticPadTemplate src_factory = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (GST_VIDEO_CAPS_YUV ("{ YUMB, I420 }")));

static void
gst_mpeg4v_dec_base_init (gpointer gclass)
{
  GstCodecBinClass *bin_class = GST_CODEC_BIN_CLASS (gclass);
  GstElementClass *element_class = GST_ELEMENT_CLASS (gclass);

  gst_element_class_add_static_pad_template (element_class, &src_factory);
  gst_element_class_add_static_pad_template (element_class, &sink_factory);

  gst_element_class_set_details_simple (element_class,
      "MPEG4 video decoder",
      "Codec/Decoder/Video",
      "Wrapper for omx MPEG4 video decoder",
      "Mohammed Hassan <mohammed.hassan@jollamobile.com>");

  bin_class->src_element = "ste_colorconv";
  bin_class->sink_element = "omxmpeg4videodec";
}

static void
gst_mpeg4v_dec_class_init (GstMpeg4vDecClass * klass)
{
}

static void
gst_mpeg4v_dec_init (GstMpeg4vDec * bin, GstMpeg4vDecClass * gclass)
{
}
