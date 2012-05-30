noinst_LTLIBRARIES = libshared.la

libshared_la_LIBADD = $(SHARED_LIBS)
AM_CPPFLAGS = $(SHARED_CFLAGS)
AM_CFLAGS = $(GCC_CFLAGS)

libshared_la_SOURCES =				\
	config-parser.c				\
	option-parser.c				\
	image-loader.c				\
	config-parser.h				\
	os-compatibility.c			\
	os-compatibility.h			\
	cairo-util.c				\
	cairo-util.h
