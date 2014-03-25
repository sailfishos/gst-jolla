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

#include "gstanativewindowbufferpool.h"

#define container_of(ptr, type, member) ({ \
      const typeof( ((type *)0)->member ) *__mptr = (ptr); (type *)( (char *)__mptr - offsetof(type,member) );})

GST_DEBUG_CATEGORY_STATIC (gst_a_native_window_buffer_pool_debug_category);
#define GST_CAT_DEFAULT gst_a_native_window_buffer_pool_debug_category

enum {
  QOMX_COLOR_FormatYUV420PackedSemiPlanar64x32Tile2m8ka = 0x7FA30C03
};

typedef struct _GstANativeWindowBufferFormat
{
    int native_format;
    GstVideoFormat gst_format;
} GstANativeWindowBufferFormat;

static const GstANativeWindowBufferFormat gst_a_native_window_buffer_formats[] =
{
  { HAL_PIXEL_FORMAT_RGBA_8888   , GST_VIDEO_FORMAT_RGBA  },
  { HAL_PIXEL_FORMAT_RGBX_8888   , GST_VIDEO_FORMAT_RGBx  },
  { HAL_PIXEL_FORMAT_RGB_888     , GST_VIDEO_FORMAT_RGB   },
  { HAL_PIXEL_FORMAT_RGB_565     , GST_VIDEO_FORMAT_RGB16 },
  { HAL_PIXEL_FORMAT_BGRA_8888   , GST_VIDEO_FORMAT_BGRA  },
  { HAL_PIXEL_FORMAT_YV12        , GST_VIDEO_FORMAT_YV12  },
  { HAL_PIXEL_FORMAT_YCbCr_422_SP, GST_VIDEO_FORMAT_NV16  },
  { HAL_PIXEL_FORMAT_YCrCb_420_SP, GST_VIDEO_FORMAT_NV12  },
  { HAL_PIXEL_FORMAT_YCbCr_422_I , GST_VIDEO_FORMAT_YUY2  },
  { QOMX_COLOR_FormatYUV420PackedSemiPlanar64x32Tile2m8ka, GST_VIDEO_FORMAT_NV12_64Z32 }
};

typedef struct _GstANativeWindowBufferMemory GstANativeWindowBufferMemory;
typedef struct _GstANativeWindowBufferAllocator GstANativeWindowBufferAllocator;
typedef struct _GstANativeWindowBufferAllocatorClass GstANativeWindowBufferAllocatorClass;

struct _GstANativeWindowBufferMemory
{
  GstMemory mem;

  ANativeWindowBuffer_t window_buffer;
  GstVideoInfo video_info;
  gpointer map_data;
  int map_count;
  GstMapFlags map_flags;
};

struct _GstANativeWindowBufferAllocator
{
  GstAllocator parent;

  alloc_device_t *alloc_device;
};

struct _GstANativeWindowBufferAllocatorClass
{
  GstAllocatorClass parent_class;
};

static const gralloc_module_t *gst_a_native_window_buffer_allocator_module = NULL;

static void
gst_a_native_window_buffer_memory_increment_reference (
    android_native_base_t * buffer)
{
  GstANativeWindowBufferMemory *memory = container_of (
      (ANativeWindowBuffer_t *)buffer, GstANativeWindowBufferMemory,
      window_buffer);

  gst_memory_ref ((GstMemory *) memory);
}

static void
gst_a_native_window_buffer_memory_decrement_reference (
    android_native_base_t * buffer)
{
  GstANativeWindowBufferMemory *memory = container_of (
      (ANativeWindowBuffer_t *) buffer, GstANativeWindowBufferMemory,
      window_buffer);

  gst_memory_unref ((GstMemory *) memory);
}

static GstMemory *
gst_a_native_window_buffer_allocator_alloc (GstAllocator * alloc, gsize size,
    GstAllocationParams * params)
{
  g_assert_not_reached ();
  return NULL;
}

