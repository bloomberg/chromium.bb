lib_LTLIBRARIES = libwayland-server.la libwayland-client.la
noinst_LTLIBRARIES = libwayland-util.la

include_HEADERS =				\
	wayland-util.h				\
	wayland-server-protocol.h		\
	wayland-server.h			\
	wayland-client-protocol.h		\
	wayland-client.h

libwayland_util_la_SOURCES =			\
	connection.c				\
	connection.h				\
	wayland-util.c				\
	wayland-util.h				\
	wayland-hash.c

libwayland_server_la_LIBADD = $(FFI_LIBS) libwayland-util.la
libwayland_server_la_SOURCES =			\
	wayland-protocol.c			\
	wayland-server.c			\
	event-loop.c

libwayland_client_la_LIBADD = $(FFI_LIBS) libwayland-util.la
libwayland_client_la_SOURCES =			\
	wayland-protocol.c			\
	wayland-client.c

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = wayland-client.pc wayland-server.pc

AM_CPPFLAGS = $(FFI_CFLAGS)
AM_CFLAGS = $(GCC_CFLAGS)

include $(top_srcdir)/wayland/scanner.mk

noinst_PROGRAMS = scanner

scanner_SOURCES =				\
	scanner.c

scanner_LDADD = $(EXPAT_LIBS) libwayland-util.la

$(BUILT_SOURCES) : scanner

BUILT_SOURCES =					\
	wayland-server-protocol.h		\
	wayland-client-protocol.h		\
	wayland-protocol.c

CLEANFILES = $(BUILT_SOURCES)
