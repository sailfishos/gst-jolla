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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "neemovideotexture.h"

enum
{
  SIGNAL_FRAME_READY,
  LAST_SIGNAL
};

static guint nemo_gst_video_texture_iface_signals[LAST_SIGNAL] = { 0 };

static void nemo_gst_video_texture_iface_base_init (NemoGstVideoTextureClass *
    klass);
static void nemo_gst_video_texture_iface_class_init (NemoGstVideoTextureClass *
    klass);

GType
nemo_gst_video_texture_get_type (void)
{
  static GType nemo_gst_video_texture_type = 0;

  if (!nemo_gst_video_texture_type) {
    static const GTypeInfo nemo_gst_video_texture_info = {
      sizeof (NemoGstVideoTextureClass),
      (GBaseInitFunc) nemo_gst_video_texture_iface_base_init,   /* base_init */
      NULL,                     /* base_finalize */
      (GClassInitFunc) nemo_gst_video_texture_iface_class_init, /* class_init */
      NULL,                     /* class_finalize */
      NULL,                     /* class_data */
      0,
      0,                        /* n_preallocs */
      NULL,                     /* instance_init */
    };

    nemo_gst_video_texture_type = g_type_register_static (G_TYPE_INTERFACE,
        "NemoGstVideoTexture", &nemo_gst_video_texture_info, 0);
    g_type_interface_add_prerequisite (nemo_gst_video_texture_type,
        GST_TYPE_IMPLEMENTS_INTERFACE);
  }

  return nemo_gst_video_texture_type;
}

static void
nemo_gst_video_texture_iface_base_init (NemoGstVideoTextureClass * klass)
{
  static gboolean initialized = FALSE;

  if (!initialized) {
    nemo_gst_video_texture_iface_signals[SIGNAL_FRAME_READY] =
        g_signal_new ("frame-ready",
        G_TYPE_FROM_CLASS (klass),
        G_SIGNAL_RUN_LAST,
        0, NULL, NULL,
        g_cclosure_marshal_VOID__INT, G_TYPE_NONE, 1, G_TYPE_INT);

    initialized = TRUE;
  }

  klass->acquire_frame = NULL;
  klass->bind_frame = NULL;
  klass->unbind_frame = NULL;
  klass->release_frame = NULL;
}

static void
nemo_gst_video_texture_iface_class_init (NemoGstVideoTextureClass * klass)
{
  g_object_interface_install_property (klass,
      g_param_spec_pointer ("egl-display",
          "EGL display ",
          "The application provided EGL display to be used for creating EGLImageKHR objects.",
          G_PARAM_READWRITE));
}

gboolean
neemo_gst_video_texture_acquire_frame (NemoGstVideoTexture * iface)
{
  NemoGstVideoTextureClass *klass = NEMO_GST_VIDEO_TEXTURE_GET_CLASS (iface);

  if (klass->acquire_frame) {
    return klass->acquire_frame (iface);
  }

  return FALSE;
}

gboolean
neemo_gst_video_texture_bind_frame (NemoGstVideoTexture * iface,
    EGLImageKHR * image)
{
  NemoGstVideoTextureClass *klass = NEMO_GST_VIDEO_TEXTURE_GET_CLASS (iface);

  if (klass->bind_frame) {
    return klass->bind_frame (iface, image);
  }

  return FALSE;
}

void
neemo_gst_video_texture_unbind_frame (NemoGstVideoTexture * iface)
{
  NemoGstVideoTextureClass *klass = NEMO_GST_VIDEO_TEXTURE_GET_CLASS (iface);

  if (klass->unbind_frame) {
    klass->unbind_frame (iface);
  }
}

void
neemo_gst_video_texture_release_frame (NemoGstVideoTexture * iface,
    EGLSyncKHR sync)
{
  NemoGstVideoTextureClass *klass = NEMO_GST_VIDEO_TEXTURE_GET_CLASS (iface);

  if (klass->release_frame) {
    klass->release_frame (iface, sync);
  }
}

void
nemo_gst_video_texture_frame_ready (NemoGstVideoTexture * iface, gint frame)
{
  g_return_if_fail (NEMO_GST_IS_VIDEO_TEXTURE (iface));

  g_signal_emit (G_OBJECT (iface),
      nemo_gst_video_texture_iface_signals[SIGNAL_FRAME_READY], 0, frame);
}
