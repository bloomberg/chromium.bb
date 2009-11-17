AM_CFLAGS = \
	-I$(top_srcdir)/include/drm \
	-I$(top_srcdir)/intel/ \
	-I$(top_srcdir)

noinst_PROGRAMS = \
	modeprint

modeprint_SOURCES = \
	modeprint.c
modeprint_LDADD = \
	$(top_builddir)/libdrm.la \
	$(top_builddir)/intel/libdrm_intel.la
