AM_CFLAGS = \
	$(WARN_CFLAGS) \
	-I$(top_srcdir) \
	-I$(top_srcdir)/nouveau \
	$(PTHREADSTUBS_CFLAGS) \
	-I$(top_srcdir)/include/drm

libdrm_nouveau_la_LTLIBRARIES = libdrm_nouveau.la
libdrm_nouveau_ladir = $(libdir)
libdrm_nouveau_la_LDFLAGS = -version-number 1:0:0 -no-undefined
libdrm_nouveau_la_LIBADD = ../libdrm.la @PTHREADSTUBS_LIBS@

libdrm_nouveau_la_SOURCES = \
			    nouveau_device.c \
			    nouveau_channel.c \
			    nouveau_pushbuf.c \
			    nouveau_grobj.c \
			    nouveau_notifier.c \
			    nouveau_bo.c \
			    nouveau_resource.c \
			    nouveau_private.h \
			    nouveau_reloc.c

libdrm_nouveaucommonincludedir = ${includedir}/nouveau
libdrm_nouveaucommoninclude_HEADERS = \
				nouveau_device.h \
				nouveau_channel.h \
				nouveau_grobj.h \
				nouveau_notifier.h \
				nouveau_pushbuf.h \
				nouveau_bo.h \
				nouveau_resource.h \
				nouveau_reloc.h


libdrm_nouveauincludedir = ${includedir}/libdrm
libdrm_nouveauinclude_HEADERS = \
				nouveau_drmif.h

pkgconfigdir = @pkgconfigdir@
pkgconfig_DATA = libdrm_nouveau.pc
