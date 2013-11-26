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
	input.c					\
	data-device.c				\
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
	scaler-protocol.c			\
	scaler-server-protocol.h		\
	bindings.c				\
	animation.c				\
	noop-renderer.c				\
	pixman-renderer.c			\
	pixman-renderer.h			\
	../shared/matrix.c			\
	../shared/matrix.h			\
	../shared/zalloc.h			\
	weston-egl-ext.h

noinst_LTLIBRARIES =				\
	libsession-helper.la

libsession_helper_la_SOURCES =			\
	weston-launch.h				\
	launcher-util.c				\
	launcher-util.h
libsession_helper_la_CFLAGS = $(GCC_CFLAGS) $(LIBDRM_CFLAGS) $(PIXMAN_CFLAGS) $(COMPOSITOR_CFLAGS)
libsession_helper_la_LIBADD = $(LIBDRM_LIBS)

if ENABLE_DBUS
if HAVE_SYSTEMD_LOGIN
libsession_helper_la_SOURCES +=			\
	dbus.h					\
	dbus.c					\
	logind-util.h				\
	logind-util.c
libsession_helper_la_CFLAGS += $(SYSTEMD_LOGIN_CFLAGS) $(DBUS_CFLAGS)
libsession_helper_la_LIBADD += $(SYSTEMD_LOGIN_LIBS) $(DBUS_LIBS)
endif
endif

git-version.h : .FORCE
	$(AM_V_GEN)(echo "#define BUILD_ID \"$(shell git --git-dir=$(top_srcdir)/.git describe --always --dirty) $(shell git --git-dir=$(top_srcdir)/.git log -1 --format='%s (%ci)')\"" > $@-new; \
	cmp -s $@ $@-new || cp $@-new $@; \
	rm $@-new)

.FORCE :

if BUILD_WESTON_LAUNCH
weston_launch = weston-launch
weston_launch_SOURCES = weston-launch.c weston-launch.h
weston_launch_CPPFLAGS = -DBINDIR='"$(bindir)"'
weston_launch_CFLAGS=				\
	$(GCC_CFLAGS)				\
	$(PAM_CFLAGS)				\
	$(SYSTEMD_LOGIN_CFLAGS)			\
	$(LIBDRM_CFLAGS)
weston_launch_LDADD = $(PAM_LIBS) $(SYSTEMD_LOGIN_LIBS) $(LIBDRM_LIBS)

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
	../shared/config-parser.h		\
	../shared/zalloc.h

moduledir = $(libdir)/weston
module_LTLIBRARIES =				\
	$(cms_static)				\
	$(cms_colord)				\
	$(gl_renderer)				\
	$(x11_backend)				\
	$(drm_backend)				\
	$(wayland_backend)			\
	$(headless_backend)			\
	$(fbdev_backend)			\
	$(rdp_backend)

if INSTALL_RPI_COMPOSITOR
module_LTLIBRARIES += $(rpi_backend)
else
noinst_LTLIBRARIES += $(rpi_backend)
endif

if ENABLE_EGL
gl_renderer = gl-renderer.la
gl_renderer_la_LDFLAGS = -module -avoid-version
gl_renderer_la_LIBADD = $(COMPOSITOR_LIBS) $(EGL_LIBS)
gl_renderer_la_CFLAGS =				\
	$(COMPOSITOR_CFLAGS)			\
	$(EGL_CFLAGS)				\
	$(GCC_CFLAGS)
gl_renderer_la_SOURCES =			\
	gl-renderer.h				\
	gl-renderer.c				\
	vertex-clipping.c			\
	vertex-clipping.h
endif

if ENABLE_X11_COMPOSITOR
x11_backend = x11-backend.la
x11_backend_la_LDFLAGS = -module -avoid-version
x11_backend_la_LIBADD = $(COMPOSITOR_LIBS) $(X11_COMPOSITOR_LIBS) \
	../shared/libshared-cairo.la
x11_backend_la_CFLAGS =				\
	$(COMPOSITOR_CFLAGS)			\
	$(EGL_CFLAGS)				\
	$(PIXMAN_CFLAGS)			\
	$(CAIRO_CFLAGS)				\
	$(X11_COMPOSITOR_CFLAGS)		\
	$(GCC_CFLAGS)
x11_backend_la_SOURCES = compositor-x11.c
endif

if ENABLE_DRM_COMPOSITOR
drm_backend = drm-backend.la
drm_backend_la_LDFLAGS = -module -avoid-version
drm_backend_la_LIBADD =				\
	$(COMPOSITOR_LIBS)			\
	$(DRM_COMPOSITOR_LIBS)			\
	../shared/libshared.la -lrt		\
	libsession-helper.la
