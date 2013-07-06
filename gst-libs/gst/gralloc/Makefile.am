# Note: plugindir is set in configure

lib_LTLIBRARIES = libgstgralloc.la

libgstgralloc_la_SOURCES = gstgralloc.c

libgstgralloc_la_CFLAGS = $(GST_CFLAGS) \
                          $(DROID_CFLAGS)

libgstgralloc_la_LIBADD = $(GST_LIBS)

libgstgralloc_la_LIBTOOLFLAGS = --tag=disable-static

libgstgralloc_ladir = $(includedir)/gstreamer-0.10/gst/
libgstgralloc_la_HEADERS = gstgralloc.h
