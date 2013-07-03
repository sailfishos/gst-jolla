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

#include "gstdroideglbuffer.h"

GST_DEBUG_CATEGORY_STATIC (droideglbuffer_debug);
#define GST_CAT_DEFAULT droideglbuffer_debug

static void gst_droid_egl_buffer_finalize (GstDroidEglBuffer * buf);

static GstBufferClass *parent_class;

G_DEFINE_TYPE (GstDroidEglBuffer, gst_droid_egl_buffer, GST_TYPE_BUFFER);

static void
gst_droid_egl_buffer_class_init (GstDroidEglBufferClass * buffer_class)
{
  GstMiniObjectClass *mo_class = GST_MINI_OBJECT_CLASS (buffer_class);

  parent_class = g_type_class_peek_parent (buffer_class);

  mo_class->finalize =
      (GstMiniObjectFinalizeFunction) gst_droid_egl_buffer_finalize;

  GST_DEBUG_CATEGORY_INIT (droideglbuffer_debug, "droideglbuffer", 0,
      "GstDroidEglBuffer debug");
}

static void
gst_droid_egl_buffer_init (GstDroidEglBuffer * buf)
{
  GST_DEBUG_OBJECT (buf, "init");

  memset (&buf->native, 0x0, sizeof (struct ANativeWindowBuffer));

  buf->buffer = NULL;
  buf->image = NULL;
  buf->sync = NULL;
}

static void
gst_droid_egl_buffer_finalize (GstDroidEglBuffer * buf)
{
  GST_DEBUG_OBJECT (buf, "finalize");

  // TODO:
  GST_MINI_OBJECT_CLASS (parent_class)->finalize (GST_MINI_OBJECT (buf));
}

GstDroidEglBuffer *
gst_droid_egl_buffer_new ()
{
  GstDroidEglBuffer *buf;

  buf = (GstDroidEglBuffer *) gst_mini_object_new (GST_TYPE_DROID_EGL_BUFFER);

  return buf;
}
