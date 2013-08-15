include $(top_srcdir)/Makefile.decl

AM_CPPFLAGS = \
	$(BUILD_CFLAGS) \
	$(LIBMEDIAART_CFLAGS)

#
# Binaries
#
lib_LTLIBRARIES = libmediaart-@LIBMEDIAART_API_VERSION@.la

libmediaartdir = $(libdir)/libmediaart-$(LIBMEDIAART_API_VERSION)/
libmediaartincludedir=$(includedir)/libmediaart-$(LIBMEDIAART_API_VERSION)/

libmediaart_@LIBMEDIAART_API_VERSION@_la_SOURCES = \
	cache.c \
	cache.h \
	extract.c \
	extract.h \
	extractgeneric.h \
	$(backend)

libmediaartinclude_HEADERS = \
	mediaart.h \
	cache.h
	extract.h \
	extractgeneric.h

if HAVE_GDKPIXBUF
libmediaart_@LIBMEDIAART_API_VERSION@_la_SOURCES += extractpixbuf.c
else
if HAVE_QT
libmediaart_@LIBMEDIAART_API_VERSION@_la_SOURCES += extractqt.cpp
else
libmediaart_@LIBMEDIAART_API_VERSION@_la_SOURCES += extractdummy.c
endif
endif


libmediaart_@LIBMEDIAART_API_VERSION@_la_LDFLAGS = \
	-version-info $(LT_CURRENT):$(LT_REVISION):$(LT_AGE)

libmediaart_@LIBMEDIAART_API_VERSION@_la_LIBADD = \
	-lm \
	$(BUILD_LIBS) \
	$(LIBMEDIAART_LIBS)
