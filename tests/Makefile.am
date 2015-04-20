SUBDIRS = modeprint proptest modetest vbltest

if HAVE_LIBKMS
SUBDIRS += kmstest
endif

if HAVE_RADEON
SUBDIRS += radeon
endif

if HAVE_AMDGPU
if HAVE_CUNIT
SUBDIRS += amdgpu
endif
endif

if HAVE_EXYNOS
SUBDIRS += exynos
endif

if HAVE_TEGRA
SUBDIRS += tegra
endif

AM_CFLAGS = \
	$(WARN_CFLAGS)\
	-I $(top_srcdir)/include/drm \
	-I $(top_srcdir)

LDADD = $(top_builddir)/libdrm.la

check_PROGRAMS = \
	dristat \
	drmstat

if HAVE_NOUVEAU
SUBDIRS += nouveau
endif

TESTS = \
	drmsl \
	hash \
	random

if HAVE_LIBUDEV

check_LTLIBRARIES = libdrmtest.la

libdrmtest_la_SOURCES = \
	drmtest.c \
	drmtest.h

LDADD += \
	libdrmtest.la \
	$(LIBUDEV_LIBS)


XFAIL_TESTS =					\
	auth					\
	lock

TESTS +=					\
	openclose				\
	getversion				\
	getclient				\
	getstats				\
	setversion				\
	updatedraw				\
	name_from_fd
endif

check_PROGRAMS += $(TESTS)
