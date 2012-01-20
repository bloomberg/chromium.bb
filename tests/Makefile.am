if BUILD_TESTS

noinst_PROGRAMS = matrix-test

endif


AM_CFLAGS = $(GCC_CFLAGS)
AM_CPPFLAGS = -I../src -DUNIT_TEST

matrix_test_SOURCES =				\
	matrix-test.c				\
	../src/matrix.c				\
	../src/matrix.h
matrix_test_LDADD = -lm -lrt
