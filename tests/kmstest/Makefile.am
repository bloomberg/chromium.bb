AM_CFLAGS = \
	-I$(top_srcdir)/include/drm \
	-I$(top_srcdir)/libkms/ \
	-I$(top_srcdir)

noinst_PROGRAMS = \
	kmstest

kmstest_SOURCES = \
	main.c

kmstest_LDADD = \
	$(top_builddir)/libdrm.la \
	$(top_builddir)/libkms/libkms.la

run: kmstest
	./kmstest
