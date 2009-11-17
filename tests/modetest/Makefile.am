AM_CFLAGS = \
	-I$(top_srcdir)/include/drm \
	-I$(top_srcdir)/intel/ \
	-I$(top_srcdir) \
	$(CAIRO_CFLAGS)

noinst_PROGRAMS = \
	modetest

modetest_SOURCES = \
	modetest.c
modetest_LDADD = \
	$(top_builddir)/libdrm.la \
	$(top_builddir)/intel/libdrm_intel.la \
	$(CAIRO_LIBS)
