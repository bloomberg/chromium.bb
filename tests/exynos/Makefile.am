AM_CFLAGS = \
	$(WARN_CFLAGS)\
	-I $(top_srcdir)/include/drm \
	-I $(top_srcdir)/libkms/ \
	-I $(top_srcdir)/exynos \
	-I $(top_srcdir)

if HAVE_LIBKMS
if HAVE_INSTALL_TESTS
bin_PROGRAMS = \
	exynos_fimg2d_test
else
noinst_PROGRAMS = \
	exynos_fimg2d_test
endif
endif

exynos_fimg2d_test_LDADD = \
	$(top_builddir)/libdrm.la \
	$(top_builddir)/libkms/libkms.la \
	$(top_builddir)/exynos/libdrm_exynos.la

exynos_fimg2d_test_SOURCES = \
	exynos_fimg2d_test.c

