TESTS = $(check_PROGRAMS)

check_PROGRAMS = matrix-test

noinst_PROGRAMS = setbacklight

AM_CFLAGS = $(GCC_CFLAGS)
AM_CPPFLAGS = -I$(top_srcdir)/src -DUNIT_TEST $(COMPOSITOR_CFLAGS)

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
