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
#include <EGL/egl.h>
#include <EGL/eglext.h>

struct _GstDroidEglBufferPrivate
{
  GstNativeBuffer *buffer;
  struct ANativeWindowBuffer native;
  EGLImageKHR image;
  EGLSyncKHR sync;
  GstDroidEglBuffer *public_object;
};

GST_DEBUG_CATEGORY_STATIC (droideglbuffer_debug);
#define GST_CAT_DEFAULT droideglbuffer_debug

static void gst_droid_egl_buffer_finalize (GstDroidEglBuffer * buf);
static void gst_droid_egl_buffer_ref (struct android_native_base_t *base);
static void gst_droid_egl_buffer_unref (struct android_native_base_t *base);

static GstBufferClass *parent_class;

#define container_of(ptr, type, member) ({ \
      const typeof( ((type *)0)->member ) *__mptr = (ptr); (type *)( (char *)__mptr - offsetof(type,member) );})

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

  buf->priv = g_malloc (sizeof (GstDroidEglBufferPrivate));
  buf->priv->public_object = buf;

  buf->priv->buffer = NULL;
  buf->priv->image = EGL_NO_IMAGE_KHR;
  buf->priv->sync = EGL_NO_SYNC_KHR;

  memset (&buf->priv->native, 0x0, sizeof (struct ANativeWindowBuffer));
  buf->priv->native.common.magic = ANDROID_NATIVE_BUFFER_MAGIC;
  buf->priv->native.common.version = sizeof (struct ANativeWindowBuffer);

  buf->priv->native.common.incRef = gst_droid_egl_buffer_ref;
  buf->priv->native.common.decRef = gst_droid_egl_buffer_unref;

  memset (buf->priv->native.common.reserved, 0,
      sizeof (buf->priv->native.common.reserved));
}

static void
gst_droid_egl_buffer_finalize (GstDroidEglBuffer * buf)
{
  GST_DEBUG_OBJECT (buf, "finalize");

  g_free (buf->priv);
  // TODO:
  GST_MINI_OBJECT_CLASS (parent_class)->finalize (GST_MINI_OBJECT (buf));
}

static void
gst_droid_egl_buffer_ref (struct android_native_base_t *base)
{
  struct ANativeWindowBuffer *win =
      container_of (base, struct ANativeWindowBuffer, common);

  GstDroidEglBufferPrivate *priv =
      container_of (win, GstDroidEglBufferPrivate, native);

  GstDroidEglBuffer *buffer = priv->public_object;

  gst_buffer_ref (GST_BUFFER (buffer));
}

static void
gst_droid_egl_buffer_unref (struct android_native_base_t *base)
{
  struct ANativeWindowBuffer *win =
      container_of (base, struct ANativeWindowBuffer, common);

  GstDroidEglBufferPrivate *priv =
      container_of (win, GstDroidEglBufferPrivate, native);

  GstDroidEglBuffer *buffer = priv->public_object;

  gst_buffer_unref (GST_BUFFER (buffer));
}

GstDroidEglBuffer *
gst_droid_egl_buffer_new ()
{
  GstDroidEglBuffer *buf;

  buf = (GstDroidEglBuffer *) gst_mini_object_new (GST_TYPE_DROID_EGL_BUFFER);

  return buf;
}

void
gst_droid_egl_buffer_set_native_buffer (GstDroidEglBuffer * buffer,
    GstNativeBuffer * buf)
{
  GstNativeBuffer *old_buffer = buffer->priv->buffer;
  if (old_buffer == buf) {
    GST_DEBUG_OBJECT (buffer, "set native buffer: old and new are the same.");
    return;
  }

  buffer->priv->buffer = buf;

  GST_LOG_OBJECT (buffer, "set native buffer: replacing %p with %p", old_buffer,
      buf);

  if (old_buffer) {
    gst_buffer_unref (GST_BUFFER (old_buffer));
  }

  if (buf) {
    gst_buffer_ref (GST_BUFFER (buf));

    buffer->priv->native.stride = buf->stride;
    buffer->priv->native.usage = buf->usage;
    buffer->priv->native.handle = buf->handle;
  } else {
    buffer->priv->native.stride = 0;
    buffer->priv->native.usage = 0;
    buffer->priv->native.width = 0;
    buffer->priv->native.height = 0;
    buffer->priv->native.format = 0;
    buffer->priv->native.handle = NULL;
  }
}

GstNativeBuffer *
gst_droid_egl_buffer_get_native_buffer (GstDroidEglBuffer * buffer)
{
  GST_LOG_OBJECT (buffer, "get native buffer");

  return buffer->priv->buffer;
}

void
gst_droid_egl_buffer_set_format (GstDroidEglBuffer * buffer, int width,
    int height, int format)
{
  buffer->priv->native.width = width;
  buffer->priv->native.height = height;
  buffer->priv->native.format = format;
}