static void
gst_a_native_window_buffer_allocator_free (GstAllocator * alloc, GstMemory * mem)
{
  GstANativeWindowBufferAllocator * allocator
      = (GstANativeWindowBufferAllocator *) alloc;
  GstANativeWindowBufferMemory *memory = (GstANativeWindowBufferMemory *) mem;

  GST_DEBUG ("Freeing native buffer memory");

  allocator->alloc_device->free (allocator->alloc_device,
      memory->window_buffer.handle);

  g_slice_free (GstANativeWindowBufferMemory, memory);
}

static GstANativeWindowBufferMemory *
gst_a_native_window_buffer_memory_new (GstANativeWindowBufferAllocator * allocator,
    GstVideoInfo *video_info, int format, int usage)
{
  int result, stride;
  buffer_handle_t handle;
  GstANativeWindowBufferMemory *memory;

  result = allocator->alloc_device->alloc (allocator->alloc_device,
      video_info->width, video_info->height, format, usage, &handle, &stride);

  if (result) {
    GST_WARNING ("Unable to allocate buffer %d", result);
    return NULL;
  }

  memory = g_slice_new (GstANativeWindowBufferMemory);

  gst_video_info_set_format (&memory->video_info, video_info->finfo->format,
      stride, video_info->height);
  memory->video_info.width = video_info->width;
  memory->video_info.size = video_info->size;

  gst_memory_init (GST_MEMORY_CAST (memory), GST_MEMORY_FLAG_NO_SHARE,
      GST_ALLOCATOR(allocator), NULL, memory->video_info.size, 4, 0,
      memory->video_info.size);

  memory->window_buffer.common.magic = ANDROID_NATIVE_BUFFER_MAGIC;
  memory->window_buffer.common.version = sizeof (ANativeWindowBuffer_t);
  memset (memory->window_buffer.common.reserved, 0,
      sizeof (memory->window_buffer.common.reserved));
  memory->window_buffer.common.incRef
      = gst_a_native_window_buffer_memory_increment_reference;
  memory->window_buffer.common.decRef
      = gst_a_native_window_buffer_memory_decrement_reference;
  memory->window_buffer.width = video_info->width;
  memory->window_buffer.height = video_info->height;
  memory->window_buffer.format = format;
  memory->window_buffer.usage = usage;
  memory->window_buffer.handle = handle;
  memory->window_buffer.stride = stride;
  memory->map_data = NULL;
  memory->map_flags = 0;
  memory->map_count = 0;

  return memory;
}

static gpointer
gst_a_native_window_buffer_memory_map (GstMemory * mem, gsize maxsize,
    GstMapFlags flags)
{
  int result;
  GstANativeWindowBufferMemory *memory = (GstANativeWindowBufferMemory *) mem;
  gpointer data = memory->map_data;
  int usage = 0;

  if (flags & GST_MAP_READ)
    usage |= GRALLOC_USAGE_SW_READ_OFTEN;
  if (flags & GST_MAP_WRITE)
    usage |= GRALLOC_USAGE_SW_WRITE_OFTEN;

  if (memory->map_count > 0) {
    if (memory->map_flags != flags) {
      GST_ERROR ("Tried to lock buffer with different flags");
      return NULL;
    }
  } else {
    result = gst_a_native_window_buffer_allocator_module->lock (
        gst_a_native_window_buffer_allocator_module, memory->window_buffer.handle,
        usage, 0, 0, memory->window_buffer.width, memory->window_buffer.height,
        &data);
    if (result) {
      GST_ERROR ("Failed to lock buffer %d", result);
      return NULL;
    }
  }

  memory->map_data = data;
  memory->map_count += 1;
  memory->map_flags = flags;

  return data;
}

static void
gst_a_native_window_buffer_memory_unmap (GstMemory * mem)
{
  GstANativeWindowBufferMemory *memory = (GstANativeWindowBufferMemory *) mem;

  if (memory->map_count > 0 && (memory->map_count -= 1) == 0) {
    memory->map_data = NULL;
    gst_a_native_window_buffer_allocator_module->unlock
        (gst_a_native_window_buffer_allocator_module,
        memory->window_buffer.handle);
  }
}

