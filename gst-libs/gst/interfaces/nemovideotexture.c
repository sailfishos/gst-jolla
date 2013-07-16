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

/**
 * SECTION:nemo_gst_video_texture
 * @short_description Interface for video rendering
 *
 * This is an interface which enables integrating video rendering into any OpenGL ES 2.0
 * application.
 */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "nemovideotexture.h"

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
    /**
     * NemoGstVideoTextureClass::frame-ready
     *
     * Triggered when the sink element implementing this interface eith has
     * a frame for the application to render or lost its frame.
     * frame is an opaque frame identifier.
     * frame will be -1 if the sink element has lost its frame
     * or will be 0 or greater if there is a frame available for rendering.
     *
     * If frame is 0 or more, application should call nemo_gst_video_texture_acquire_frame()
     * if its interested in rendering.
     * If frame is -1 then application should release any acquired frames.
     */
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
  /**
   * NemoGstVideoTextureClass::egl-display
   * EGL Display
   * Application must provide a valid EGL display to the sink before attempting
   * to call nemo_gst_video_texture_acquire_frame()
   */
  g_object_interface_install_property (klass,
      g_param_spec_pointer ("egl-display",
          "EGL display ",
          "The application provided EGL display to be used for creating EGLImageKHR objects.",
          G_PARAM_READWRITE));
}

/**
 * nemo_gst_video_texture_acquire_frame:
 * @iface: #NemoGstVideoTexture of a GStreamer element
 *
 * Used to inform the sink that the application wants to render a frame.
 * The sink should do any needed steps in order to prepare for rendering
 *
 * Returns: %TRUE on success and %FALSE on failure.
 */
gboolean
nemo_gst_video_texture_acquire_frame (NemoGstVideoTexture * iface)
{
  NemoGstVideoTextureClass *klass = NEMO_GST_VIDEO_TEXTURE_GET_CLASS (iface);

  if (klass->acquire_frame) {
    return klass->acquire_frame (iface);
  }

  return FALSE;
}

/**
 * nemo_gst_video_texture_bind_frame:
 * @iface: #NemoGstVideoTexture of a GStreamer element
 *
 * The sink will acquire the needed EGLImageKHR corresponding to the
 * latest available frame for rendering. If the function returns TRUE then
 * @image will carry the address of the EGLImageKHR corresponding to the latest frame.
 *
 * The sink ensures that the pointer remains valid until the application unbinds the frame.
 * The application ensures that only one EGLImageKHR will be bound at a time.
 *
 * Application might get a different EGLImageKHR everytime this function gets called.
 * Application should not try to use the pointer address in a hash or something.
 *
 * The time between bind and unbind should be as minimal as possible. Ideally it should
 * be done at vsync to minimize the time of acquisition.
 *
 * Returns: %TRUE on success and %FALSE on failure.
 */
gboolean
nemo_gst_video_texture_bind_frame (NemoGstVideoTexture * iface,
    EGLImageKHR * image)
{
  NemoGstVideoTextureClass *klass = NEMO_GST_VIDEO_TEXTURE_GET_CLASS (iface);

  if (klass->bind_frame) {
    return klass->bind_frame (iface, image);
  }

  return FALSE;
}

/**
 * nemo_gst_video_texture_unbind_frame:
 * @iface: #NemoGstVideoTexture of a GStreamer element
 *
 * This call is used to inform the sink that the application is not
 * interested in using the EGLImageKHR obtained from calling nemo_gst_video_texture_bind_frame
 * anymore.
 * Application must not access the EGLImageKHR pointer obtained by calling
 * nemo_gst_video_texture_bind_frame anymore. Failing to do so will result in a crash.
 */
void
nemo_gst_video_texture_unbind_frame (NemoGstVideoTexture * iface)
{
  NemoGstVideoTextureClass *klass = NEMO_GST_VIDEO_TEXTURE_GET_CLASS (iface);

  if (klass->unbind_frame) {
    klass->unbind_frame (iface);
  }
}

/**
  * nemo_gst_video_texture_release_frame:
  * @iface: #NemoGstVideoTexture of a GStreamer element
  * @sync: an optional fence object
  *
  * Called by the application to release a frame after rendering
  * it via the GPU.
  * The application is expected to use the
  * EGL_KHR_sync_fence extension to place a fence into the
  * GPU command stream after the rendering commands
  * accessing the video frame.
  * This sync object should be given as @sync, and the sink
  * assumes ownership of that sync object.
  *
  * If @sync is not %NULL, then the sink implementation
  * asynchronously waits until the specified fence
  * object is signalled before reusing the image.
  */
void
nemo_gst_video_texture_release_frame (NemoGstVideoTexture * iface,
    EGLSyncKHR sync)
{
  NemoGstVideoTextureClass *klass = NEMO_GST_VIDEO_TEXTURE_GET_CLASS (iface);

  if (klass->release_frame) {
    klass->release_frame (iface, sync);
  }
}

/**
 * nemo_gst_video_texture_frame_ready:
 * @iface: #NemoGstVideoTexture of a GStreamer element
 * @frame: frame number
 *
 * Called by the elements implementing the #NemoGstVideoTexture interface
 * to trigger the frame-ready signal emission.
 */
void
nemo_gst_video_texture_frame_ready (NemoGstVideoTexture * iface, gint frame)
{
  g_return_if_fail (NEMO_GST_IS_VIDEO_TEXTURE (iface));

  g_signal_emit (G_OBJECT (iface),
      nemo_gst_video_texture_iface_signals[SIGNAL_FRAME_READY], 0, frame);
}
