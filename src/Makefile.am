lib_LTLIBRARIES = libwayland-server.la libwayland-client.la
noinst_LTLIBRARIES = libwayland-util.la

include_HEADERS =				\
	wayland-util.h				\
	wayland-server-protocol.h		\
	wayland-server.h			\
	wayland-client-protocol.h		\
	wayland-client.h			\
	wayland-egl.h				\
	wayland-version.h

libwayland_util_la_SOURCES =			\
	connection.c				\
	wayland-util.c				\
	wayland-util.h				\
	wayland-os.c				\
	wayland-os.h				\
	wayland-private.h

libwayland_server_la_LIBADD = $(FFI_LIBS) libwayland-util.la -lrt -lm
libwayland_server_la_SOURCES =			\
	wayland-protocol.c			\
	wayland-server.c			\
	wayland-shm.c				\
	data-device.c				\
	event-loop.c

libwayland_client_la_LIBADD = $(FFI_LIBS) libwayland-util.la -lrt -lm
libwayland_client_la_SOURCES =			\
	wayland-protocol.c			\
	wayland-client.c

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = wayland-client.pc wayland-server.pc

AM_CPPFLAGS = $(FFI_CFLAGS)
AM_CFLAGS = $(GCC_CFLAGS)

protocoldir = $(top_srcdir)/protocol

if ENABLE_SCANNER
wayland_scanner = $(top_builddir)/src/wayland-scanner
else
wayland_scanner = wayland-scanner
endif

include $(top_srcdir)/src/scanner.mk

if ENABLE_SCANNER
bin_PROGRAMS = wayland-scanner

wayland_scanner_SOURCES =				\
	scanner.c

wayland_scanner_LDADD = $(EXPAT_LIBS) libwayland-util.la

$(BUILT_SOURCES) : wayland-scanner
endif

BUILT_SOURCES =					\
	wayland-server-protocol.h		\
	wayland-client-protocol.h		\
	wayland-protocol.c

CLEANFILES = $(BUILT_SOURCES)
DISTCLEANFILES = wayland-version.h
EXTRA_DIST = wayland-version.h.in
