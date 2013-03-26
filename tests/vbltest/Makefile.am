AM_CFLAGS = \
	-I$(top_srcdir)/include/drm \
	-I$(top_srcdir)
if HAVE_INSTALL_TESTS
bin_PROGRAMS = \
	vbltest
else
noinst_PROGRAMS = \
	vbltest
endif

vbltest_SOURCES = \
	vbltest.c
vbltest_LDADD = \
	$(top_builddir)/libdrm.la
