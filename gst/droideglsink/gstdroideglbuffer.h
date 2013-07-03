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

#ifndef __GST_DROID_EGL_BUFFER_H__
#define __GST_DROID_EGL_BUFFER_H__

#include <gst/gst.h>
#include "gstnativebuffer.h"

G_BEGIN_DECLS

typedef struct _GstDroidEglBuffer GstDroidEglBuffer;
typedef struct _GstDroidEglBufferClass GstDroidEglBufferClass;

#define GST_TYPE_DROID_EGL_BUFFER            (gst_droid_egl_buffer_get_type())
#define GST_IS_DROID_EGL_BUFFER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GST_TYPE_DROID_EGL_BUFFER))
#define GST_IS_DROID_EGL_BUFFER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GST_TYPE_DROID_EGL_BUFFER))
#define GST_DROID_EGL_BUFFER_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GST_TYPE_DROID_EGL_BUFFER, GstDroidEglBufferClass))
#define GST_DROID_EGL_BUFFER(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GST_TYPE_DROID_EGL_BUFFER, GstDroidEglBuffer))
#define GST_DROID_EGL_BUFFER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GST_TYPE_DROID_EGL_BUFFER, GstDroidEglBufferClass))

typedef void *EGLImageKHR;
typedef void *EGLSyncKHR;

struct _GstDroidEglBuffer {
  GstBuffer parent;

  GstNativeBuffer *buffer;
  struct ANativeWindowBuffer native;
  EGLImageKHR image;
  EGLSyncKHR sync;
};

struct _GstDroidEglBufferClass {
  GstBufferClass parent_class;
};

GstDroidEglBuffer *gst_droid_egl_buffer_new ();

G_END_DECLS

#endif /* __GST_DROID_EGL_BUFFER_H__ */
