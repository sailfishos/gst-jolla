/*!
 * Copyright (c) 2013 Jolla Ltd.
 */

#ifndef __GST_CODEC_BIN_H__
#define __GST_CODEC_BIN_H__

#include <gst/gst.h>

G_BEGIN_DECLS

#define GST_TYPE_CODEC_BIN \
  (gst_codec_bin_get_type())
#define GST_CODEC_BIN(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_CODEC_BIN,GstCodecBin))
#define GST_CODEC_BIN_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_CODEC_BIN,GstCodecBinClass))
#define GST_IS_CODEC_BIN(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_CODEC_BIN))
#define GST_IS_CODEC_BIN_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_CODEC_BIN))

typedef struct _GstCodecBin      GstCodecBin;
typedef struct _GstCodecBinClass GstCodecBinClass;

struct _GstCodecBin
{
  GstBin parent;

  GstPad *sinkpad, *srcpad;
  GstElement *src;
  GstElement *sink;
  GstPad *element_sink_pad;
  GstPad *element_src_pad;
};

struct _GstCodecBinClass
{
  GstBinClass parent_class;

  gchar *src_element;
  gchar *sink_element;
};

GType gst_codec_bin_get_type (void);

void gst_codec_bin_register_src (GstCodecBinClass *klass, gchar *src);
void gst_codec_bin_register_sink (GstCodecBinClass *klass, gchar *sink);

G_END_DECLS

#endif /* __GST_CODEC_BIN_H__ */
