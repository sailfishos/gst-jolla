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

#ifndef __GST_DROID_BUFFER_UNPACK_H__
#define __GST_DROID_BUFFER_UNPACK_H__

#include <gst/gst.h>
#include <gst/base/gstbasetransform.h>

G_BEGIN_DECLS

#define GST_TYPE_DROID_BUFFER_UNPACK \
  (gst_droid_buffer_unpack_get_type())
#define GST_DROID_BUFFER_UNPACK(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_DROID_BUFFER_UNPACK,GstDroidBufferUnpack))
#define GST_DROID_BUFFER_UNPACK_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_DROID_BUFFER_UNPACK,GstDroidBufferUnpackClass))
#define GST_IS_DROID_BUFFER_UNPACK(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_DROID_BUFFER_UNPACK))
#define GST_IS_DROID_BUFFER_UNPACK_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_DROID_BUFFER_UNPACK))
#define GST_DROID_BUFFER_UNPACK_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), GST_TYPE_DROID_BUFFER_UNPACK, GstDroidBufferUnpackClass))

typedef struct _GstDroidBufferUnpack GstDroidBufferUnpack;
typedef struct _GstDroidBufferUnpackClass GstDroidBufferUnpackClass;

struct _GstDroidBufferUnpack {
  GstBaseTransform parent;
};

struct _GstDroidBufferUnpackClass {
  GstBaseTransformClass parent_class;
};

GType gst_droid_buffer_unpack_get_type (void);

G_END_DECLS

#endif /* __GST_DROID_BUFFER_UNPACK_H__ */
