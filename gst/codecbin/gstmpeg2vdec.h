/*!
 * Copyright (c) 2013 Jolla Ltd.
 */

#ifndef __GST_MPEG2V_DEC_H__
#define __GST_MPEG2V_DEC_H__

#include "gstcodecbin.h"

G_BEGIN_DECLS

#define GST_TYPE_MPEG2V_DEC \
  (gst_mpeg2v_dec_get_type())
#define GST_MPEG2V_DEC(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_MPEG2V_DEC,GstMpeg2vDec))
#define GST_MPEG2V_DEC_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_MPEG2V_DEC,GstMpeg2vDecClass))
#define GST_IS_MPEG2V_DEC(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_MPEG2V_DEC))
#define GST_IS_MPEG2V_DEC_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_MPEG2V_DEC))

typedef struct _GstMpeg2vDec      GstMpeg2vDec;
typedef struct _GstMpeg2vDecClass GstMpeg2vDecClass;

struct _GstMpeg2vDec
{
  GstCodecBin parent;

  GstPad *sinkpad, *srcpad;
};

struct _GstMpeg2vDecClass
{
  GstCodecBinClass parent_class;
};

GType gst_mpeg2v_dec_get_type (void);

G_END_DECLS

#endif /* __GST_MPEG2V_DEC_H__ */
