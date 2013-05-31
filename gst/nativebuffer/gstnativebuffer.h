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

#ifndef __GST_NATIVE_BUFFER_H__
#define __GST_NATIVE_BUFFER_H__

#include <gst/gst.h>
#include "gstgralloc.h"

G_BEGIN_DECLS

typedef struct _GstNativeBuffer GstNativeBuffer;
typedef struct _GstNativeBufferClass GstNativeBufferClass;

#define GST_TYPE_NATIVE_BUFFER            (gst_native_buffer_get_type())
#define GST_IS_NATIVE_BUFFER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GST_TYPE_NATIVE_BUFFER))
#define GST_IS_NATIVE_BUFFER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GST_TYPE_NATIVE_BUFFER))
#define GST_NATIVE_BUFFER_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GST_TYPE_NATIVE_BUFFER, GstNativeBufferClass))
#define GST_NATIVE_BUFFER(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GST_TYPE_NATIVE_BUFFER, GstNativeBuffer))
#define GST_NATIVE_BUFFER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GST_TYPE_NATIVE_BUFFER, GstNativeBufferClass))

struct _GstNativeBuffer {
  GstBuffer buffer;

  buffer_handle_t handle;
  GstGralloc *gralloc;
};

struct _GstNativeBufferClass {
  GstBufferClass  buffer_class;
};

GType           gst_native_buffer_get_type           (void);

GstNativeBuffer*   gst_native_buffer_new             (buffer_handle_t handle, GstGralloc * gralloc);

G_END_DECLS

#endif /* __GST_NATIVE_BUFFER_H__ */
