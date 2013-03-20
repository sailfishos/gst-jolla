/*!
 * Copyright (c) 2013 Jolla Ltd.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */
#include "gstmpeg2vdec.h"

GST_BOILERPLATE (GstMpeg2vDec, gst_mpeg2v_dec, GstCodecBin, GST_TYPE_CODEC_BIN);

static void
gst_mpeg2v_dec_base_init (gpointer gclass)
{
  GstCodecBinClass *bin_class = GST_CODEC_BIN_CLASS (gclass);

  GstElementClass *element_class = GST_ELEMENT_CLASS (gclass);

  gst_element_class_set_details_simple (element_class,
      "MPEG2 video decoder",
      "Codec/Decoder/Video",
      "Wrapper for omx MPEG2 video decoder",
      "Mohammed Hassan <mohammed.hassan@jollamobile.com>");

  gst_codec_bin_register_src (bin_class, "ste_colorconv");
  gst_codec_bin_register_sink (bin_class, "omxmpeg2videodec");
}

static void
gst_mpeg2v_dec_class_init (GstMpeg2vDecClass * klass)
{
}

static void
gst_mpeg2v_dec_init (GstMpeg2vDec * bin, GstMpeg2vDecClass * gclass)
{
}
