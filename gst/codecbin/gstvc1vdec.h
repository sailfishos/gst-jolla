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

#ifndef __GST_VC1V_DEC_H__
#define __GST_VC1V_DEC_H__

#include "gstcodecbin.h"

G_BEGIN_DECLS

#define GST_TYPE_VC1V_DEC \
  (gst_vc1v_dec_get_type())
#define GST_VC1V_DEC(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_VC1V_DEC,GstVc1vDec))
#define GST_VC1V_DEC_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_VC1V_DEC,GstVc1vDecClass))
#define GST_IS_VC1V_DEC(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_VC1V_DEC))
#define GST_IS_VC1V_DEC_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_VC1V_DEC))

typedef struct _GstVc1vDec      GstVc1vDec;
typedef struct _GstVc1vDecClass GstVc1vDecClass;

struct _GstVc1vDec
{
  GstCodecBin parent;

  GstPad *sinkpad, *srcpad;
};

struct _GstVc1vDecClass
{
  GstCodecBinClass parent_class;
};

GType gst_vc1v_dec_get_type (void);

G_END_DECLS

#endif /* __GST_VC1V_DEC_H__ */
