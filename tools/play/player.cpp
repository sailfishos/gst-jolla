#include <QtGui/QOpenGLShaderProgram>
#include <QtGui/QGuiApplication>
#include "player.h"
#include <gst/interfaces/meegovideotexture.h>

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
  m_dpy(EGL_NO_DISPLAY),
  m_context(EGL_NO_CONTEXT) {

}

void Player::initialize() {
  m_program = new QOpenGLShaderProgram(this);
  m_program->addShaderFromSourceCode(QOpenGLShader::Vertex, vertexShaderSource);
  m_program->addShaderFromSourceCode(QOpenGLShader::Fragment, fragmentShaderSource);
  m_program->link();

  m_inputVertex = m_program->attributeLocation("inputVertex");
  m_textureCoord = m_program->attributeLocation("textureCoord");

  m_dpy = eglGetCurrentDisplay ();
  m_context = eglGetCurrentContext ();

  g_object_set(G_OBJECT(m_sink), "egl-display", m_dpy, "egl-context", m_context,
	       NULL);

  g_signal_connect(G_OBJECT(m_sink), "frame-ready", G_CALLBACK(on_frame_ready), this);
}

void Player::render() {
  MeegoGstVideoTexture *sink = MEEGO_GST_VIDEO_TEXTURE (m_sink);
  glViewport(0, 0, width(), height());

  glClear(GL_COLOR_BUFFER_BIT);

  if (!m_hasFrame) {
    return;
  }

  if (!meego_gst_video_texture_acquire_frame(sink, 0)) {
    qDebug() << "Failed to acquire frame";
    return;
  }

  glEnable(GL_TEXTURE_2D);

  glActiveTexture(GL_TEXTURE0);

  m_program->bind();

  if (!meego_gst_video_texture_bind_frame(sink, GL_TEXTURE_EXTERNAL_OES, 0)) {
    qDebug() << "Failed to bind frame";
    m_program->release();
    return;
  }

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

  if (!meego_gst_video_texture_bind_frame(sink, GL_TEXTURE_EXTERNAL_OES, -1)) {
    qDebug() << "Failed to unbind frame";
  }

  m_program->release();

  glDisable(GL_TEXTURE_2D);

  meego_gst_video_texture_release_frame(sink, 0, NULL);
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

  GstBus *bus = gst_pipeline_get_bus (GST_PIPELINE (m_pipeline));
  gst_bus_add_watch (bus, bus_callback, NULL);
  gst_object_unref (bus);

  if (gst_element_set_state (m_pipeline, GST_STATE_PLAYING) == GST_STATE_CHANGE_FAILURE) {
    return false;
  }

  return true;
}

void Player::on_frame_ready(GstElement *sink, gint frame, gpointer data) {
  Q_UNUSED (sink);
  Q_UNUSED (frame);

  Player *player = (Player *) data;

  player->m_hasFrame = true;
  QMetaObject::invokeMethod(player, "renderLater", Qt::QueuedConnection);
}

gboolean Player::bus_callback(GstBus * bus, GstMessage * msg, gpointer data) {
  Q_UNUSED (bus);
  Q_UNUSED (data);

  switch (GST_MESSAGE_TYPE (msg)) {
    case GST_MESSAGE_EOS:
      g_printerr ("End of stream\n");
      QGuiApplication::quit();
      break;

    case GST_MESSAGE_ERROR:{
      gchar *debug;
      GError *error;

      gst_message_parse_error (msg, &error, &debug);
      g_printerr ("Error: %s (%s)\n", error->message, debug);
      g_error_free (error);

      if (debug) {
        g_free (debug);
      }

      QGuiApplication::quit();
      break;
    }

    case GST_MESSAGE_WARNING:{
      gchar *debug;
      GError *error;

      gst_message_parse_warning (msg, &error, &debug);

      g_printerr ("Warning: %s (%s)\n", error->message, debug);
      g_error_free (error);

      if (debug) {
        g_free (debug);
      }

      break;
    }

    default:
      break;
  }

  return TRUE;
}
