/*
 * Copyright (C) 2014 Jolla LTD.
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

#ifndef __GST_A_NATIVE_WINDOW_BUFFER_POOL__
#define __GST_A_NATIVE_WINDOW_BUFFER_POOL__

#include <gst/gst.h>
#include <gst/video/video.h>

#include <system/window.h>
#include <hardware/gralloc.h>

G_BEGIN_DECLS

typedef struct _GstANativeWindowBufferPool GstANativeWindowBufferPool;
typedef struct _GstANativeWindowBufferPoolPrivate GstANativeWindowBufferPoolPrivate;
typedef struct _GstANativeWindowBufferPoolClass GstANativeWindowBufferPoolClass;

#define GST_A_NATIVE_WINDOW_BUFFER_MEMORY_TYPE "ANativeWindowBuffer"
#define GST_A_NATIVE_WINDOW_BUFFER_MEMORY_VIDEO_FORMATS "{ NV12_64Z32, YV12, NV16, " \
    "NV12, YUY2, RGBA, RGBx, RGB, RGB16, BGRA }"
#define GST_CAPS_FEATURE_MEMORY_A_NATIVE_WINDOW_BUFFER "memory:ANativeWindowBuffer"

ANativeWindowBuffer_t *gst_a_native_window_buffer_memory_get_buffer (GstMemory *mem);

#define GST_TYPE_A_NATIVE_WINDOW_BUFFER_POOL      (gst_a_native_window_buffer_pool_get_type())
#define GST_IS_A_NATIVE_WINDOW_BUFFER_POOL(obj)   (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GST_TYPE_A_NATIVE_WINDOW_BUFFER_POOL))
#define GST_A_NATIVE_WINDOW_BUFFER_POOL(obj)      (G_TYPE_CHECK_INSTANCE_CAST ((obj), GST_TYPE_A_NATIVE_WINDOW_BUFFER_POOL, GstANativeWindowBufferPool))
#define GST_A_NATIVE_WINDOW_BUFFER_POOL_CAST(obj) ((GstANativeWindowBufferPool*) (obj))

struct _GstANativeWindowBufferPool {
  GstBufferPool bufferpool;

  GstANativeWindowBufferPoolPrivate *priv;
};

struct _GstANativeWindowBufferPoolClass {
  GstBufferPoolClass parent_class;
};

GType gst_a_native_window_buffer_pool_get_type (void);

GstBufferPool *gst_a_native_window_buffer_pool_new (void);

G_END_DECLS

#endif
