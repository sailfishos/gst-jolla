plugin_LTLIBRARIES = libgstdroidegl.la

libgstdroidegl_la_SOURCES = plugin.c \
			    gstdroideglsink.c \
			    gstdroideglsink.h

libgstdroidegl_la_CFLAGS = $(DROID_CFLAGS) \
			   -I$(top_builddir)/gst-libs \
                           -I$(top_builddir)/gst-libs/gst/gralloc/ \
                           -I$(top_builddir)/gst/nativebuffer/ \
                           $(GST_CFLAGS) \
			   $(GST_VIDEO_CFLAGS) \
			   $(MGI_CFLAGS) \
		           $(GLES2_CFLAGS) \
			   $(EGL_CFLAGS)

libgstdroidegl_la_LIBADD = $(GST_LIBS) \
                           $(GST_VIDEO_LIBS) \
                           $(GST_VIDEO_LIBS) \
		           $(GLES2_LIBS) \
			   $(EGL_LIBS) \
			   $(top_builddir)/gst-libs/gst/interfaces/libgstnemointerfaces-$(GST_MAJORMINOR).la \
                           $(top_builddir)/gst-libs/gst/gralloc/libgstgralloc.la \
			   $(top_builddir)/gst/nativebuffer/libgstnativebuffer.la

libgstdroidegl_la_LDFLAGS = $(GST_PLUGIN_LDFLAGS)
libgstdroidegl_la_LIBTOOLFLAGS = --tag=disable-static

noinst_HEADERS = gstdroideglsink.h
