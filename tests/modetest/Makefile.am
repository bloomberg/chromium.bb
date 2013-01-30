AM_CFLAGS = \
	-I$(top_srcdir)/include/drm \
	-I$(top_srcdir)/libkms/ \
	-I$(top_srcdir)

if HAVE_INSTALL_TESTS
bin_PROGRAMS = \
	modetest
else
noinst_PROGRAMS = \
	modetest
endif

modetest_SOURCES = \
	buffers.c modetest.c buffers.h

modetest_LDADD = \
	$(top_builddir)/libdrm.la \
	$(top_builddir)/libkms/libkms.la

if HAVE_CAIRO
AM_CFLAGS += $(CAIRO_CFLAGS)
modetest_LDADD += $(CAIRO_LIBS)
endif