drm_backend_la_CFLAGS =				\
	$(COMPOSITOR_CFLAGS)			\
	$(EGL_CFLAGS)				\
	$(DRM_COMPOSITOR_CFLAGS)		\
	$(GCC_CFLAGS)
drm_backend_la_SOURCES =			\
	compositor-drm.c			\
	udev-seat.c				\
	udev-seat.h				\
	evdev.c					\
	evdev.h					\
	evdev-touchpad.c			\
	libbacklight.c				\
	libbacklight.h

if ENABLE_VAAPI_RECORDER
drm_backend_la_SOURCES += vaapi-recorder.c vaapi-recorder.h
drm_backend_la_LIBADD += $(LIBVA_LIBS)
drm_backend_la_CFLAGS += $(LIBVA_CFLAGS)
endif
endif

if ENABLE_WAYLAND_COMPOSITOR
wayland_backend = wayland-backend.la
wayland_backend_la_LDFLAGS = -module -avoid-version
wayland_backend_la_LIBADD =			\
	$(COMPOSITOR_LIBS)			\
	$(WAYLAND_COMPOSITOR_LIBS)		\
	../shared/libshared-cairo.la
wayland_backend_la_CFLAGS =			\
	$(COMPOSITOR_CFLAGS)			\
	$(EGL_CFLAGS)				\
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
	libsession-helper.la			\
	../shared/libshared.la
rpi_backend_la_CFLAGS =				\
	$(GCC_CFLAGS)				\
	$(COMPOSITOR_CFLAGS)			\
	$(RPI_COMPOSITOR_CFLAGS)		\
	$(RPI_BCM_HOST_CFLAGS)
rpi_backend_la_SOURCES =			\
	compositor-rpi.c			\
	rpi-renderer.c				\
	rpi-renderer.h				\
	rpi-bcm-stubs.h 			\
	udev-seat.c				\
	udev-seat.h				\
	evdev.c					\
	evdev.h					\
	evdev-touchpad.c

if ENABLE_EGL
rpi_backend_la_LIBADD += $(EGL_LIBS)
rpi_backend_la_CFLAGS += $(EGL_CFLAGS)
endif

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
	libsession-helper.la \
	../shared/libshared.la
fbdev_backend_la_CFLAGS = \
	$(COMPOSITOR_CFLAGS) \
	$(EGL_CFLAGS) \
	$(FBDEV_COMPOSITOR_CFLAGS) \
	$(PIXMAN_CFLAGS) \
	$(GCC_CFLAGS)
fbdev_backend_la_SOURCES = \
	compositor-fbdev.c \
	udev-seat.c \
	udev-seat.h \
	evdev.c \
	evdev.h \
	evdev-touchpad.c
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

if HAVE_LCMS
cms_static = cms-static.la
cms_static_la_LDFLAGS = -module -avoid-version
cms_static_la_LIBADD = $(COMPOSITOR_LIBS) $(LCMS_LIBS) ../shared/libshared.la
cms_static_la_CFLAGS = $(GCC_CFLAGS) $(COMPOSITOR_CFLAGS) $(LCMS_CFLAGS)
cms_static_la_SOURCES =				\
	cms-static.c				\
	cms-helper.c				\
	cms-helper.h
if ENABLE_COLORD
cms_colord = cms-colord.la
cms_colord_la_LDFLAGS = -module -avoid-version
cms_colord_la_LIBADD = $(COMPOSITOR_LIBS) $(COLORD_LIBS)
cms_colord_la_CFLAGS = $(GCC_CFLAGS) $(COMPOSITOR_CFLAGS) $(COLORD_CFLAGS)
cms_colord_la_SOURCES =				\
	cms-colord.c				\
	cms-helper.c				\
	cms-helper.h
endif
endif

noinst_PROGRAMS = spring-tool

spring_tool_CFLAGS = $(GCC_CFLAGS) $(COMPOSITOR_CFLAGS)
spring_tool_LDADD = $(COMPOSITOR_LIBS) -lm
spring_tool_SOURCES =				\
	spring-tool.c				\
	animation.c				\
	../shared/matrix.c			\
	../shared/matrix.h			\
	compositor.h

BUILT_SOURCES =					\
	screenshooter-server-protocol.h		\
	screenshooter-protocol.c		\
	text-cursor-position-server-protocol.h	\
	text-cursor-position-protocol.c		\
	text-protocol.c				\
	text-server-protocol.h			\
	input-method-protocol.c			\
	input-method-server-protocol.h		\
	workspaces-server-protocol.h		\
	workspaces-protocol.c			\
	scaler-server-protocol.h		\
	scaler-protocol.c			\
	git-version.h

CLEANFILES = $(BUILT_SOURCES)

wayland_protocoldir = $(top_srcdir)/protocol
include $(top_srcdir)/wayland-scanner.mk
