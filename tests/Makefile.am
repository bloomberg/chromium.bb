TESTS = surface-test.la client-test.la	\
	event-test.la text-test.la	\
	surface-global-test.la		\
	button-test.la

TESTS_ENVIRONMENT = $(SHELL) $(top_srcdir)/tests/weston-test

export abs_builddir

AM_CFLAGS = $(GCC_CFLAGS)
AM_CPPFLAGS = -I$(top_srcdir)/src -DUNIT_TEST $(COMPOSITOR_CFLAGS)


check_LTLIBRARIES = $(TESTS)
check_PROGRAMS = test-client test-text-client

AM_LDFLAGS = -module -avoid-version -rpath $(libdir)

test_runner_src = test-runner.c test-runner.h

surface_global_test_la_SOURCES = surface-global-test.c $(test_runner_src)
surface_test_la_SOURCES = surface-test.c $(test_runner_src)
client_test_la_SOURCES = client-test.c $(test_runner_src)
event_test_la_SOURCES = event-test.c $(test_runner_src)
text_test_la_SOURCES = text-test.c $(test_runner_src)
button_test_la_SOURCES = button-test.c $(test_runner_src)

test_client_SOURCES = test-client.c
test_client_LDADD = $(SIMPLE_CLIENT_LIBS)

test_text_client_SOURCES = test-text-client.c  ../clients/text-protocol.c
test_text_client_LDADD = $(SIMPLE_CLIENT_LIBS)

noinst_PROGRAMS = $(setbacklight) matrix-test

matrix_test_SOURCES =				\
	matrix-test.c				\
	$(top_srcdir)/src/matrix.c		\
	$(top_srcdir)/src/matrix.h
matrix_test_LDADD = -lm -lrt

setbacklight_SOURCES =				\
	setbacklight.c				\
	$(top_srcdir)/src/libbacklight.c	\
	$(top_srcdir)/src/libbacklight.h

setbacklight_CFLAGS = $(AM_CFLAGS) $(SETBACKLIGHT_CFLAGS)
setbacklight_LDADD = $(SETBACKLIGHT_LIBS)

if BUILD_SETBACKLIGHT
setbacklight = setbacklight
endif

EXTRA_DIST = weston-test
