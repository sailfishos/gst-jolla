#include <QtGui/QGuiApplication>
#include "player.h"
#include <gst/gst.h>
#include <iostream>

GstElement *findSink(GstElement *pipeline) {
  for (int x = 0; x < GST_BIN_NUMCHILDREN (GST_BIN (pipeline)); x++) {
    gpointer data = g_list_nth_data (GST_BIN (pipeline)->children, x);
    if (GST_IS_ELEMENT (data)) {
      GstElement *elem = GST_ELEMENT (data);
      if (GST_PLUGIN_FEATURE_NAME (GST_PLUGIN_FEATURE (gst_element_get_factory (elem))) ==
	  QLatin1String ("droideglsink")) {
	return elem;
      }
    }
  }

  return NULL;
}

int main(int argc, char **argv)
{
    setenv ("QT_WAYLAND_DISABLE_WINDOWDECORATION", "1", 1);
    QGuiApplication app(argc, argv);

    gst_init (&argc, &argv);

    if (argc < 2) {
      std::cout << "Usage: " << argv[0] << " <pipeline description> or file to play" << std::endl;
      return 1;
    }

    Player player;

    if (argc == 2) {
      // Most likely it's a file.
      gchar *file = g_strdup_printf ("file://%s/%s", g_get_current_dir (), argv[1]);

      GstElement *pipe = gst_element_factory_make ("playbin2", NULL);
      GstElement *sink = gst_element_factory_make ("droideglsink", NULL);
      g_object_set (pipe, "video-sink", sink, "flags", 99, "uri", file, NULL);
      g_free (file);

      player.setPipeline(pipe);
      player.setSink(sink);
    }
    else {
      gchar **argvn = g_new0 (char *, argc);
      memcpy (argvn, argv + 1, sizeof (char *) * (argc - 1));
      GstElement *pipe = gst_parse_launchv ((const gchar **) argvn, NULL);
      g_free (argvn);

      if (!pipe) {
	std::cout << "Failed to construct pipeline" << std::endl;
	return 1;
      }

      GstElement *sink = findSink (pipe);
      if (!sink) {
	std::cout << "No sink found";
	return 1;
      }

      player.setPipeline(pipe);
      player.setSink(sink);
    }

    player.resize(768, 1280);
    player.show();

    if (!player.start()) {
      std::cout << "Failed to start" << std::endl;
      return 1;
    }

    return app.exec();
}
