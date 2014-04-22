AM_CFLAGS = $(filter-out -Wpointer-arith, $(WARN_CFLAGS))

AM_CFLAGS += \
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
	buffers.c buffers.h \
	cursor.c cursor.h \
	modetest.c

modetest_LDADD = \
	$(top_builddir)/libdrm.la \
	$(top_builddir)/libkms/libkms.la

if HAVE_CAIRO
AM_CFLAGS += $(CAIRO_CFLAGS)
modetest_LDADD += $(CAIRO_LIBS)
endif
