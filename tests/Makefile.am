TESTS = $(check_PROGRAMS)


check_PROGRAMS = array-test map-test list-test connection-test event-loop-test

map_test_SOURCES = map-test.c test-runner.c test-runner.h
array_test_SOURCES = array-test.c test-runner.c
list_test_SOURCES = list-test.c test-runner.c
connection_test_SOURCES = connection-test.c test-runner.c
event_loop_test_SOURCES = event-loop-test.c test-runner.c

AM_CFLAGS = $(GCC_CFLAGS)
LDADD = $(top_builddir)/src/libwayland-util.la \
	$(top_builddir)/src/libwayland-server.la \
	-lrt -ldl $(FFI_LIBS)

