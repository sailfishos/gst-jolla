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

#ifndef __GST_DROID_EGL_SINK_H__
#define __GST_DROID_EGL_SINK_H__

#include <gst/gst.h>
#include <gst/video/video.h>
#include <gst/video/gstvideosink.h>
#include "gstgralloc.h"
#include "gstnativebuffer.h"
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>

G_BEGIN_DECLS

#define GST_TYPE_DROID_EGL_SINK \
  (gst_droid_egl_sink_get_type())
#define GST_DROID_EGL_SINK(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_DROID_EGL_SINK,GstDroidEglSink))
#define GST_DROID_EGL_SINK_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_DROID_EGL_SINK,GstDroidEglSinkClass))
#define GST_IS_DROID_EGL_SINK(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_DROID_EGL_SINK))
#define GST_IS_DROID_EGL_SINK_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_DROID_EGL_SINK))

typedef struct _GstDroidEglSink GstDroidEglSink;
typedef struct _GstDroidEglSinkClass GstDroidEglSinkClass;

typedef EGLint(EGLAPIENTRYP PFNEGLCLIENTWAITSYNCKHRPROC)(EGLDisplay dpy, EGLSyncKHR sync,
							 EGLint flags, EGLTimeKHR timeout);
typedef EGLBoolean(EGLAPIENTRYP PFNEGLDESTROYSYNCKHRORIC)(EGLDisplay dpy, EGLSyncKHR sync);

struct _GstDroidEglSink {
  GstVideoSink parent;

  GstGralloc *gralloc;

  int fps_n;
  int fps_d;

  GstVideoFormat format;
  int hal_format;

  GPtrArray *buffers;

  EGLDisplay dpy;

  GstNativeBuffer *last_buffer;
  GstNativeBuffer *acquired_buffer;
  GMutex buffer_lock;

  EGLSyncKHR sync;
  EGLImageKHR image;

  GQueue *free_buffers;

  PFNEGLCREATEIMAGEKHRPROC eglCreateImageKHR;
  PFNEGLDESTROYIMAGEKHRPROC eglDestroyImageKHR;
  PFNEGLDESTROYSYNCKHRORIC eglDestroySyncKHR;
  PFNEGLCLIENTWAITSYNCKHRPROC eglClientWaitSyncKHR;
};

struct _GstDroidEglSinkClass {
  GstVideoSinkClass parent_class;
};

GType gst_droid_egl_sink_get_type (void);

G_END_DECLS

#endif /* __GST_DROID_EGL_SINK_H__ */