static GstMemory *
gst_a_native_window_buffer_memory_share (GstMemory * mem, gssize offset,
    gssize size)
{
  g_assert_not_reached ();
  return NULL;
}

static GstMemory *
gst_a_native_window_buffer_memory_copy (GstMemory *mem, gssize offset,
    gssize size)
{
  GstANativeWindowBufferMemory *memory = (GstANativeWindowBufferMemory *) mem;
  GstMemory *copy;
  GstMapInfo src, dest;

  if (!gst_memory_map (mem, &src, GST_MAP_READ)) {
    GST_WARNING ("Failed to map source buffer for copy");
    return NULL;
  }

  copy = (GstMemory *) gst_a_native_window_buffer_memory_new (
      (GstANativeWindowBufferAllocator *) memory->mem.allocator,
      &memory->video_info, memory->window_buffer.format,
      memory->window_buffer.usage);

  if (!copy) {
    GST_WARNING ("Failed to allocate destination buffer for copy");
  } else if (gst_memory_map (copy, &dest, GST_MAP_WRITE)) {
    memcpy (dest.data, src.data, dest.size);

    gst_memory_unmap (copy, &dest);
  } else {
    GST_WARNING ("Failed to map destination buffer for copy");
    gst_memory_unref (copy);
    copy = NULL;
  }

  gst_memory_unmap (mem, &src);

  return copy;
}

ANativeWindowBuffer_t *
gst_a_native_window_buffer_memory_get_buffer (GstMemory *mem)
{
  GstANativeWindowBufferMemory *memory = (GstANativeWindowBufferMemory *) mem;

  return &memory->window_buffer;
}

GType gst_a_native_window_buffer_allocator_get_type (void);
G_DEFINE_TYPE (GstANativeWindowBufferAllocator,
    gst_a_native_window_buffer_allocator, GST_TYPE_ALLOCATOR);

#define GST_TYPE_A_NATIVE_WINDOW_BUFFER_ALLOCATOR      (gst_a_native_window_buffer_allocator_get_type())
#define GST_IS_A_NATIVE_WINDOW_BUFFER_ALLOCATOR(obj)   (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GST_TYPE_A_NATIVE_WINDOW_BUFFER_ALLOCATOR))
#define GST_A_NATIVE_WINDOW_BUFFER_ALLOCATOR(obj)      (G_TYPE_CHECK_INSTANCE_CAST ((obj), GST_TYPE_A_NATIVE_WINDOW_BUFFER_ALLOCATOR, GstANativeWindowBufferAllocator))
#define GST_A_NATIVE_WINDOW_BUFFER_ALLOCATOR_CAST(obj) ((GstANativeWindowBufferAllocator*) (obj))

static void
gst_a_native_window_buffer_allocator_finalize (GObject * object)
{
  GstANativeWindowBufferAllocator * allocator = GST_A_NATIVE_WINDOW_BUFFER_ALLOCATOR
      (object);

  if (allocator->alloc_device) {
    gralloc_close (allocator->alloc_device);
  }

  G_OBJECT_CLASS (gst_a_native_window_buffer_allocator_parent_class)->finalize
      (object);
}

static void
gst_a_native_window_buffer_allocator_class_init (
    GstANativeWindowBufferAllocatorClass * klass)
{
  int result;
  GObjectClass *gobject_class = (GObjectClass *) klass;
  GstAllocatorClass *allocator_class = (GstAllocatorClass *) klass;

  gobject_class->finalize = gst_a_native_window_buffer_allocator_finalize;

  allocator_class->alloc = gst_a_native_window_buffer_allocator_alloc;
  allocator_class->free = gst_a_native_window_buffer_allocator_free;

  result = hw_get_module(GRALLOC_HARDWARE_MODULE_ID,
      (const hw_module_t **) (&gst_a_native_window_buffer_allocator_module));
  if (result) {
    GST_ERROR ("Failed to acquire gralloc module: %d", result);
  }
}

