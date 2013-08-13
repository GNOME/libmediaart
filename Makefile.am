include $(top_srcdir)/Makefile.decl

SUBDIRS = libmediaart

if HAVE_UNIT_TESTS
SUBDIRS += tests
endif

EXTRA_DIST =                                           \
	intltool-extract.in                            \
	intltool-merge.in                              \
	intltool-update.in			       \
	libmediaart.doap

DISTCLEANFILES =                                       \
	intltool-extract                               \
	intltool-merge                                 \
	intltool-update

DISTCHECK_CONFIGURE_FLAGS =                            \
	--enable-unit-tests

