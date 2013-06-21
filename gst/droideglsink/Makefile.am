plugin_LTLIBRARIES = libgstdroidegl.la

libgstdroidegl_la_SOURCES = plugin.c \
			    gstdroideglsink.c \
			    gstdroideglsink.h

libgstdroidegl_la_CFLAGS = $(GST_CFLAGS) \
			   $(GST_VIDEO_CFLAGS) \
			   $(MGI_CFLAGS) \
		           $(GLES2_CFLAGS) \
			   $(EGL_CFLAGS) \
			   -I../gralloc \
			   -I/usr/include/android/ \
			   -I../nativebuffer

libgstdroidegl_la_LIBADD = $(GST_LIBS) \
                           $(GST_VIDEO_LIBS) \
			   $(MGI_LIBS) \
		           $(GLES2_LIBS) \
			   $(EGL_LIBS) \
                           -L../gralloc -lgstgralloc \
                           -L../nativebuffer -lgstnativebuffer

libgstdroidegl_la_LDFLAGS = $(GST_PLUGIN_LDFLAGS)
libgstdroidegl_la_LIBTOOLFLAGS = --tag=disable-static

noinst_HEADERS = gstdroideglsink.h
