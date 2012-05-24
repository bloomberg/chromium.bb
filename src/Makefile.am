bin_PROGRAMS = weston				\
	$(weston_launch)			\
	wcap-decode

wcap_decode_SOURCES =				\
	wcap-decode.c

wcap_decode_CFLAGS = $(CAIRO_EGL_CFLAGS)
wcap_decode_LDADD = $(CAIRO_EGL_LIBS)

AM_CPPFLAGS =					\
	-DDATADIR='"$(datadir)"'		\
	-DMODULEDIR='"$(moduledir)"'		\
	-DLIBEXECDIR='"$(libexecdir)"'

weston_LDFLAGS = -export-dynamic
weston_CFLAGS = $(GCC_CFLAGS) $(COMPOSITOR_CFLAGS)
weston_LDADD = $(COMPOSITOR_LIBS) $(DLOPEN_LIBS) -lm ../shared/libshared.la

weston_SOURCES =				\
	compositor.c				\
	compositor.h				\
	filter.c				\
	filter.h				\
	screenshooter.c				\
	screenshooter-protocol.c		\
	screenshooter-server-protocol.h		\
	util.c					\
	matrix.c				\
	matrix.h				\
	weston-launch.h				\
	weston-egl-ext.h

if ENABLE_XSERVER_LAUNCHER
SUBDIRS = xwayland
endif

DIST_SUBDIRS = xwayland


if BUILD_WESTON_LAUNCH
weston_launch = weston-launch
weston_launch_SOURCES = weston-launch.c weston-launch.h
weston_launch_CFLAGS= $(GCC_CFLAGS)
weston_launch_CPPFLAGS = $(WESTON_LAUNCH_CFLAGS) $(SYSTEMD_LOGIN_CFLAGS) \
		 -DBINDIR='"$(bindir)"'
weston_launch_LDADD = $(WESTON_LAUNCH_LIBS) $(SYSTEMD_LOGIN_LIBS)

if ENABLE_SETUID_INSTALL
install-exec-hook:
	chown root $(DESTDIR)$(bindir)/weston-launch
	chmod u+s $(DESTDIR)$(bindir)/weston-launch
endif

else # BUILD_WESTON_LAUNCH

if ENABLE_SETUID_INSTALL
install-exec-hook:
	chown root $(DESTDIR)$(bindir)/weston
	chmod u+s $(DESTDIR)$(bindir)/weston
endif

endif # BUILD_WESTON_LAUNCH

moduledir = @libdir@/weston
module_LTLIBRARIES =				\
	$(desktop_shell)			\
	$(tablet_shell)				\
	$(x11_backend)				\
	$(drm_backend)				\
	$(wayland_backend)			\
	$(openwfd_backend)

if ENABLE_X11_COMPOSITOR
x11_backend = x11-backend.la
x11_backend_la_LDFLAGS = -module -avoid-version
x11_backend_la_LIBADD = $(COMPOSITOR_LIBS) $(X11_COMPOSITOR_LIBS) \
	../shared/libshared.la
x11_backend_la_CFLAGS =				\
	$(COMPOSITOR_CFLAGS)			\
	$(X11_COMPOSITOR_CFLAGS)		\
	$(GCC_CFLAGS)
x11_backend_la_SOURCES = compositor-x11.c
endif

if ENABLE_DRM_COMPOSITOR
drm_backend = drm-backend.la
drm_backend_la_LDFLAGS = -module -avoid-version
drm_backend_la_LIBADD = $(COMPOSITOR_LIBS) $(DRM_COMPOSITOR_LIBS) \
	../shared/libshared.la
drm_backend_la_CFLAGS =				\
	$(COMPOSITOR_CFLAGS)			\
	$(DRM_COMPOSITOR_CFLAGS)		\
	$(GCC_CFLAGS)
drm_backend_la_SOURCES =			\
	compositor-drm.c			\
	tty.c					\
	evdev.c					\
	evdev.h					\
	evdev-private.h				\
	evdev-touchpad.c			\
	launcher-util.c				\
	launcher-util.h				\
	libbacklight.c				\
	libbacklight.h
endif

if ENABLE_WAYLAND_COMPOSITOR
wayland_backend = wayland-backend.la
wayland_backend_la_LDFLAGS = -module -avoid-version
wayland_backend_la_LIBADD = $(COMPOSITOR_LIBS) $(WAYLAND_COMPOSITOR_LIBS) \
	../shared/libshared.la
wayland_backend_la_CFLAGS =			\
	$(COMPOSITOR_CFLAGS)			\
	$(WAYLAND_COMPOSITOR_CFLAGS)		\
	$(GCC_CFLAGS)
wayland_backend_la_SOURCES = compositor-wayland.c
endif

if ENABLE_OPENWFD_COMPOSITOR
openwfd_backend = openwfd-backend.la
openwfd_backend_la_LDFLAGS = -module -avoid-version
openwfd_backend_la_LIBADD = $(COMPOSITOR_LIBS) $(OPENWFD_COMPOSITOR_LIBS)
openwfd_backend_la_CFLAGS = $(OPENWFD_COMPOSITOR_CFLAGS) $(GCC_CFLAGS)
openwfd_backend_la_SOURCES = compositor-openwfd.c tty.c evdev.c evdev.h
endif

if ENABLE_DESKTOP_SHELL
desktop_shell = desktop-shell.la
desktop_shell_la_LDFLAGS = -module -avoid-version
desktop_shell_la_LIBADD = $(COMPOSITOR_LIBS)	\
	../shared/libshared.la
desktop_shell_la_CFLAGS = $(GCC_CFLAGS) $(COMPOSITOR_CFLAGS)
desktop_shell_la_SOURCES =			\
	shell.c					\
	desktop-shell-protocol.c		\
	desktop-shell-server-protocol.h
endif

if ENABLE_TABLET_SHELL
tablet_shell = tablet-shell.la
tablet_shell_la_LDFLAGS = -module -avoid-version
tablet_shell_la_LIBADD = $(COMPOSITOR_LIBS)
tablet_shell_la_CFLAGS = $(GCC_CFLAGS) $(COMPOSITOR_CFLAGS)
tablet_shell_la_SOURCES =			\
	tablet-shell.c				\
	tablet-shell-protocol.c			\
	tablet-shell-server-protocol.h
endif

BUILT_SOURCES =					\
	screenshooter-server-protocol.h		\
	screenshooter-protocol.c		\
	tablet-shell-protocol.c			\
	tablet-shell-server-protocol.h		\
	desktop-shell-protocol.c		\
	desktop-shell-server-protocol.h

CLEANFILES = $(BUILT_SOURCES)

@wayland_scanner_rules@
