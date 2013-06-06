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

#include "gstnativebuffer.h"

static void gst_native_buffer_finalize (GstNativeBuffer * buf);

static GstBufferClass *parent_class;

G_DEFINE_TYPE (GstNativeBuffer, gst_native_buffer, GST_TYPE_BUFFER);

static void
gst_native_buffer_class_init (GstNativeBufferClass * buffer_class)
{
  GstMiniObjectClass *mo_class = GST_MINI_OBJECT_CLASS (buffer_class);

  parent_class = g_type_class_peek_parent (buffer_class);

  mo_class->finalize =
      (GstMiniObjectFinalizeFunction) gst_native_buffer_finalize;
}

static void
gst_native_buffer_init (GstNativeBuffer * buf)
{
  buf->handle = NULL;
  buf->gralloc = NULL;
  buf->stride = 0;
}

static void
gst_native_buffer_finalize (GstNativeBuffer * buf)
{
  if (buf->finalize_callback) {
    if (buf->finalize_callback (buf->finalize_callback_data, buf)) {
      // Callback returning TRUE means it's resurrected the buffer.
      return;
    }
  }

  gst_gralloc_unref (buf->gralloc);

  GST_MINI_OBJECT_CLASS (parent_class)->finalize (GST_MINI_OBJECT (buf));
}

GstNativeBuffer *
gst_native_buffer_new (buffer_handle_t handle, GstGralloc * gralloc, int stride)
{
  GstNativeBuffer *buffer =
      (GstNativeBuffer *) gst_mini_object_new (GST_TYPE_NATIVE_BUFFER);

  buffer->handle = handle;
  buffer->gralloc = gst_gralloc_ref (gralloc);
  buffer->stride = stride;

  return buffer;
}
