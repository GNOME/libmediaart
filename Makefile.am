include $(top_srcdir)/Makefile.decl

SUBDIRS = libmediaart docs

if HAVE_UNIT_TESTS
SUBDIRS += tests
endif

pkgconfig_DATA = libmediaart-@LIBMEDIAART_API_VERSION@.pc
pkgconfigdir = $(libdir)/pkgconfig

EXTRA_DIST =                                           \
	libmediaart.doap                               \
	libmediaart-@LIBMEDIAART_API_VERSION@.pc.in

DISTCHECK_CONFIGURE_FLAGS =                            \
	--enable-unit-tests                            \
	--enable-introspection

