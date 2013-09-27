include $(top_srcdir)/Makefile.decl

noinst_PROGRAMS = $(TEST_PROGS)

TEST_PROGS += mediaarttest

AM_CPPFLAGS = \
	-DTOP_SRCDIR=\"$(abs_top_srcdir)\" \
	-DTOP_BUILDDIR=\"$(abs_top_builddir)\" \
	$(BUILD_CFLAGS) \
	-I$(top_srcdir) \
	$(LIBMEDIAART_CFLAGS)                    

LDADD = \
	$(top_builddir)/libmediaart/libmediaart-@LIBMEDIAART_API_VERSION@.la \
	$(BUILD_LIBS) \
	$(LIBMEDIAART_LIBS)

mediaarttest = mediaarttest.c
