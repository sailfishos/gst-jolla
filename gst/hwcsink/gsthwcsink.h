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

#ifndef __GST_HWC_SINK_H__
#define __GST_HWC_SINK_H__

#include <gst/gst.h>
#include <gst/video/gstvideosink.h>
#include "gstgralloc.h"
#include <hardware/fb.h>
#include <hardware/hwcomposer.h>

G_BEGIN_DECLS

#define GST_TYPE_HWC_SINK \
  (gst_hwc_sink_get_type())
#define GST_HWC_SINK(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_HWC_SINK,GstHwcSink))
#define GST_HWC_SINK_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_HWC_SINK,GstHwcSinkClass))
#define GST_IS_HWC_SINK(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_HWC_SINK))
#define GST_IS_HWC_SINK_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_HWC_SINK))

typedef struct _GstHwcSink GstHwcSink;
typedef struct _GstHwcSinkClass GstHwcSinkClass;

struct _GstHwcSink {
  GstVideoSink parent;

  GstGralloc *gralloc;

  hwc_rect_t src_rect;
  hwc_rect_t screen_rect;
  hwc_display_contents_1_t *hwc_display;

  framebuffer_device_t *fb;
  hwc_composer_device_1_t *hwc;

  int fps_n;
  int fps_d;
};

struct _GstHwcSinkClass {
  GstVideoSinkClass parent_class;
};

GType gst_hwc_sink_get_type (void);

G_END_DECLS

#endif /* __GST_HWC_SINK_H__ */
