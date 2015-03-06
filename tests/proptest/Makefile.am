AM_CFLAGS = \
	$(WARN_CFLAGS)\
	-I$(top_srcdir)/include/drm \
	-I$(top_srcdir)

if HAVE_INSTALL_TESTS
bin_PROGRAMS = \
	proptest
else
noinst_PROGRAMS = \
	proptest
endif

proptest_SOURCES = \
	proptest.c
proptest_LDADD = \
	$(top_builddir)/libdrm.la
