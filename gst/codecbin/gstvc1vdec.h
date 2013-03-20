/*!
 * Copyright (c) 2013 Jolla Ltd.
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
