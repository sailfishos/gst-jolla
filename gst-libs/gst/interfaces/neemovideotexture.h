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

#ifndef __NEEMO_GST_VIDEO_TEXTURE_H__
#define __NEEMO_GST_VIDEO_TEXTURE_H__

#include <gst/gst.h>

G_BEGIN_DECLS

#define NEMO_GST_TYPE_VIDEO_TEXTURE \
  (nemo_gst_video_texture_get_type ())
#define NEMO_GST_VIDEO_TEXTURE(obj) \
  (GST_IMPLEMENTS_INTERFACE_CHECK_INSTANCE_CAST ((obj), NEMO_GST_TYPE_VIDEO_TEXTURE, \
                                                 NemoGstVideoTexture))
#define NEMO_GST_VIDEO_TEXTURE_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), NEMO_GST_TYPE_VIDEO_TEXTURE, NemoGstVideoTextureClass))
#define NEMO_GST_IS_VIDEO_TEXTURE(obj) \
  (GST_IMPLEMENTS_INTERFACE_CHECK_INSTANCE_TYPE ((obj), NEMO_GST_TYPE_VIDEO_TEXTURE))
#define NEMO_GST_IS_VIDEO_TEXTURE_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), NEMO_GST_TYPE_VIDEO_TEXTURE))
#define NEMO_GST_VIDEO_TEXTURE_GET_CLASS(inst) \
  (G_TYPE_INSTANCE_GET_INTERFACE ((inst), NEMO_GST_TYPE_VIDEO_TEXTURE, NemoGstVideoTextureClass))

typedef void * EGLImageKHR;
typedef void * EGLSyncKHR;

typedef struct _NemoGstVideoTexture NemoGstVideoTexture;
typedef struct _NemoGstVideoTextureClass NemoGstVideoTextureClass;

struct _NemoGstVideoTextureClass
{
  GTypeInterface parent_klass;

  /* interface methods */
  gboolean (* acquire_frame) (NemoGstVideoTexture *iface);
  gboolean (* bind_frame) (NemoGstVideoTexture *iface, EGLImageKHR *image);
  void (* unbind_frame) (NemoGstVideoTexture *iface);
  void (* release_frame) (NemoGstVideoTexture *iface, EGLSyncKHR sync);

  /*< private >*/
  gpointer                 _gst_reserved[GST_PADDING];
};

GType   nemo_gst_video_texture_get_type          (void);

/* virtual class function wrappers */

gboolean neemo_gst_video_texture_acquire_frame (NemoGstVideoTexture *iface);
gboolean neemo_gst_video_texture_bind_frame (NemoGstVideoTexture *iface, EGLImageKHR *image);
void     neemo_gst_video_texture_unbind_frame (NemoGstVideoTexture *iface);
void     neemo_gst_video_texture_release_frame (NemoGstVideoTexture *iface, EGLSyncKHR sync);

/* trigger signals */
void     nemo_gst_video_texture_frame_ready (NemoGstVideoTexture *iface, gint frame);

G_END_DECLS

#endif /* __NEEMO_GST_VIDEO_TEXTURE_H__ */
