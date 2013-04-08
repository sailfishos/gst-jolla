/*!
 * Copyright (c) 2013 Jolla Ltd.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#include <gst/gst.h>
#include "gstmpeg4vdec.h"
#include "gstmpeg2vdec.h"
#include "gstvc1vdec.h"
#include "gsth263dec.h"
#include "gsth264dec.h"

static gboolean
plugin_init (GstPlugin * plugin)
{
  return
      gst_element_register (plugin, "mpeg4vdec", GST_RANK_PRIMARY + 10,
      gst_mpeg4v_dec_get_type ()) &&
      gst_element_register (plugin, "mpeg2vdec", GST_RANK_PRIMARY + 10,
      gst_mpeg2v_dec_get_type ()) &&
      gst_element_register (plugin, "vc1vdec", GST_RANK_PRIMARY + 10,
      gst_vc1v_dec_get_type ()) &&
      gst_element_register (plugin, "h263dec", GST_RANK_PRIMARY + 10,
      gst_h263_dec_get_type ()) &&
      gst_element_register (plugin, "h264dec", GST_RANK_PRIMARY + 10,
      gst_h264_dec_get_type ());
}

GST_PLUGIN_DEFINE (GST_VERSION_MAJOR, GST_VERSION_MINOR, "codecbin",
    "Codec bin", plugin_init, VERSION, "LGPL", PACKAGE_NAME,
    "http://jollamobile.com/")
