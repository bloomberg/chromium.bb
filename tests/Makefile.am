NULL:=#

AM_CPPFLAGS = \
	-I $(top_srcdir)/include/drm \
	-I $(top_srcdir)

LDADD = $(top_builddir)/libdrm.la

check_PROGRAMS = \
	dristat \
	drmstat

SUBDIRS = modeprint proptest

if HAVE_LIBKMS
SUBDIRS += kmstest modetest
endif

if HAVE_RADEON
SUBDIRS += radeon
endif

if HAVE_EXYNOS
SUBDIRS += exynos
endif

if HAVE_TEGRA
SUBDIRS += tegra
endif

if HAVE_LIBUDEV

check_LTLIBRARIES = libdrmtest.la

libdrmtest_la_SOURCES = \
	drmtest.c \
	drmtest.h

libdrmtest_la_LIBADD = \
	$(top_builddir)/libdrm.la \
	$(LIBUDEV_LIBS)

LDADD += libdrmtest.la

XFAIL_TESTS =					\
	auth					\
	lock

TESTS =						\
	openclose				\
	getversion				\
	getclient				\
	getstats				\
	setversion				\
	updatedraw				\
	name_from_fd				\
	$(NULL)

SUBDIRS += vbltest $(NULL)

if HAVE_INTEL
TESTS +=					\
	gem_basic				\
	gem_flink				\
	gem_readwrite				\
	gem_mmap				\
	$(NULL)
endif

check_PROGRAMS += $(TESTS)

endif
