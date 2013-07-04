#ifndef PLAYER_H
#define PLAYER_H

#include "openglwindow.h"
#include <gst/gst.h>
#include <EGL/egl.h>

class QOpenGLShaderProgram;

typedef void *EGLSyncKHR;
typedef EGLSyncKHR(EGLAPIENTRYP PFNEGLCREATESYNCKHRPROC)(EGLDisplay dpy, EGLenum type,
                                                          const EGLint *attrib_list);

class Player : public OpenGLWindow {
public:
  Player();

  void initialize();
  void render();

  void setSink(GstElement *sink);
  void setPipeline(GstElement *pipeline);

  bool start();

private:
  GLuint loadShader(GLenum type, const char *source);

  static void on_frame_ready(GstElement *sink, gint frame, gpointer data);
  static gboolean bus_callback(GstBus * bus, GstMessage * msg, gpointer data);

  GLuint m_inputVertex;
  GLuint m_textureCoord;
  GLuint m_matrixUniform;
  QOpenGLShaderProgram *m_program;

  GstElement *m_sink, *m_pipeline;

  bool m_hasFrame;

  GLuint m_texture;

  EGLDisplay m_dpy;
  PFNGLEGLIMAGETARGETTEXTURE2DOESPROC m_glEGLImageTargetTexture2DOES;
  PFNEGLCREATESYNCKHRPROC m_eglCreateSyncKHR;
};

#endif /* PLAYER_H */
