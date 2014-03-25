plugin_LTLIBRARIES = libgstdroideglsink.la

libgstdroideglsink_la_SOURCES = plugin.c \
			        gstdroideglsink.c \
			        gstdroideglsink.h

libgstdroideglsink_la_CFLAGS = $(DROID_CFLAGS) \
			       -I$(top_builddir)/gst-libs \
                               $(GST_CFLAGS) \
			       $(GST_VIDEO_CFLAGS) \
			       $(NGI_CFLAGS) \
		               $(GLES2_CFLAGS) \
                               $(EGL_CFLAGS) \
                               $(NEMO_GST_INTERFACES_CFLAGS)

libgstdroideglsink_la_LIBADD = $(GST_LIBS) \
                               $(GST_VIDEO_LIBS) \
                               $(GST_VIDEO_LIBS) \
                               $(GLES2_LIBS) \
                               $(EGL_LIBS) \
			       $(NGI_LIBS) \
                               $(NEMO_GST_INTERFACES_LIBS) \
                               $(top_builddir)/gst-libs/gst/anativewindowbuffer/libgstanativewindowbuffer-@GST_API_VERSION@.la

libgstdroideglsink_la_LDFLAGS = $(GST_PLUGIN_LDFLAGS)
libgstdroideglsink_la_LIBTOOLFLAGS = --tag=disable-static

noinst_HEADERS = gstdroideglsink.h
