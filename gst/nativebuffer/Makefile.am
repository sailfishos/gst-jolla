lib_LTLIBRARIES = libgstnativebuffer.la

libgstnativebuffer_la_SOURCES = gstnativebuffer.c

libgstnativebuffer_la_CFLAGS = $(GST_CFLAGS) \
			       -I$(top_builddir)/gst-libs/gst/gralloc/ \
			       -I/usr/include/android/

libgstnativebuffer_la_LIBADD = $(GST_LIBS) \
			       $(top_builddir)/gst-libs/gst/gralloc/libgstgralloc.la

libgstnativebuffer_la_LIBTOOLFLAGS = --tag=disable-static

libgstnativebuffer_ladir = $(includedir)/gstreamer-0.10/gst/
libgstnativebuffer_la_HEADERS = gstnativebuffer.h
