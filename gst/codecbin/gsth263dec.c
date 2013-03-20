/*!
 * Copyright (c) 2013 Jolla Ltd.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */
#include "gsth263dec.h"

GST_BOILERPLATE (GstH263Dec, gst_h263_dec, GstCodecBin, GST_TYPE_CODEC_BIN);

static void
gst_h263_dec_base_init (gpointer gclass)
{
  GstCodecBinClass *bin_class = GST_CODEC_BIN_CLASS (gclass);

  GstElementClass *element_class = GST_ELEMENT_CLASS (gclass);

  gst_element_class_set_details_simple (element_class,
      "MPEG4 video decoder",
      "Codec/Decoder/Video",
      "Wrapper for omx MPEG4 video decoder",
      "Mohammed Hassan <mohammed.hassan@jollamobile.com>");

  gst_codec_bin_register_src (bin_class, "ste_colorconv");
  gst_codec_bin_register_sink (bin_class, "omxh263dec");
}

static void
gst_h263_dec_class_init (GstH263DecClass * klass)
{
}

static void
gst_h263_dec_init (GstH263Dec * bin, GstH263DecClass * gclass)
{
}
