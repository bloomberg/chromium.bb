noinst_LTLIBRARIES = libshared.la libshared-cairo.la

libshared_la_CFLAGS = $(GCC_CFLAGS) $(COMPOSITOR_CFLAGS)

libshared_la_SOURCES =				\
	config-parser.c				\
	option-parser.c				\
	config-parser.h				\
	os-compatibility.c			\
	os-compatibility.h

libshared_cairo_la_CFLAGS =			\
	$(GCC_CFLAGS)				\
	$(COMPOSITOR_CFLAGS)			\
	$(PIXMAN_CFLAGS)			\
	$(CAIRO_CFLAGS)				\
	$(PNG_CFLAGS)				\
	$(WEBP_CFLAGS)

libshared_cairo_la_LIBADD =			\
	$(PIXMAN_LIBS)				\
	$(CAIRO_LIBS)				\
	$(PNG_LIBS)				\
	$(WEBP_LIBS)				\
	$(JPEG_LIBS)

libshared_cairo_la_SOURCES =			\
	$(libshared_la_SOURCES)			\
	image-loader.c				\
	image-loader.h				\
	cairo-util.c				\
	cairo-util.h
