TESTS = surface-test.la client-test.la event-test.la

TESTS_ENVIRONMENT = $(SHELL) $(top_srcdir)/tests/weston-test

export abs_builddir

AM_CFLAGS = $(GCC_CFLAGS)
AM_CPPFLAGS = -I$(top_srcdir)/src -DUNIT_TEST $(COMPOSITOR_CFLAGS)


check_LTLIBRARIES = $(TESTS)
check_PROGRAMS = test-client

AM_LDFLAGS = -module -avoid-version -rpath $(libdir)

test_runner_src = test-runner.c test-runner.h

surface_test_la_SOURCES = surface-test.c $(test_runner_src)
client_test_la_SOURCES = client-test.c $(test_runner_src)
event_test_la_SOURCES = event-test.c $(test_runner_src)

test_client_SOURCES = test-client.c
test_client_LDADD = $(SIMPLE_CLIENT_LIBS)

noinst_PROGRAMS = setbacklight matrix-test

matrix_test_SOURCES =				\
	matrix-test.c				\
	$(top_srcdir)/src/matrix.c		\
	$(top_srcdir)/src/matrix.h
matrix_test_LDADD = -lm -lrt

setbacklight_SOURCES =				\
	setbacklight.c				\
	$(top_srcdir)/src/libbacklight.c	\
	$(top_srcdir)/src/libbacklight.h

setbacklight_LDADD = -ludev -ldrm

EXTRA_DIST = weston-test
