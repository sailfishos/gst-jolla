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

GST_DEBUG_CATEGORY_STATIC (nativebuffer_debug);
#define GST_CAT_DEFAULT nativebuffer_debug

static void gst_native_buffer_finalize (GstNativeBuffer * buf);

static GstBufferClass *parent_class;

G_DEFINE_TYPE (GstNativeBuffer, gst_native_buffer, GST_TYPE_BUFFER);

struct _GstNativeBufferPrivate
{
  buffer_handle_t handle;
  GstGralloc *gralloc;
  int stride;
  int usage;
  gboolean locked;

  GstNativeBufferFinalizeCallback finalize_callback;
  void *finalize_callback_data;
};

static void
gst_native_buffer_class_init (GstNativeBufferClass * buffer_class)
{
  GstMiniObjectClass *mo_class = GST_MINI_OBJECT_CLASS (buffer_class);

  parent_class = g_type_class_peek_parent (buffer_class);

  mo_class->finalize =
      (GstMiniObjectFinalizeFunction) gst_native_buffer_finalize;

  GST_DEBUG_CATEGORY_INIT (nativebuffer_debug, "nativebuffer", 0,
      "GstNativeBuffer debug");
}

static void
gst_native_buffer_init (GstNativeBuffer * buf)
{
  buf->priv = g_malloc (sizeof (GstNativeBufferPrivate));

  buf->priv->handle = NULL;
  buf->priv->gralloc = NULL;
  buf->priv->stride = 0;
  buf->priv->locked = FALSE;

  buf->priv->finalize_callback = NULL;
  buf->priv->finalize_callback_data = NULL;

  GST_DEBUG_OBJECT (buf, "init");
}

static void
gst_native_buffer_finalize (GstNativeBuffer * buf)
{
  GST_DEBUG_OBJECT (buf, "finalize");

  if (buf->priv->finalize_callback) {
    if (buf->priv->finalize_callback (buf->priv->finalize_callback_data, buf)) {
      // Callback returning TRUE means it's resurrected the buffer.

      GST_DEBUG_OBJECT (buf, "resurrected");
      return;
    }
  }

  gst_native_buffer_unlock (buf);

  gst_gralloc_unref (buf->priv->gralloc);

  g_free (buf->priv);

  GST_DEBUG_OBJECT (buf, "finalized");

  GST_MINI_OBJECT_CLASS (parent_class)->finalize (GST_MINI_OBJECT (buf));
}

GstNativeBuffer *
gst_native_buffer_new (buffer_handle_t handle, GstGralloc * gralloc, int stride,
    int usage)
{
  GstNativeBuffer *buffer;

  buffer = (GstNativeBuffer *) gst_mini_object_new (GST_TYPE_NATIVE_BUFFER);

  GST_DEBUG_OBJECT (buffer, "new");

  buffer->priv->handle = handle;
  buffer->priv->gralloc = gst_gralloc_ref (gralloc);
  buffer->priv->stride = stride;
  buffer->priv->usage = usage;

  GST_BUFFER_SIZE (GST_BUFFER (buffer)) = sizeof (handle);
  GST_BUFFER_DATA (GST_BUFFER (buffer)) = (guint8 *) handle;

  return buffer;
}

gboolean
gst_native_buffer_lock (GstNativeBuffer * buffer, GstVideoFormat format,
    int width, int height, int usage)
{
  void *data;
  int err;

  GST_DEBUG_OBJECT (buffer, "lock");

  if (buffer->priv->locked) {
    return TRUE;
  }

  if (format == GST_VIDEO_FORMAT_UNKNOWN) {
    GST_WARNING_OBJECT (buffer, "Unknown video format");
    return FALSE;
  }

  err = buffer->priv->gralloc->gralloc->lock (buffer->priv->gralloc->gralloc,
      buffer->priv->handle, usage, 0, 0, width, height, &data);

  if (err != 0) {
    GST_WARNING_OBJECT (buffer, "Error 0x%x locking buffer", err);
    return FALSE;
  }

  GST_BUFFER_SIZE (buffer) = gst_video_format_get_size (format, width, height);
  GST_BUFFER_DATA (buffer) = data;

  buffer->priv->locked = TRUE;

  return TRUE;
}

gboolean
gst_native_buffer_unlock (GstNativeBuffer * buffer)
{
  int err;

  GST_DEBUG_OBJECT (buffer, "unlock");

  if (!buffer->priv->locked) {
    return TRUE;
  }

  err =
      buffer->priv->gralloc->gralloc->unlock (buffer->priv->gralloc->gralloc,
      buffer->priv->handle);

  if (err != 0) {
    GST_WARNING_OBJECT (buffer, "Error 0x%x unlocking buffer", err);
    return FALSE;
  }

  buffer->priv->locked = FALSE;
  GST_BUFFER_SIZE (GST_BUFFER (buffer)) = sizeof (buffer->priv->handle);
  GST_BUFFER_DATA (GST_BUFFER (buffer)) = (guint8 *) buffer->priv->handle;

  return TRUE;
}

buffer_handle_t
gst_native_buffer_get_handle (GstNativeBuffer * buffer)
{
  return buffer->priv->handle;
}

int
gst_native_buffer_get_usage (GstNativeBuffer * buffer)
{
  return buffer->priv->usage;
}

int
gst_native_buffer_get_stride (GstNativeBuffer * buffer)
{
  return buffer->priv->stride;
}

GstGralloc *
gst_native_buffer_get_gralloc (GstNativeBuffer * buffer)
{
  return buffer->priv->gralloc;
}

void
gst_native_buffer_set_finalize_callback (GstNativeBuffer * buffer,
    GstNativeBufferFinalizeCallback cb, void *data)
{
  g_assert (buffer->priv->finalize_callback == NULL);
  g_assert (buffer->priv->finalize_callback_data == NULL);

  buffer->priv->finalize_callback = cb;
  buffer->priv->finalize_callback_data = data;
}
