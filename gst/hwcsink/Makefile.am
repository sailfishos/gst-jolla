plugin_LTLIBRARIES = libgsthwcsink.la

libgsthwcsink_la_SOURCES = plugin.c \
			gsthwcsink.c \
			gsthwcsink.h

libgsthwcsink_la_CFLAGS = $(GST_CFLAGS) \
                          $(GST_VIDEO_CFLAGS) \
                          -I/usr/include/android/ \
                          -I$(top_builddir)/gst-libs/gst/gralloc/ \
                          -I$(top_builddir)/gst/nativebuffer/

libgsthwcsink_la_LIBADD = $(GST_LIBS) \
                          $(GST_VIDEO_LIBS) \
			  $(top_builddir)/gst-libs/gst/gralloc/libgstgralloc.la \
			  $(top_builddir)/gst/nativebuffer/libgstnativebuffer.la

libgsthwcsink_la_LDFLAGS = $(GST_PLUGIN_LDFLAGS)
libgsthwcsink_la_LIBTOOLFLAGS = --tag=disable-static

noinst_HEADERS = gsthwcsink.h
