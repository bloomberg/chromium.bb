AM_CFLAGS = \
	-I$(top_srcdir)/include/drm \
	-I$(top_srcdir)/libkms/ \
	-I$(top_srcdir)

noinst_PROGRAMS = \
	modetest

modetest_SOURCES = \
	buffers.c modetest.c buffers.h

modetest_LDADD = \
	$(top_builddir)/libdrm.la \
	$(top_builddir)/libkms/libkms.la

if HAVE_CAIRO
AM_CFLAGS += $(CAIRO_CFLAGS)
modetest_LDADD += $(CAIRO_LIBS)
endif
