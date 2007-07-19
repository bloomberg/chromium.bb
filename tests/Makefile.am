AM_CFLAGS = \
	-I $(top_srcdir)/shared-core

noinst_PROGRAMS = \
	dristat \
	drmstat

EXTRA_LTLIBRARIES = libdrmtest.la
libdrmtest_la_SOURCES = \
	drmtest.c \
	drmtest.h
libdrmtest_la_LIBADD = \
	$(top_builddir)/libdrm/libdrm.la

LDADD = libdrmtest.la

TESTS = openclose \
	getversion \
	getclient

XFAIL_TESTS = getclient

EXTRA_PROGRAMS = $(TESTS)
CLEANFILES = $(EXTRA_PROGRAMS) $(EXTRA_LTLIBRARIES)