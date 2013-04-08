# Note: plugindir is set in configure

plugin_LTLIBRARIES = libgstcodecbin.la

# sources used to compile this plug-in
libgstcodecbin_la_SOURCES = plugin.c \
			    gstcodecbin.c \
			    gstmpeg4vdec.c \
			    gstmpeg2vdec.c \
			    gstvc1vdec.c \
			    gsth263dec.c \
			    gsth264dec.c \
			    gstcodecbin.h \
			    gstmpeg4vdec.h \
			    gstmpeg2vdec.h \
			    gstvc1vdec.h \
			    gsth263dec.h \
			    gsth264dec.h
# compiler and linker flags used to compile this plugin, set in configure.ac
libgstcodecbin_la_CFLAGS = $(GST_CFLAGS)
libgstcodecbin_la_LIBADD = $(GST_LIBS)
libgstcodecbin_la_LDFLAGS = $(GST_PLUGIN_LDFLAGS)
libgstcodecbin_la_LIBTOOLFLAGS = --tag=disable-static

# headers we need but don't want installed
noinst_HEADERS = gstcodecbin.h gstmpeg4vdec.h
