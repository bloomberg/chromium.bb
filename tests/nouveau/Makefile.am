AM_CFLAGS = \
	$(WARN_CFLAGS) \
	-I$(top_srcdir)/include/drm \
	-I$(top_srcdir)/nouveau \
	-I$(top_srcdir)

LDADD = \
	../../nouveau/libdrm_nouveau.la \
	../../libdrm.la \
	-ldl -lpthread

TESTS = threaded

check_PROGRAMS = $(TESTS)

