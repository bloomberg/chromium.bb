AM_CPPFLAGS =					\
	-DDATADIR='"$(datadir)"'		\
	-DMODULEDIR='"$(moduledir)"'		\
	-DLIBEXECDIR='"$(libexecdir)"'		\
	-DXSERVER_PATH='"@XSERVER_PATH@"'

moduledir = @libdir@/weston
module_LTLIBRARIES = xserver-launcher.la

xserver_launcher = xserver-launcher.la
xserver_launcher_la_LDFLAGS = -module -avoid-version
xserver_launcher_la_LIBADD =			\
	$(XSERVER_LAUNCHER_LIBS)		\
	$(top_builddir)/shared/libshared.la
xserver_launcher_la_CFLAGS = $(GCC_CFLAGS) $(COMPOSITOR_CFLAGS)
xserver_launcher_la_SOURCES =			\
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

@wayland_scanner_rules@
