include $(top_srcdir)/Makefile.decl

SUBDIRS = libmediaart docs

if HAVE_UNIT_TESTS
SUBDIRS += tests
endif

EXTRA_DIST =                                           \
	libmediaart.doap

DISTCHECK_CONFIGURE_FLAGS =                            \
	--enable-unit-tests                            \
	--enable-introspection

