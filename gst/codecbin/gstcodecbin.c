/*!
 * Copyright (c) 2013 Jolla Ltd.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#include "gstcodecbin.h"
#include <gst/gst.h>

GST_DEBUG_CATEGORY_STATIC (codecbin_debug);
#define GST_CAT_DEFAULT codecbin_debug

#define gst_codec_bin_debug_init(ignored_parameter)					\
  GST_DEBUG_CATEGORY_INIT (codecbin_debug, "codecbin", 0, "codecbin element"); \

GST_BOILERPLATE_FULL (GstCodecBin, gst_codec_bin, GstBin, GST_TYPE_BIN,
    gst_codec_bin_debug_init);

static void gst_codec_bin_finalize (GObject * object);
static gboolean gst_codec_bin_create_sink_element (GstCodecBin * bin,
    GstCodecBinClass * klass, const gchar * factory);
static gboolean gst_codec_bin_create_src_element (GstCodecBin * bin,
    GstCodecBinClass * klass, const gchar * factory);

static void
gst_codec_bin_base_init (gpointer gclass)
{

}

static void
gst_codec_bin_class_init (GstCodecBinClass * klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;

  gobject_class->finalize = gst_codec_bin_finalize;
}

static void
gst_codec_bin_init (GstCodecBin * bin, GstCodecBinClass * gclass)
{
  bin->sink = NULL;
  bin->src = NULL;
  bin->element_sink_pad = NULL;
  bin->element_src_pad = NULL;

  GST_DEBUG_OBJECT (bin,
      "superclass provided sink element %s and source element %s",
      gclass->sink_element, gclass->src_element);

  if (!gclass->sink_element || !gclass->src_element) {
    GST_ERROR_OBJECT (bin, "Subclass didn't provide needed element names");
    return;
  }

  if (!gst_codec_bin_create_sink_element (bin, gclass, gclass->sink_element) ||
      !gst_codec_bin_create_src_element (bin, gclass, gclass->src_element)) {
    return;
  }

  if (!gst_element_link (bin->sink, bin->src)) {
    GST_ERROR_OBJECT (bin, "Failed to link sink and source elements");
    return;
  }
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

static gboolean
gst_codec_bin_create_sink_element (GstCodecBin * bin, GstCodecBinClass * klass,
    const gchar * factory)
{
  GstPad *sink_pad = NULL;
  GstPad *element_pad = NULL;
  GstElement *sink = NULL;
  GstPadTemplate *tpl =
      gst_element_class_get_pad_template (GST_ELEMENT_CLASS (klass), "sink");

  GST_DEBUG_OBJECT (bin, "sink pad template has caps %" GST_PTR_FORMAT,
      tpl->caps);

  sink = gst_element_factory_make (factory, NULL);
  if (!sink) {
    GST_ERROR_OBJECT (bin, "Failed to create sink element %s", factory);
    return FALSE;
  }

  gst_bin_add (GST_BIN (bin), sink);
  element_pad = gst_element_get_static_pad (sink, "sink");
  if (!element_pad) {
    GST_ERROR_OBJECT ("Failed to get static sink pad for %s", factory);
    return FALSE;
  }

  sink_pad = gst_ghost_pad_new_from_template ("sink", element_pad, tpl);
  gst_element_add_pad (GST_ELEMENT (bin), sink_pad);

  bin->element_sink_pad = element_pad;
  bin->sink = sink;

  return TRUE;
}

static gboolean
gst_codec_bin_create_src_element (GstCodecBin * bin, GstCodecBinClass * klass,
    const gchar * factory)
{
  GstPad *src_pad = NULL;
  GstPad *element_pad = NULL;
  GstElement *src = NULL;
  GstPadTemplate *tpl =
      gst_element_class_get_pad_template (GST_ELEMENT_CLASS (klass), "src");

  GST_DEBUG_OBJECT (bin, "src pad template has caps %" GST_PTR_FORMAT,
      tpl->caps);

  src = gst_element_factory_make (factory, NULL);
  if (!src) {
    GST_ERROR_OBJECT (bin, "Failed to create src element %s", factory);
    return FALSE;
  }

  gst_bin_add (GST_BIN (bin), src);
  element_pad = gst_element_get_static_pad (src, "src");
  if (!element_pad) {
    GST_ERROR_OBJECT ("Failed to get static src pad for %s", factory);
    return FALSE;
  }

  src_pad = gst_ghost_pad_new_from_template ("src", element_pad, tpl);
  gst_element_add_pad (GST_ELEMENT (bin), src_pad);

  bin->element_src_pad = element_pad;
  bin->src = src;

  return TRUE;
}
