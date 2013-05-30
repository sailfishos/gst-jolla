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

#include "gstgralloc.h"

G_DEFINE_TYPE (GstGralloc, gst_gralloc, GST_TYPE_MINI_OBJECT);

static GstGrallocClass *parent_class;

static void gst_gralloc_finalize (GstGralloc * gralloc);

static void
gst_gralloc_class_init (GstGrallocClass * gralloc_class)
{
  GstMiniObjectClass *mo_class = GST_MINI_OBJECT_CLASS (gralloc_class);

  parent_class = g_type_class_peek_parent (gralloc_class);

  mo_class->finalize = (GstMiniObjectFinalizeFunction) gst_gralloc_finalize;
}

static void
gst_gralloc_init (GstGralloc * instance)
{
  instance->gralloc = NULL;
  instance->allocator = NULL;
}

static void
gst_gralloc_finalize (GstGralloc * gralloc)
{
  int err = gralloc_close (gralloc->allocator);
  if (err != 0) {
    g_warning ("Failed to close gralloc");
  }

  gralloc->gralloc = NULL;
  gralloc->allocator = NULL;
}

GstGralloc *
gst_gralloc_new ()
{
  hw_module_t *hwmod = NULL;
  gralloc_module_t *gralloc_module = NULL;
  alloc_device_t *alloc_device = NULL;
  GstGralloc *gralloc = NULL;

  int err =
      hw_get_module (GRALLOC_HARDWARE_MODULE_ID, (const hw_module_t **) &hwmod);

  if (err != 0) {
    g_warning ("Failed to load gralloc");
    goto out;
  }

  gralloc_module = (gralloc_module_t *) hwmod;
  err = gralloc_open ((const hw_module_t *) gralloc_module, &alloc_device);
  if (err != 0) {
    g_warning ("Failed to open gralloc");
    goto out;
  }

  gralloc = (GstGralloc *) gst_mini_object_new (GST_TYPE_GRALLOC);
  gralloc->gralloc = gralloc_module;
  gralloc->allocator = alloc_device;

out:
  return gralloc;
}

buffer_handle_t
gst_gralloc_allocate (GstGralloc * gralloc, int width, int height,
    int format, int usage, int *stride)
{
  buffer_handle_t handle = NULL;

  int err = gralloc->allocator->alloc (gralloc->allocator,
      width, height, format, usage, &handle, stride);
  if (err != 0) {
    g_warning ("Failed to allocate buffer");
  }

  return handle;
}

void
gst_gralloc_free (GstGralloc * gralloc, buffer_handle_t handle)
{
  gralloc->allocator->free (gralloc->allocator, handle);
}
