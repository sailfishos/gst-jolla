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

#ifndef __GST_GRALLOC_H__
#define __GST_GRALLOC_H__

#include <gst/gstminiobject.h>
#include <hardware/gralloc.h>

G_BEGIN_DECLS

typedef struct _GstGralloc GstGralloc;
typedef struct _GstGrallocClass GstGrallocClass;

#define GST_TYPE_GRALLOC            (gst_gralloc_get_type())

struct _GstGralloc {
  GstMiniObject parent;
  alloc_device_t *allocator;
  gralloc_module_t *gralloc;
};

struct _GstGrallocClass {
  GstMiniObjectClass  parent_class;
};

GType         gst_gralloc_get_type           (void);

GstGralloc*   gst_gralloc_new                ();

G_INLINE_FUNC GstGralloc * gst_gralloc_ref (GstGralloc * gralloc);

static inline GstGralloc *
gst_gralloc_ref (GstGralloc * gralloc)
{
  return (GstGralloc *) gst_mini_object_ref (GST_MINI_OBJECT_CAST (gralloc));
}

G_INLINE_FUNC void gst_gralloc_unref (GstGralloc * gralloc);

static inline void
gst_gralloc_unref (GstGralloc * gralloc)
{
  gst_mini_object_unref (GST_MINI_OBJECT_CAST (gralloc));
}

buffer_handle_t gst_gralloc_allocate (GstGralloc *gralloc, int width, int height,
				      int format, int usage, int *stride);
void gst_gralloc_free (GstGralloc *gralloc, buffer_handle_t handle);

G_END_DECLS

#endif /* __GST_NATIVE_BUFFER_H__ */