static void
gst_a_native_window_buffer_allocator_init (
    GstANativeWindowBufferAllocator * allocator)
{
  int result;
  GstAllocator *alloc = GST_ALLOCATOR_CAST (allocator);

  alloc->mem_type = GST_A_NATIVE_WINDOW_BUFFER_MEMORY_TYPE;
  alloc->mem_map = gst_a_native_window_buffer_memory_map;
  alloc->mem_unmap = gst_a_native_window_buffer_memory_unmap;
  alloc->mem_share = gst_a_native_window_buffer_memory_share;
  alloc->mem_copy = gst_a_native_window_buffer_memory_copy;

  GST_OBJECT_FLAG_SET (allocator, GST_ALLOCATOR_FLAG_CUSTOM_ALLOC);

  allocator->alloc_device = 0;
  if (gst_a_native_window_buffer_allocator_module) {
    result = gralloc_open (
        (const hw_module_t *) gst_a_native_window_buffer_allocator_module,
        &allocator->alloc_device);
    if (result) {
      GST_ERROR ("Failed to open gralloc: %d", result);
    }
  }
}

#define GST_A_NATIVE_WINDOW_BUFFER_POOL_GET_PRIVATE(obj)  \
    (G_TYPE_INSTANCE_GET_PRIVATE ((obj), GST_TYPE_A_NATIVE_WINDOW_BUFFER_POOL, \
    GstANativeWindowBufferPoolPrivate))

struct _GstANativeWindowBufferPoolPrivate
{
  GstANativeWindowBufferAllocator *allocator;

  GstVideoInfo video_info;

  int format;
  int usage;
};

static void gst_a_native_window_buffer_pool_finalize (GObject *object);

G_DEFINE_TYPE (GstANativeWindowBufferPool, gst_a_native_window_buffer_pool,
    GST_TYPE_BUFFER_POOL);

static const gchar **
gst_a_native_window_buffer_pool_get_options (GstBufferPool * bpool)
{
  static const gchar *options[] = { GST_BUFFER_POOL_OPTION_VIDEO_META, NULL };

  return options;
}

static gboolean
gst_a_native_window_buffer_pool_set_config (GstBufferPool * bpool,
    GstStructure * config)
{
  GstCaps *caps;
  gint usage;
  gint format_index, format_count;
  guint size;
  GstAllocationParams params = { GST_MEMORY_FLAG_NO_SHARE, 4,  0, 0 };
  GstANativeWindowBufferPool *pool = GST_A_NATIVE_WINDOW_BUFFER_POOL (bpool);

  if (!gst_buffer_pool_config_get_params (config, &caps, &size, NULL, NULL)) {
    GST_WARNING_OBJECT (pool, "Invalid pool configuration");
    return FALSE;
  }

  if (!gst_structure_get_int (config, "usage", &usage) || usage == 0) {
    usage = GRALLOC_USAGE_SW_READ_OFTEN | GRALLOC_USAGE_SW_WRITE_OFTEN;
  }

  if (caps == NULL) {
    GST_WARNING_OBJECT (pool, "Null caps");
    return FALSE;
  }

  GST_OBJECT_LOCK (pool);

  if (!gst_video_info_from_caps (&pool->priv->video_info, caps)) {
    GST_OBJECT_UNLOCK (pool);
    GST_WARNING_OBJECT (pool, "Invalid video caps %" GST_PTR_FORMAT, caps);
    return FALSE;
  }

  pool->priv->video_info.size = size;

  format_count = sizeof (gst_a_native_window_buffer_formats)
      / sizeof (GstANativeWindowBufferFormat);

  for (format_index = 0; format_index < format_count; ++format_index) {
    if (gst_a_native_window_buffer_formats[format_index].gst_format
        == pool->priv->video_info.finfo->format) {
      break;
    }
  }

  if (format_index == format_count) {
    GST_WARNING_OBJECT (pool, "Video format unsupported %" GST_PTR_FORMAT, caps);
    return FALSE;
  }

  pool->priv->format = gst_a_native_window_buffer_formats[format_index].native_format;
  pool->priv->usage = usage | GRALLOC_USAGE_SW_READ_OFTEN
      | GRALLOC_USAGE_SW_WRITE_OFTEN | GRALLOC_USAGE_HW_TEXTURE;

  GST_OBJECT_UNLOCK (pool);

  GST_DEBUG_OBJECT (pool, "Configured pool. usage: %x caps: %"
      GST_PTR_FORMAT, usage, caps);

  gst_buffer_pool_config_set_allocator (config,
      (GstAllocator *) pool->priv->allocator, &params);

  return GST_BUFFER_POOL_CLASS (gst_a_native_window_buffer_pool_parent_class)
      ->set_config (bpool, config);
}

