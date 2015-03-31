include Makefile.sources

AM_CFLAGS = \
	$(WARN_CFLAGS) \
	-I$(top_srcdir) \
	$(PTHREADSTUBS_CFLAGS) \
	-I$(top_srcdir)/include/drm \
	-DDEBUG

libdrm_nouveau_la_LTLIBRARIES = libdrm_nouveau.la
libdrm_nouveau_ladir = $(libdir)
libdrm_nouveau_la_LDFLAGS = -version-number 2:0:0 -no-undefined
libdrm_nouveau_la_LIBADD = ../libdrm.la @PTHREADSTUBS_LIBS@

libdrm_nouveau_la_SOURCES = $(LIBDRM_NOUVEAU_FILES)

libdrm_nouveauincludedir = ${includedir}/libdrm
libdrm_nouveauinclude_HEADERS = $(LIBDRM_NOUVEAU_H_FILES)

pkgconfigdir = @pkgconfigdir@
pkgconfig_DATA = libdrm_nouveau.pc

TESTS = nouveau-symbol-check
EXTRA_DIST = Android.mk $(TESTS)
