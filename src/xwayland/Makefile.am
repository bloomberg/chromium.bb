AM_CPPFLAGS =					\
	-I$(top_srcdir)/shared			\
	-I$(top_builddir)/src			\
	-DDATADIR='"$(datadir)"'		\
	-DMODULEDIR='"$(moduledir)"'		\
	-DLIBEXECDIR='"$(libexecdir)"'		\
	-DXSERVER_PATH='"@XSERVER_PATH@"'

moduledir = @libdir@/weston
module_LTLIBRARIES = xwayland.la

xwayland = xwayland.la
xwayland_la_LDFLAGS = -module -avoid-version
xwayland_la_LIBADD =			\
	$(XWAYLAND_LIBS)		\
	$(top_builddir)/shared/libshared-cairo.la
xwayland_la_CFLAGS =				\
	$(GCC_CFLAGS)				\
	$(COMPOSITOR_CFLAGS)			\
	$(PIXMAN_CFLAGS)			\
	$(CAIRO_CFLAGS)
xwayland_la_SOURCES =				\
	xwayland.h				\
	window-manager.c			\
	selection.c				\
	launcher.c				\
	xserver-protocol.c			\
	xserver-server-protocol.h		\
	hash.c					\
	hash.h

BUILT_SOURCES =					\
	xserver-protocol.c			\
	xserver-server-protocol.h

CLEANFILES = $(BUILT_SOURCES)

wayland_protocoldir = $(top_srcdir)/protocol
include $(top_srcdir)/wayland-scanner.mk
