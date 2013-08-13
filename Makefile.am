include $(top_srcdir)/Makefile.decl

SUBDIRS = libmediaart

if HAVE_UNIT_TESTS
SUBDIRS += tests
endif

EXTRA_DIST =                                           \
	libmediaart.doap

DISTCHECK_CONFIGURE_FLAGS =                            \
	--enable-unit-tests

