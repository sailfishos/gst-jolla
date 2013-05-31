plugin_LTLIBRARIES = libgsthwsink.la

libgsthwsink_la_SOURCES = plugin.c \
			gsthwcsink.c \
			gsthwcsink.h

libgsthwsink_la_CFLAGS = $(GST_CFLAGS) $(GST_VIDEO_CFLAGS) -I../gralloc -I/usr/include/android/
libgsthwsink_la_LIBADD = $(GST_LIBS) $(GST_VIDEO_LIBS) -L../gralloc -lgstgralloc
libgsthwsink_la_LDFLAGS = $(GST_PLUGIN_LDFLAGS)
libgsthwsink_la_LIBTOOLFLAGS = --tag=disable-static

noinst_HEADERS = gsthwcsink.h
