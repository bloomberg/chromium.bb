my_check_programs =				\
	sanity-test				\
	array-test				\
	map-test				\
	list-test				\
	connection-test				\
	event-loop-test				\
	client-test				\
	os-wrappers-test

TESTS = $(my_check_programs)

check_PROGRAMS =				\
	$(my_check_programs)			\
	exec-fd-leak-checker

test_runner_src = test-runner.c test-runner.h test-helpers.c

sanity_test_SOURCES = sanity-test.c $(test_runner_src)
map_test_SOURCES = map-test.c $(test_runner_src)
array_test_SOURCES = array-test.c $(test_runner_src)
list_test_SOURCES = list-test.c $(test_runner_src)
connection_test_SOURCES = connection-test.c $(test_runner_src)
event_loop_test_SOURCES = event-loop-test.c $(test_runner_src)
client_test_SOURCES = client-test.c $(test_runner_src)

os_wrappers_test_SOURCES = 			\
	os-wrappers-test.c			\
	../src/wayland-os.c			\
	$(test_runner_src)

AM_CFLAGS = $(GCC_CFLAGS) $(FFI_CFLAGS)
LDADD = $(top_builddir)/src/libwayland-util.la \
	$(top_builddir)/src/libwayland-server.la \
	-lrt -ldl $(FFI_LIBS)

exec_fd_leak_checker_SOURCES =			\
	exec-fd-leak-checker.c			\
	test-runner.h				\
	test-helpers.c
exec_fd_leak_checker_LDADD =
