bin_PROGRAMS = weston				\
	$(weston_launch)

AM_CPPFLAGS =					\
	-I$(top_srcdir)/shared			\
	-DDATADIR='"$(datadir)"'		\
	-DMODULEDIR='"$(moduledir)"'		\
	-DLIBEXECDIR='"$(libexecdir)"'		\
	-DIN_WESTON

weston_LDFLAGS = -export-dynamic
weston_CFLAGS = $(GCC_CFLAGS) $(COMPOSITOR_CFLAGS) $(LIBUNWIND_CFLAGS)
weston_LDADD = $(COMPOSITOR_LIBS) $(LIBUNWIND_LIBS) \
	$(DLOPEN_LIBS) -lm ../shared/libshared.la

weston_SOURCES =				\
	git-version.h				\
	log.c					\
	compositor.c				\
	compositor.h				\
	filter.c				\
	filter.h				\
	screenshooter.c				\
	screenshooter-protocol.c		\
	screenshooter-server-protocol.h		\
	clipboard.c				\
	text-cursor-position-protocol.c		\
	text-cursor-position-server-protocol.h	\
	zoom.c					\
	text-backend.c				\
	text-protocol.c				\
	text-server-protocol.h			\
	input-method-protocol.c			\
	input-method-server-protocol.h		\
	workspaces-protocol.c			\
	workspaces-server-protocol.h		\
	bindings.c				\
	animation.c				\
	gl-renderer.h				\
	noop-renderer.c				\
	pixman-renderer.c			\
	pixman-renderer.h			\
	../shared/matrix.c			\
	../shared/matrix.h			\
	weston-launch.h				\
	weston-egl-ext.h

if ENABLE_EGL
weston_SOURCES +=				\
	gl-renderer.c
endif

git-version.h : .FORCE
	$(AM_V_GEN)(echo "#define BUILD_ID \"$(shell git --git-dir=$(top_srcdir)/.git describe --always --dirty) $(shell git --git-dir=$(top_srcdir)/.git log -1 --format='%s (%ci)')\"" > $@-new; \
	cmp -s $@ $@-new || cp $@-new $@; \
	rm $@-new)

.FORCE :

if ENABLE_XWAYLAND
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

endif # BUILD_WESTON_LAUNCH

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = weston.pc

westonincludedir = $(includedir)/weston
westoninclude_HEADERS =				\
	version.h				\
	compositor.h				\
	../shared/matrix.h			\
	../shared/config-parser.h

moduledir = $(libdir)/weston
module_LTLIBRARIES =				\
	$(desktop_shell)			\
	$(tablet_shell)				\
	$(x11_backend)				\
	$(drm_backend)				\
	$(wayland_backend)			\
	$(headless_backend)			\
	$(fbdev_backend)			\
	$(rdp_backend)

noinst_LTLIBRARIES =

if INSTALL_RPI_COMPOSITOR
module_LTLIBRARIES += $(rpi_backend)
else
noinst_LTLIBRARIES += $(rpi_backend)
endif

if ENABLE_X11_COMPOSITOR
x11_backend = x11-backend.la
x11_backend_la_LDFLAGS = -module -avoid-version
x11_backend_la_LIBADD = $(COMPOSITOR_LIBS) $(X11_COMPOSITOR_LIBS) \
	../shared/libshared-cairo.la
x11_backend_la_CFLAGS =				\
	$(COMPOSITOR_CFLAGS)			\
	$(PIXMAN_CFLAGS)			\
	$(CAIRO_CFLAGS)				\
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
	udev-seat.c				\
	udev-seat.h				\
	evdev.c					\
	evdev.h					\
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
	../shared/libshared-cairo.la
wayland_backend_la_CFLAGS =			\
	$(COMPOSITOR_CFLAGS)			\
	$(PIXMAN_CFLAGS)			\
	$(CAIRO_CFLAGS)				\
	$(WAYLAND_COMPOSITOR_CFLAGS)		\
	$(GCC_CFLAGS)
wayland_backend_la_SOURCES = compositor-wayland.c
endif

if ENABLE_RPI_COMPOSITOR
rpi_backend = rpi-backend.la
rpi_backend_la_LDFLAGS = -module -avoid-version
rpi_backend_la_LIBADD = $(COMPOSITOR_LIBS)	\
	$(RPI_COMPOSITOR_LIBS)			\
	$(RPI_BCM_HOST_LIBS)			\
	../shared/libshared.la
rpi_backend_la_CFLAGS =				\
	$(GCC_CFLAGS)				\
	$(COMPOSITOR_CFLAGS)			\
	$(RPI_COMPOSITOR_CFLAGS)		\
	$(RPI_BCM_HOST_CFLAGS)
rpi_backend_la_SOURCES =			\
	compositor-rpi.c			\
	rpi-bcm-stubs.h 			\
	tty.c					\
	evdev.c					\
	evdev.h					\
	evdev-touchpad.c
endif

if ENABLE_HEADLESS_COMPOSITOR
headless_backend = headless-backend.la
headless_backend_la_LDFLAGS = -module -avoid-version
headless_backend_la_LIBADD = $(COMPOSITOR_LIBS) \
	../shared/libshared.la
headless_backend_la_CFLAGS =			\
	$(COMPOSITOR_CFLAGS)			\
	$(GCC_CFLAGS)
headless_backend_la_SOURCES = compositor-headless.c
endif

if ENABLE_FBDEV_COMPOSITOR
fbdev_backend = fbdev-backend.la
fbdev_backend_la_LDFLAGS = -module -avoid-version
fbdev_backend_la_LIBADD = \
	$(COMPOSITOR_LIBS) \
	$(FBDEV_COMPOSITOR_LIBS) \
	../shared/libshared.la
fbdev_backend_la_CFLAGS = \
	$(COMPOSITOR_CFLAGS) \
	$(FBDEV_COMPOSITOR_CFLAGS) \
	$(PIXMAN_CFLAGS) \
	$(GCC_CFLAGS)
fbdev_backend_la_SOURCES = \
	compositor-fbdev.c \
	tty.c \
	udev-seat.c \
	udev-seat.h \
	evdev.c \
	evdev.h \
	evdev-touchpad.c \
	launcher-util.c
endif

if ENABLE_RDP_COMPOSITOR
rdp_backend = rdp-backend.la
rdp_backend_la_LDFLAGS = -module -avoid-version
rdp_backend_la_LIBADD = $(COMPOSITOR_LIBS) \
	$(RDP_COMPOSITOR_LIBS) \
	../shared/libshared.la
rdp_backend_la_CFLAGS =			\
	$(COMPOSITOR_CFLAGS)			\
	$(RDP_COMPOSITOR_CFLAGS) \
	$(GCC_CFLAGS)
rdp_backend_la_SOURCES = compositor-rdp.c
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
	text-cursor-position-server-protocol.h	\
	text-cursor-position-protocol.c		\
	tablet-shell-protocol.c			\
	tablet-shell-server-protocol.h		\
	desktop-shell-protocol.c		\
	desktop-shell-server-protocol.h		\
	text-protocol.c				\
	text-server-protocol.h			\
	input-method-protocol.c			\
	input-method-server-protocol.h		\
	workspaces-server-protocol.h		\
	workspaces-protocol.c			\
	git-version.h

CLEANFILES = $(BUILT_SOURCES)

@wayland_scanner_rules@
