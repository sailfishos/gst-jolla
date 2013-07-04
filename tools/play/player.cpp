#include <QtGui/QOpenGLShaderProgram>
#include <QtGui/QGuiApplication>
#include "player.h"
#include <gst/interfaces/nemovideotexture.h>

#define EGL_SYNC_FENCE_KHR                      0x30F9

static const char *vertexShaderSource =
    "attribute highp vec4 inputVertex;"
    "attribute lowp vec2 textureCoord;"
    "uniform highp mat4 matrix;"
    "varying lowp vec2 fragTexCoord;"
    ""
    "void main() {"
    "  gl_Position = matrix * inputVertex;"
    "  fragTexCoord = textureCoord;"
    "}"
  "";

static const char *fragmentShaderSource =
    "#extension GL_OES_EGL_image_external: enable\n"
    "uniform samplerExternalOES texture0;"
    "varying lowp vec2 fragTexCoord;"
    "void main() {"
    "  gl_FragColor = texture2D(texture0, fragTexCoord);"
    "}"
  "";

float texCoords[8] = {
  0.0, 1.0,
  1.0, 1.0,
  1.0, 0.0,
  0.0, 0.0
};

float vertexCoords[8] = {
  -1.0, 1.0,
  -1.0, -1.0,
  1.0, -1.0,
  1.0, 1.0,
};

Player::Player() :
  m_inputVertex(0),
  m_textureCoord(0),
  m_matrixUniform(0),
  m_program(0),
  m_sink(0),
  m_pipeline(0),
  m_hasFrame(false),
  m_texture(0),
  m_dpy(EGL_NO_DISPLAY),
  m_glEGLImageTargetTexture2DOES(NULL),
  m_eglCreateSyncKHR(NULL) {

}

void Player::initialize() {
  m_program = new QOpenGLShaderProgram(this);
  m_program->addShaderFromSourceCode(QOpenGLShader::Vertex, vertexShaderSource);
  m_program->addShaderFromSourceCode(QOpenGLShader::Fragment, fragmentShaderSource);
  m_program->link();

  m_inputVertex = m_program->attributeLocation("inputVertex");
  m_textureCoord = m_program->attributeLocation("textureCoord");

  m_glEGLImageTargetTexture2DOES = (PFNGLEGLIMAGETARGETTEXTURE2DOESPROC)
    eglGetProcAddress ("glEGLImageTargetTexture2DOES");

  m_eglCreateSyncKHR = (PFNEGLCREATESYNCKHRPROC)
    eglGetProcAddress ("eglCreateSyncKHR");

  m_dpy = eglGetCurrentDisplay ();
  if (m_dpy == EGL_NO_DISPLAY) {
    abort ();
  }

  g_object_set(G_OBJECT(m_sink), "egl-display", m_dpy, NULL);

  g_signal_connect(G_OBJECT(m_sink), "frame-ready", G_CALLBACK(on_frame_ready), this);
}

void Player::render() {
  NemoGstVideoTexture *sink = NEMO_GST_VIDEO_TEXTURE (m_sink);
  glViewport(0, 0, width(), height());

  glClear(GL_COLOR_BUFFER_BIT);

  if (!m_hasFrame && m_texture) {
    glDeleteTextures (1, &m_texture);
    m_texture = 0;
    return;
  }

  if (!nemo_gst_video_texture_acquire_frame(sink)) {
    qDebug() << "Failed to acquire frame";
    return;
  }

  EGLImageKHR img;
  if (!nemo_gst_video_texture_bind_frame (sink, &img)) {
    qDebug() << "Failed to bind frame";

    nemo_gst_video_texture_release_frame (sink, NULL);
    return;
  }

  if (!m_texture) {
    glGenTextures(1, &m_texture);
    glBindTexture(GL_TEXTURE_EXTERNAL_OES, m_texture);
    glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  }
  else {
    glBindTexture(GL_TEXTURE_EXTERNAL_OES, m_texture);
  }

  glActiveTexture(GL_TEXTURE0);

  m_program->bind();

  m_glEGLImageTargetTexture2DOES (GL_TEXTURE_EXTERNAL_OES, (GLeglImageOES)img);

  QMatrix4x4 matrix;

  m_program->setUniformValue(m_program->attributeLocation("tex"), 0);
  m_program->setUniformValue(m_program->uniformLocation("matrix"), matrix);

  glVertexAttribPointer(m_inputVertex, 2, GL_FLOAT, GL_FALSE, 0, &vertexCoords);
  glVertexAttribPointer(m_textureCoord, 2, GL_FLOAT, GL_FALSE, 0, &texCoords);

  glEnableVertexAttribArray(m_inputVertex);
  glEnableVertexAttribArray(m_textureCoord);

  glDrawArrays(GL_TRIANGLE_FAN, 0, 4);

  glDisableVertexAttribArray(m_textureCoord);
  glDisableVertexAttribArray(m_inputVertex);

  m_program->release();

  glBindTexture(GL_TEXTURE_EXTERNAL_OES, 0);

  nemo_gst_video_texture_unbind_frame (sink);

  EGLSyncKHR sync = m_eglCreateSyncKHR (m_dpy, EGL_SYNC_FENCE_KHR, NULL);
  nemo_gst_video_texture_release_frame(sink, sync);

  glDeleteTextures (1, &m_texture);
  m_texture = 0;
}

void Player::setSink(GstElement *sink) {
  m_sink = sink;
}

void Player::setPipeline(GstElement *pipeline) {
  m_pipeline = pipeline;
}

GLuint Player::loadShader(GLenum type, const char *source) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, 0);
    glCompileShader(shader);
    return shader;
}

bool Player::start() {
  if (!m_sink || !m_pipeline) {
    return false;
  }

  if (gst_element_set_state (m_pipeline, GST_STATE_PLAYING) == GST_STATE_CHANGE_FAILURE) {
    return false;
  }

  return true;
}

void Player::on_frame_ready(GstElement *sink, gint frame, gpointer data) {
  Q_UNUSED (sink);
  Q_UNUSED (frame);

  Player *player = (Player *) data;

  if (frame < 0) {
    player->m_hasFrame = false;
  }
  else {
    player->m_hasFrame = true;
    QMetaObject::invokeMethod(player, "renderLater", Qt::QueuedConnection);
  }
}