static GstFlowReturn
gst_a_native_window_buffer_pool_alloc_buffer (GstBufferPool * bpool,
    GstBuffer ** buf, GstBufferPoolAcquireParams * params)
{
  GstBuffer *buffer;
  GstVideoMeta *video_meta;
  GstANativeWindowBufferMemory *memory;
  GstANativeWindowBufferPool *pool = GST_A_NATIVE_WINDOW_BUFFER_POOL (bpool);

  GST_DEBUG_OBJECT (pool, "allocating buffer w: %d, h: %d, f: %x, u: %x",
      pool->priv->video_info.width, pool->priv->video_info.height,
      pool->priv->format, pool->priv->usage);

  memory = gst_a_native_window_buffer_memory_new (pool->priv->allocator,
      &pool->priv->video_info, pool->priv->format, pool->priv->usage);

  if (!memory) {
    return GST_FLOW_ERROR;
  }

  buffer = gst_buffer_new ();
  gst_buffer_append_memory (buffer, (GstMemory *) memory);

  video_meta = gst_buffer_add_video_meta_full (buffer,
      GST_VIDEO_FRAME_FLAG_NONE, memory->video_info.finfo->format,
      memory->video_info.width, memory->video_info.height,
      memory->video_info.finfo->n_planes, memory->video_info.offset,
      memory->video_info.stride);
  GST_META_FLAG_SET ((GstMeta *) video_meta, GST_META_FLAG_POOLED);

  *buf = buffer;

  return GST_FLOW_OK;
}

static void
gst_a_native_window_buffer_pool_finalize (GObject * object)
{
  GstANativeWindowBufferPool *pool = GST_A_NATIVE_WINDOW_BUFFER_POOL (object);

  gst_object_unref (pool->priv->allocator);
  pool->priv->allocator = 0;

  G_OBJECT_CLASS (gst_a_native_window_buffer_pool_parent_class)->finalize (object);
}

static void
gst_a_native_window_buffer_pool_class_init (GstANativeWindowBufferPoolClass * klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;
  GstBufferPoolClass *gstbufferpool_class = (GstBufferPoolClass *) klass;

  g_type_class_add_private (klass, sizeof (GstANativeWindowBufferPoolPrivate));

  gobject_class->finalize = gst_a_native_window_buffer_pool_finalize;
  gstbufferpool_class->get_options = gst_a_native_window_buffer_pool_get_options;
  gstbufferpool_class->set_config = gst_a_native_window_buffer_pool_set_config;
  gstbufferpool_class->alloc_buffer = gst_a_native_window_buffer_pool_alloc_buffer;

  GST_DEBUG_CATEGORY_INIT (gst_a_native_window_buffer_pool_debug_category,
      "anativewindowbuffer", 0, "Android native window buffer pool");
}

static void
gst_a_native_window_buffer_pool_init (GstANativeWindowBufferPool * pool)
{
  pool->priv = GST_A_NATIVE_WINDOW_BUFFER_POOL_GET_PRIVATE (pool);

  pool->priv->allocator = g_object_new
      (gst_a_native_window_buffer_allocator_get_type(), NULL);
  pool->priv->format = 0;
  pool->priv->usage = 0;
}

GstBufferPool *
gst_a_native_window_buffer_pool_new (void)
{
  GstANativeWindowBufferPool *pool;

  pool = g_object_new (gst_a_native_window_buffer_pool_get_type (), NULL);

  if (!pool->priv->allocator->alloc_device) {
    gst_object_unref (pool);
    return NULL;
  }

  return GST_BUFFER_POOL (pool);
}
