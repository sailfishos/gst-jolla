plugin_LTLIBRARIES = libgstdroidegl.la

libgstdroidegl_la_SOURCES = plugin.c \
			    gstdroideglsink.c \
			    gstdroideglsink.h \
			    gstdroideglbuffer.c \
			    gstdroideglbuffer.h

libgstdroidegl_la_CFLAGS = $(GST_CFLAGS) \
			   $(GST_VIDEO_CFLAGS) \
			   $(MGI_CFLAGS) \
		           $(GLES2_CFLAGS) \
			   $(EGL_CFLAGS) \
			   -I/usr/include/android/ \
			   -I$(top_builddir)/gst-libs \
                           -I$(top_builddir)/gst-libs/gst/gralloc/ \
                           -I$(top_builddir)/gst/nativebuffer/

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

noinst_HEADERS = gstdroideglsink.h gstdroideglbuffer.h
