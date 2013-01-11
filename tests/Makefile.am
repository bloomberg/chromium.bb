TESTS =						\
	array-test				\
	client-test				\
	display-test				\
	connection-test				\
	event-loop-test				\
	fixed-test				\
	list-test				\
	map-test				\
	os-wrappers-test			\
	sanity-test				\
	socket-test				\
	queue-test

check_PROGRAMS =				\
	$(TESTS)				\
	exec-fd-leak-checker

noinst_PROGRAMS =				\
	fixed-benchmark

test_runner_src = test-runner.c test-runner.h test-helpers.c

array_test_SOURCES = array-test.c $(test_runner_src)
client_test_SOURCES = client-test.c $(test_runner_src)
display_test_SOURCES = display-test.c $(test_runner_src)
connection_test_SOURCES = connection-test.c $(test_runner_src)
event_loop_test_SOURCES = event-loop-test.c $(test_runner_src)
fixed_test_SOURCES = fixed-test.c $(test_runner_src)
list_test_SOURCES = list-test.c $(test_runner_src)
map_test_SOURCES = map-test.c $(test_runner_src)
sanity_test_SOURCES = sanity-test.c $(test_runner_src)
socket_test_SOURCES = socket-test.c $(test_runner_src)
queue_test_SOURCES = queue-test.c $(test_runner_src)

fixed_benchmark_SOURCES = fixed-benchmark.c

os_wrappers_test_SOURCES = 			\
	os-wrappers-test.c			\
	../src/wayland-os.c			\
	$(test_runner_src)

AM_CPPFLAGS = -I$(top_builddir)/src -I$(top_srcdir)/src
AM_CFLAGS = $(GCC_CFLAGS) $(FFI_CFLAGS)
LDADD = $(top_builddir)/src/libwayland-util.la \
	$(top_builddir)/src/libwayland-client.la \
	$(top_builddir)/src/libwayland-server.la \
	-lrt -ldl $(FFI_LIBS)

exec_fd_leak_checker_SOURCES =			\
	exec-fd-leak-checker.c			\
	test-runner.h				\
	test-helpers.c
exec_fd_leak_checker_LDADD =
