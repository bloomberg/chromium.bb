if BUILD_TESTS

noinst_PROGRAMS = matrix-test

endif


AM_CFLAGS = $(GCC_CFLAGS)
AM_CPPFLAGS = -I$(top_srcdir)/src -DUNIT_TEST $(COMPOSITOR_CFLAGS)

matrix_test_SOURCES =				\
	matrix-test.c				\
	$(top_srcdir)/src/matrix.c		\
	$(top_srcdir)/src/matrix.h
matrix_test_LDADD = -lm -lrt
