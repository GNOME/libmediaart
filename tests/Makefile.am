include $(top_srcdir)/Makefile.decl

noinst_PROGRAMS += $(test_programs)

test_programs = mediaarttest

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

mediaarttest_SOURCES = mediaarttest.c

EXTRA_DIST += \
	543249_King-Kilo---Radium.mp3 \
	cover.png
