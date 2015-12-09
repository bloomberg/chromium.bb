AM_CPPFLAGS = \
	-I$(top_srcdir)/include/drm \
	-I$(top_srcdir)/tests \
	-I$(top_srcdir)

AM_CFLAGS = \
	$(WARN_CFLAGS)

noinst_LTLIBRARIES = libkms-test.la

libkms_test_la_SOURCES = \
	libkms-test.h \
	libkms-test-crtc.c \
	libkms-test-device.c \
	libkms-test-framebuffer.c \
	libkms-test-plane.c \
	libkms-test-screen.c

libkms_test_la_LIBADD = \
	$(top_builddir)/libdrm.la
