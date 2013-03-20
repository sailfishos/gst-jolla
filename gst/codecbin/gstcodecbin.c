/*!
 * Copyright (c) 2013 Jolla Ltd.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#include "gstcodecbin.h"
#include <gst/gst.h>

GST_BOILERPLATE (GstCodecBin, gst_codec_bin, GstBin, GST_TYPE_BIN);

static void gst_codec_bin_finalize (GObject * object);

static void
gst_codec_bin_base_init (gpointer gclass)
{

}

static void
gst_codec_bin_class_init (GstCodecBinClass * klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;

  gobject_class->finalize = gst_codec_bin_finalize;

  klass->src_element = NULL;
  klass->sink_element = NULL;
}

static void
gst_codec_bin_init (GstCodecBin * bin, GstCodecBinClass * gclass)
{
  bin->sinkpad = NULL;
  bin->srcpad = NULL;
  bin->sink = NULL;
  bin->src = NULL;
  bin->element_sink_pad = NULL;
  bin->element_src_pad = NULL;

  if (!gclass->sink_element || !gclass->src_element) {
    GST_ERROR_OBJECT (bin, "Subclass didn't provide needed element names");
    return;
  }

  bin->sink = gst_element_factory_make (gclass->sink_element, NULL);
  bin->src = gst_element_factory_make (gclass->src_element, NULL);

  if (!bin->sink || !bin->src) {
    GST_ERROR_OBJECT (bin, "Failed to create required elements");
    return;
  }

  gst_bin_add_many (GST_BIN (bin), bin->sink, bin->src, NULL);
  if (!gst_element_link (bin->sink, bin->src)) {
    GST_ERROR_OBJECT (bin, "Failed to link %s to %s", gclass->sink_element,
        gclass->src_element);
    return;
  }

  bin->element_sink_pad = gst_element_get_static_pad (bin->sink, "sink");
  bin->sinkpad = gst_ghost_pad_new_from_template ("sink", bin->element_sink_pad,
      gst_element_class_get_pad_template (GST_ELEMENT_CLASS (gclass), "sink"));

  bin->element_src_pad = gst_element_get_static_pad (bin->src, "src");
  bin->srcpad = gst_ghost_pad_new_from_template ("src", bin->element_src_pad,
      gst_element_class_get_pad_template (GST_ELEMENT_CLASS (gclass), "src"));

  gst_element_add_pad (GST_ELEMENT (bin), bin->sinkpad);
  gst_element_add_pad (GST_ELEMENT (bin), bin->srcpad);
}

static void
gst_codec_bin_finalize (GObject * object)
{
  GstCodecBin *self = GST_CODEC_BIN (object);

  if (self->element_sink_pad) {
    gst_object_unref (self->element_sink_pad);
  }

  if (self->element_src_pad) {
    gst_object_unref (self->element_src_pad);
  }

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

void
gst_codec_bin_register_src (GstCodecBinClass * klass, gchar * src)
{
  GstElementFactory *factory = gst_element_factory_find (src);
  int x = 0;

  if (!factory)
    return;

  for (x = 0; x < factory->numpadtemplates; x++) {
    GstStaticPadTemplate *tpl =
        g_list_nth_data (factory->staticpadtemplates, x);
    if (tpl->direction == GST_PAD_SRC) {
      GstPadTemplate *pad_template = gst_static_pad_template_get (tpl);
      gst_element_class_add_pad_template (GST_ELEMENT_CLASS (klass),
          pad_template);
      gst_object_unref (pad_template);
    }
  }

  gst_object_unref (factory);

  klass->src_element = src;
}

void
gst_codec_bin_register_sink (GstCodecBinClass * klass, gchar * sink)
{
  GstElementFactory *factory = gst_element_factory_find (sink);
  int x = 0;

  if (!factory)
    return;

  for (x = 0; x < factory->numpadtemplates; x++) {
    GstStaticPadTemplate *tpl =
        g_list_nth_data (factory->staticpadtemplates, x);
    if (tpl->direction == GST_PAD_SINK) {
      GstPadTemplate *pad_template = gst_static_pad_template_get (tpl);
      gst_element_class_add_pad_template (GST_ELEMENT_CLASS (klass),
          pad_template);
      gst_object_unref (pad_template);
    }
  }

  gst_object_unref (factory);

  klass->sink_element = sink;
}
