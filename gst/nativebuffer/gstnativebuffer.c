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

#define container_of(ptr, type, member) ({ \
      const typeof( ((type *)0)->member ) *__mptr = (ptr); (type *)( (char *)__mptr - offsetof(type,member) );})

static void gst_native_buffer_finalize (GstNativeBuffer * buf);
static void gst_native_buffer_inc_ref (struct android_native_base_t *base);
static void gst_native_buffer_dec_ref (struct android_native_base_t *base);

static GstBufferClass *parent_class;

G_DEFINE_TYPE (GstNativeBuffer, gst_native_buffer, GST_TYPE_BUFFER);

struct _GstNativeBufferPrivate
{
  GstNativeBuffer *buffer;

  GstGralloc *gralloc;
  gboolean locked;

  struct ANativeWindowBuffer native;

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
  buf->priv->buffer = buf;

  memset (&buf->priv->native, 0x0, sizeof (buf->priv->native));
  memset (&buf->priv->native.common.reserved, 0x0,
      sizeof (buf->priv->native.common.reserved));

  buf->priv->native.common.magic = ANDROID_NATIVE_BUFFER_MAGIC;
  buf->priv->native.common.version = sizeof (struct ANativeWindowBuffer);

  buf->priv->native.common.incRef = gst_native_buffer_inc_ref;
  buf->priv->native.common.decRef = gst_native_buffer_dec_ref;

  buf->priv->gralloc = NULL;
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

static void
gst_native_buffer_inc_ref (struct android_native_base_t *base)
{
  struct ANativeWindowBuffer *win =
      container_of (base, struct ANativeWindowBuffer, common);
  GstNativeBufferPrivate *priv =
      container_of (win, GstNativeBufferPrivate, native);

  GstNativeBuffer *buffer = priv->buffer;

  GST_LOG_OBJECT (buffer, "inc ref");

  gst_buffer_ref (GST_BUFFER (buffer));
}

static void
gst_native_buffer_dec_ref (struct android_native_base_t *base)
{
  struct ANativeWindowBuffer *win =
      container_of (base, struct ANativeWindowBuffer, common);
  GstNativeBufferPrivate *priv =
      container_of (win, GstNativeBufferPrivate, native);

  GstNativeBuffer *buffer = priv->buffer;

  GST_LOG_OBJECT (buffer, "dec ref");

  gst_buffer_unref (GST_BUFFER (buffer));
}

GstNativeBuffer *
gst_native_buffer_new (buffer_handle_t handle, GstGralloc * gralloc, int width,
    int height, int stride, int usage, int format)
{
  GstNativeBuffer *buffer;

  buffer = (GstNativeBuffer *) gst_mini_object_new (GST_TYPE_NATIVE_BUFFER);

  GST_DEBUG_OBJECT (buffer, "new");

  buffer->priv->native.handle = handle;
  buffer->priv->gralloc = gst_gralloc_ref (gralloc);
  buffer->priv->native.stride = stride;
  buffer->priv->native.usage = usage;
  buffer->priv->native.width = width;
  buffer->priv->native.height = height;
  buffer->priv->native.format = format;

  gst_buffer_set_data (buffer, (guint8 *) handle, sizeof (handle));

  return buffer;
}

gboolean
gst_native_buffer_lock (GstNativeBuffer * buffer, GstVideoFormat format,
    int usage)
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
      buffer->priv->native.handle, usage, 0, 0, buffer->priv->native.width,
      buffer->priv->native.height, &data);

  if (err != 0) {
    GST_WARNING_OBJECT (buffer, "Error 0x%x locking buffer", err);
    return FALSE;
  }

  gst_buffer_set_data (buffer, data, gst_video_format_get_size (format,
          buffer->priv->native.width, buffer->priv->native.height));

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
      buffer->priv->native.handle);

  if (err != 0) {
    GST_WARNING_OBJECT (buffer, "Error 0x%x unlocking buffer", err);
    return FALSE;
  }

  buffer->priv->locked = FALSE;

  gst_buffer_set_data (buffer, (guint8 *) buffer->priv->native.handle,
      sizeof (buffer->priv->native.handle));

  return TRUE;
}

buffer_handle_t *
gst_native_buffer_get_handle (GstNativeBuffer * buffer)
{
  return &buffer->priv->native.handle;
}

int
gst_native_buffer_get_usage (GstNativeBuffer * buffer)
{
  return buffer->priv->native.usage;
}

int
gst_native_buffer_get_stride (GstNativeBuffer * buffer)
{
  return buffer->priv->native.stride;
}

int
gst_native_buffer_get_width (GstNativeBuffer * buffer)
{
  return buffer->priv->native.width;
}

int
gst_native_buffer_get_height (GstNativeBuffer * buffer)
{
  return buffer->priv->native.height;
}

int
gst_native_buffer_get_format (GstNativeBuffer * buffer)
{
  return buffer->priv->native.format;
}

GstGralloc *
gst_native_buffer_get_gralloc (GstNativeBuffer * buffer)
{
  return buffer->priv->gralloc;
}

struct ANativeWindowBuffer *
gst_native_buffer_get_native_buffer (GstNativeBuffer * buffer)
{
  return &buffer->priv->native;
}

void
gst_native_buffer_set_finalize_callback (GstNativeBuffer * buffer,
    GstNativeBufferFinalizeCallback cb, void *data)
{
  buffer->priv->finalize_callback = cb;
  buffer->priv->finalize_callback_data = data;
}

GstNativeBuffer *
gst_native_buffer_find (buffer_handle_t * handle)
{
  struct ANativeWindowBuffer *win =
      container_of (handle, struct ANativeWindowBuffer, handle);

  GstNativeBufferPrivate *priv =
      container_of (win, GstNativeBufferPrivate, native);

  GstNativeBuffer *buffer = priv->buffer;

  return buffer;
}
