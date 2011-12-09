AM_CFLAGS = \
	$(WARN_CFLAGS) \
	-I$(top_srcdir) \
	-I$(top_srcdir)/nouveau \
	$(PTHREADSTUBS_CFLAGS) \
	-I$(top_srcdir)/include/drm \
	-DDEBUG

libdrm_nouveau_la_LTLIBRARIES = libdrm_nouveau.la
libdrm_nouveau_ladir = $(libdir)
libdrm_nouveau_la_LDFLAGS = -version-number 2:0:0 -no-undefined
libdrm_nouveau_la_LIBADD = ../libdrm.la @PTHREADSTUBS_LIBS@

libdrm_nouveau_la_SOURCES = nouveau.c \
			    pushbuf.c \
			    bufctx.c \
			    abi16.c \
			    private.h


libdrm_nouveauincludedir = ${includedir}/libdrm
libdrm_nouveauinclude_HEADERS = nouveau.h

pkgconfigdir = @pkgconfigdir@
pkgconfig_DATA = libdrm_nouveau.pc
