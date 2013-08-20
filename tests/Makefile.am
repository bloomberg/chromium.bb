TESTS = $(shared_tests) $(module_tests) $(weston_tests)

shared_tests = \
	config-parser.test

module_tests =				\
	surface-test.la			\
	surface-global-test.la

weston_tests =				\
	keyboard.weston			\
	event.weston			\
	button.weston			\
	text.weston			\
	subsurface.weston		\
	$(xwayland_test)

AM_TESTS_ENVIRONMENT = \
	abs_builddir='$(abs_builddir)'; export abs_builddir;

TEST_EXTENSIONS = .la .weston
LA_LOG_COMPILER = $(srcdir)/weston-tests-env
WESTON_LOG_COMPILER = $(srcdir)/weston-tests-env

clean-local:
	-rm -rf logs

# To remove when automake 1.11 support is dropped
export abs_builddir

noinst_LTLIBRARIES =			\
	$(weston_test)

noinst_PROGRAMS =			\
	$(setbacklight)			\
	matrix-test

check_LTLIBRARIES =			\
	$(module_tests)

check_PROGRAMS =			\
	$(shared_tests)			\
	$(weston_tests)

AM_CFLAGS = $(GCC_CFLAGS)
AM_CPPFLAGS =					\
	-I$(top_srcdir)/src			\
	-I$(top_srcdir)/shared			\
	-I$(top_builddir)/src			\
	-DUNIT_TEST				\
	$(COMPOSITOR_CFLAGS)
AM_LDFLAGS = -module -avoid-version -rpath $(libdir)

config_parser_test_LDADD =	\
	../shared/libshared.la	\
	$(COMPOSITOR_LIBS)
config_parser_test_SOURCES =	\
	config-parser-test.c

surface_global_test_la_SOURCES = surface-global-test.c
surface_test_la_SOURCES = surface-test.c

weston_test = weston-test.la
weston_test_la_LIBADD = $(COMPOSITOR_LIBS)	\
	../shared/libshared.la
weston_test_la_CFLAGS = $(GCC_CFLAGS) $(COMPOSITOR_CFLAGS)
weston_test_la_SOURCES =		\
	weston-test.c			\
	wayland-test-protocol.c		\
	wayland-test-server-protocol.h

weston_test_runner_src =		\
	weston-test-runner.c		\
	weston-test-runner.h
weston_test_client_src =		\
	weston-test-client-helper.c	\
	weston-test-client-helper.h	\
	wayland-test-protocol.c		\
	wayland-test-client-protocol.h	\
	subsurface-protocol.c		\
	subsurface-client-protocol.h	\
	$(weston_test_runner_src)
weston_test_client_libs =		\
	$(SIMPLE_CLIENT_LIBS)		\
	../shared/libshared.la

keyboard_weston_SOURCES = keyboard-test.c $(weston_test_client_src)
keyboard_weston_LDADD = $(weston_test_client_libs)

event_weston_SOURCES = event-test.c $(weston_test_client_src)
event_weston_LDADD = $(weston_test_client_libs)

button_weston_SOURCES = button-test.c $(weston_test_client_src)
button_weston_LDADD = $(weston_test_client_libs)

text_weston_SOURCES =				\
	text-test.c				\
	../clients/text-protocol.c		\
	$(weston_test_client_src)
text_weston_LDADD = $(weston_test_client_libs)

subsurface_weston_SOURCES = subsurface-test.c $(weston_test_client_src)
subsurface_weston_LDADD = $(weston_test_client_libs)

xwayland_weston_SOURCES = xwayland-test.c	$(weston_test_client_src)

xwayland_weston_LDADD = $(weston_test_client_libs) $(XWAYLAND_TEST_LIBS)

if ENABLE_XWAYLAND_TEST
xwayland_test = xwayland.weston
endif

matrix_test_SOURCES =				\
	matrix-test.c				\
	$(top_srcdir)/shared/matrix.c		\
	$(top_srcdir)/shared/matrix.h
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

EXTRA_DIST = weston-tests-env

BUILT_SOURCES =					\
	subsurface-protocol.c			\
	subsurface-client-protocol.h		\
	wayland-test-protocol.c			\
	wayland-test-server-protocol.h		\
	wayland-test-client-protocol.h

CLEANFILES = $(BUILT_SOURCES)

wayland_protocoldir = $(top_srcdir)/protocol
include $(top_srcdir)/wayland-scanner.mk
