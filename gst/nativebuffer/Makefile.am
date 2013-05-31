# Note: plugindir is set in configure

lib_LTLIBRARIES = libgstnativebuffer.la

libgstnativebuffer_la_SOURCES = gstnativebuffer.c

# compiler and linker flags used to compile this plugin, set in configure.ac
libgstnativebuffer_la_CFLAGS = $(GST_CFLAGS)
libgstnativebuffer_la_LIBADD = $(GST_LIBS)
libgstnativebuffer_la_LDFLAGS = $(GST_PLUGIN_LDFLAGS)
libgstnativebuffer_la_LIBTOOLFLAGS = --tag=disable-static

libgstnativebuffer_ladir = $(includedir)/gstreamer-@GST_MAJORMINOR@/gst/
libgstnativebuffer_la_HEADERS = gstnativebuffer.h
