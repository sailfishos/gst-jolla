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

#ifndef __GST_CODEC_BIN_H__
#define __GST_CODEC_BIN_H__

#include <gst/gst.h>
#include <gst/video/video.h>

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

G_END_DECLS

#endif /* __GST_CODEC_BIN_H__ */
