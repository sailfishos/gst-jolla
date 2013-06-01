# Note: plugindir is set in configure

lib_LTLIBRARIES = libgstgralloc.la

libgstgralloc_la_SOURCES = gstgralloc.c

# compiler and linker flags used to compile this plugin, set in configure.ac
libgstgralloc_la_CFLAGS = $(GST_CFLAGS) -I/usr/include/android/
libgstgralloc_la_LIBADD = $(GST_LIBS) -lhardware -lhybris-common
libgstgralloc_la_LDFLAGS = $(GST_PLUGIN_LDFLAGS)
libgstgralloc_la_LIBTOOLFLAGS = --tag=disable-static

libgstgralloc_ladir = $(includedir)/gstreamer-0.10/gst/
libgstgralloc_la_HEADERS = gstgralloc.h
