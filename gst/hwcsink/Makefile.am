plugin_LTLIBRARIES = libgsthwcsink.la

libgsthwcsink_la_SOURCES = plugin.c \
			gsthwcsink.c \
			gsthwcsink.h

libgsthwcsink_la_CFLAGS = $(GST_CFLAGS) $(GST_VIDEO_CFLAGS) -I../gralloc -I/usr/include/android/ -I../nativebuffer
libgsthwcsink_la_LIBADD = $(GST_LIBS) $(GST_VIDEO_LIBS) -L../gralloc -lgstgralloc -L../nativebuffer -lgstnativebuffer
libgsthwcsink_la_LDFLAGS = $(GST_PLUGIN_LDFLAGS)
libgsthwcsink_la_LIBTOOLFLAGS = --tag=disable-static

noinst_HEADERS = gsthwcsink.h
