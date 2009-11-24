NULL:=#

AM_CPPFLAGS = \
	-I $(top_srcdir)/include/drm \
	-I $(top_srcdir)

LDADD = $(top_builddir)/libdrm.la

noinst_PROGRAMS = \
	dristat \
	drmstat

SUBDIRS =

if HAVE_INTEL
endif

if HAVE_LIBUDEV

EXTRA_LTLIBRARIES = libdrmtest.la

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

if HAVE_INTEL
SUBDIRS += \
	modeprint				\
	modetest				\
	$(NULL)

TESTS +=					\
	gem_basic				\
	gem_flink				\
	gem_readwrite				\
	gem_mmap				\
	$(NULL)
endif

EXTRA_PROGRAMS = $(TESTS)

endif

CLEANFILES = $(EXTRA_PROGRAMS) $(EXTRA_LTLIBRARIES)
