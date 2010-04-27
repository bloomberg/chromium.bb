AM_CFLAGS = \
	-I$(top_srcdir)/include/drm \
	-I$(top_srcdir)/intel/ \
	-I$(top_srcdir)

noinst_PROGRAMS = \
	vbltest

vbltest_SOURCES = \
	vbltest.c
vbltest_LDADD = \
	$(top_builddir)/libdrm.la \
	$(top_builddir)/intel/libdrm_intel.la
