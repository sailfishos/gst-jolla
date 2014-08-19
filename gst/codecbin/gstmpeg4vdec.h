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

#ifndef __GST_MPEG4V_DEC_H__
#define __GST_MPEG4V_DEC_H__

#include "gstcodecbin.h"

G_BEGIN_DECLS

#define GST_TYPE_MPEG4V_DEC \
  (gst_mpeg4v_dec_get_type())
#define GST_MPEG4V_DEC(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_MPEG4V_DEC,GstMpeg4vDec))
#define GST_MPEG4V_DEC_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_MPEG4V_DEC,GstMpeg4vDecClass))
#define GST_IS_MPEG4V_DEC(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_MPEG4V_DEC))
#define GST_IS_MPEG4V_DEC_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_MPEG4V_DEC))

typedef struct _GstMpeg4vDec      GstMpeg4vDec;
typedef struct _GstMpeg4vDecClass GstMpeg4vDecClass;

struct _GstMpeg4vDec
{
  GstCodecBin parent;

  GstPad *sinkpad, *srcpad;
};

struct _GstMpeg4vDecClass
{
  GstCodecBinClass parent_class;
};

GType gst_mpeg4v_dec_get_type (void);

G_END_DECLS

#endif /* __GST_MPEG4V_DEC_H__ */
