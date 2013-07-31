plugin_LTLIBRARIES = libgstdroideglsink.la

libgstdroideglsink_la_SOURCES = plugin.c \
			        gstdroideglsink.c \
			        gstdroideglsink.h

libgstdroideglsink_la_CFLAGS = $(DROID_CFLAGS) \
			       -I$(top_builddir)/gst-libs \
                               -I$(top_builddir)/gst-libs/gst/gralloc/ \
                               -I$(top_builddir)/gst/nativebuffer/ \
                               $(GST_CFLAGS) \
			       $(GST_VIDEO_CFLAGS) \
			       $(MGI_CFLAGS) \
		               $(GLES2_CFLAGS) \
			       $(EGL_CFLAGS) \
			       $(MGI_CFLAGS)

libgstdroideglsink_la_LIBADD = $(GST_LIBS) \
                               $(GST_VIDEO_LIBS) \
                               $(GST_VIDEO_LIBS) \
                               $(GLES2_LIBS) \
                               $(EGL_LIBS) \
			       $(MGI_LIBS) \
                               $(top_builddir)/gst-libs/gst/gralloc/libgstgralloc.la \
			       $(top_builddir)/gst/nativebuffer/libgstnativebuffer.la

libgstdroideglsink_la_LDFLAGS = $(GST_PLUGIN_LDFLAGS)
libgstdroideglsink_la_LIBTOOLFLAGS = --tag=disable-static

noinst_HEADERS = gstdroideglsink.h
