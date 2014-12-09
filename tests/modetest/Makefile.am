include Makefile.sources

AM_CFLAGS = $(filter-out -Wpointer-arith, $(WARN_CFLAGS))

AM_CFLAGS += \
	-I$(top_srcdir)/include/drm \
	-I$(top_srcdir)

if HAVE_INSTALL_TESTS
bin_PROGRAMS = \
	modetest
else
noinst_PROGRAMS = \
	modetest
endif

modetest_SOURCES = $(MODETEST_FILES)

modetest_LDADD = \
	$(top_builddir)/libdrm.la \
	-lpthread

if HAVE_CAIRO
AM_CFLAGS += $(CAIRO_CFLAGS)
modetest_LDADD += $(CAIRO_LIBS)
endif

EXTRA_DIST = Android.mk
