TESTS = $(shared_tests) $(module_tests) $(weston_tests)

shared_tests = \
	config-parser.test		\
	vertex-clip.test

module_tests =				\
	surface-test.la			\
	surface-global-test.la

weston_tests =				\
	bad_buffer.weston		\
	keyboard.weston			\
	event.weston			\
	button.weston			\
	text.weston			\
	subsurface.weston		\
	$(xwayland_test)

if ENABLE_EGL
weston_tests +=				\
	buffer-count.weston
endif

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
	weston-test.la			\
	$(module_tests)			\
	libtest-runner.la		\
	libtest-client.la

noinst_PROGRAMS =			\
	$(setbacklight)			\
	$(shared_tests)			\
	$(weston_tests)			\
	matrix-test

AM_CFLAGS = $(GCC_CFLAGS)
AM_CPPFLAGS =					\
	-I$(top_srcdir)/src			\
	-I$(top_srcdir)/shared			\
	-I$(top_builddir)/src			\
	-DUNIT_TEST				\
	$(COMPOSITOR_CFLAGS)

surface_global_test_la_SOURCES = surface-global-test.c
surface_global_test_la_LDFLAGS = -module -avoid-version -rpath $(libdir)
surface_test_la_SOURCES = surface-test.c
surface_test_la_LDFLAGS = -module -avoid-version -rpath $(libdir)

weston_test_la_LIBADD = $(COMPOSITOR_LIBS) ../shared/libshared.la
weston_test_la_LDFLAGS = -module -avoid-version -rpath $(libdir)
weston_test_la_CFLAGS = $(GCC_CFLAGS) $(COMPOSITOR_CFLAGS)
weston_test_la_SOURCES =		\
	weston-test.c			\
	wayland-test-protocol.c		\
	wayland-test-server-protocol.h

if ENABLE_EGL
weston_test_la_CFLAGS += $(EGL_TESTS_CFLAGS)
weston_test_la_LDFLAGS += $(EGL_TESTS_LIBS)
endif

libtest_runner_la_SOURCES =	\
	weston-test-runner.c	\
	weston-test-runner.h
libtest_runner_la_CFLAGS = $(GCC_CFLAGS) $(COMPOSITOR_CFLAGS)

config_parser_test_LDADD =	\
	../shared/libshared.la	\
	libtest-runner.la	\
	$(COMPOSITOR_LIBS)
config_parser_test_SOURCES =	\
	config-parser-test.c

vertex_clip_test_SOURCES =		\
	vertex-clip-test.c		\
	../src/vertex-clipping.c	\
	../src/vertex-clipping.h
vertex_clip_test_LDADD =	\
	libtest-runner.la	\
	-lm -lrt

libtest_client_la_SOURCES =		\
	weston-test-client-helper.c	\
	weston-test-client-helper.h	\
	wayland-test-protocol.c		\
	wayland-test-client-protocol.h
libtest_client_la_LIBADD =		\
	$(TEST_CLIENT_LIBS)		\
	../shared/libshared.la		\
	libtest-runner.la

bad_buffer_weston_SOURCES = bad-buffer-test.c
bad_buffer_weston_LDADD = libtest-client.la

keyboard_weston_SOURCES = keyboard-test.c
keyboard_weston_LDADD = libtest-client.la

event_weston_SOURCES = event-test.c
event_weston_LDADD = libtest-client.la

button_weston_SOURCES = button-test.c
button_weston_LDADD = libtest-client.la

text_weston_SOURCES = text-test.c text-protocol.c
text_weston_LDADD = libtest-client.la

subsurface_weston_SOURCES = subsurface-test.c
subsurface_weston_LDADD = libtest-client.la

buffer_count_weston_SOURCES = buffer-count-test.c
buffer_count_weston_CFLAGS = $(GCC_CFLAGS) $(EGL_TESTS_CFLAGS)
buffer_count_weston_LDADD = libtest-client.la $(EGL_TESTS_LIBS)

xwayland_weston_SOURCES = xwayland-test.c
xwayland_weston_CFLAGS = $(GCC_CFLAGS) $(XWAYLAND_TEST_CFLAGS)
xwayland_weston_LDADD = libtest-client.la $(XWAYLAND_TEST_LIBS)

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
	wayland-test-protocol.c			\
	wayland-test-server-protocol.h		\
	wayland-test-client-protocol.h		\
	text-protocol.c				\
	text-client-protocol.h

CLEANFILES = $(BUILT_SOURCES)

wayland_protocoldir = $(top_srcdir)/protocol
include $(top_srcdir)/wayland-scanner.mk
