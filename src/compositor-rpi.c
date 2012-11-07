/*
 * Copyright © 2008-2011 Kristian Høgsberg
 * Copyright © 2011 Intel Corporation
 * Copyright © 2012 Raspberry Pi Foundation
 *
 * Permission to use, copy, modify, distribute, and sell this software and
 * its documentation for any purpose is hereby granted without fee, provided
 * that the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of the copyright holders not be used in
 * advertising or publicity pertaining to distribution of the software
 * without specific, written prior permission.  The copyright holders make
 * no representations about the suitability of this software for any
 * purpose.  It is provided "as is" without express or implied warranty.
 *
 * THE COPYRIGHT HOLDERS DISCLAIM ALL WARRANTIES WITH REGARD TO THIS
 * SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS, IN NO EVENT SHALL THE COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER
 * RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF
 * CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#define _GNU_SOURCE

#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>

#include <libudev.h>

#include "config.h"

#ifdef HAVE_BCM_HOST
#  include <bcm_host.h>
#else
#  include "rpi-bcm-stubs.h"
#endif

#include "compositor.h"
#include "evdev.h"

struct rpi_compositor;

struct rpi_output {
	struct rpi_compositor *compositor;
	struct weston_output base;

	struct weston_mode mode;
	struct wl_event_source *finish_frame_timer;

	DISPMANX_DISPLAY_HANDLE_T display;
	EGL_DISPMANX_WINDOW_T egl_window;
	DISPMANX_ELEMENT_HANDLE_T egl_element;
};

struct rpi_seat {
	struct weston_seat base;
	struct wl_list devices_list;

	struct udev_monitor *udev_monitor;
	struct wl_event_source *udev_monitor_source;
	char *seat_id;
};

struct rpi_compositor {
	struct weston_compositor base;
	uint32_t prev_state;

	struct udev *udev;
	struct tty *tty;
};

static inline struct rpi_output *
to_rpi_output(struct weston_output *base)
{
	return container_of(base, struct rpi_output, base);
}

static inline struct rpi_seat *
to_rpi_seat(struct weston_seat *base)
{
	return container_of(base, struct rpi_seat, base);
}

static inline struct rpi_compositor *
to_rpi_compositor(struct weston_compositor *base)
{
	return container_of(base, struct rpi_compositor, base);
}

static const char *
egl_error_string(EGLint code)
{
#define MYERRCODE(x) case x: return #x;
	switch (code) {
	MYERRCODE(EGL_SUCCESS)
	MYERRCODE(EGL_NOT_INITIALIZED)
	MYERRCODE(EGL_BAD_ACCESS)
	MYERRCODE(EGL_BAD_ALLOC)
	MYERRCODE(EGL_BAD_ATTRIBUTE)
	MYERRCODE(EGL_BAD_CONTEXT)
	MYERRCODE(EGL_BAD_CONFIG)
	MYERRCODE(EGL_BAD_CURRENT_SURFACE)
	MYERRCODE(EGL_BAD_DISPLAY)
	MYERRCODE(EGL_BAD_SURFACE)
	MYERRCODE(EGL_BAD_MATCH)
	MYERRCODE(EGL_BAD_PARAMETER)
	MYERRCODE(EGL_BAD_NATIVE_PIXMAP)
	MYERRCODE(EGL_BAD_NATIVE_WINDOW)
	MYERRCODE(EGL_CONTEXT_LOST)
	default:
		return "unknown";
	}
#undef MYERRCODE
}

static void
print_egl_error_state(void)
{
	EGLint code;

	code = eglGetError();
	weston_log("EGL error state: %s (0x%04lx)\n",
		   egl_error_string(code), (long)code);
}

static int
rpi_finish_frame(void *data)
{
	struct rpi_output *output = data;

	weston_output_finish_frame(&output->base,
				   weston_compositor_get_time());
	return 1;
}

static void
rpi_output_repaint(struct weston_output *base, pixman_region32_t *damage)
{
	struct rpi_output *output = to_rpi_output(base);
	struct rpi_compositor *compositor = output->compositor;

	compositor->base.renderer->repaint_output(&output->base, damage);

	/* FIXME: hook into page flip done */
	wl_event_source_timer_update(output->finish_frame_timer, 10);
}

static void
rpi_output_destroy(struct weston_output *base)
{
	struct rpi_output *output = to_rpi_output(base);
	DISPMANX_UPDATE_HANDLE_T update;

	wl_event_source_remove(output->finish_frame_timer);

	eglDestroySurface(output->compositor->base.egl_display,
			  output->base.egl_surface);

	wl_list_remove(&output->base.link);
	weston_output_destroy(&output->base);

	update = vc_dispmanx_update_start(0);
	vc_dispmanx_element_remove(update, output->egl_element);
	vc_dispmanx_update_submit_sync(update);

	/* XXX: how to destroy Dispmanx objects? */
	vc_dispmanx_display_close(output->display);

	free(output);
}

static int
rpi_output_create(struct rpi_compositor *compositor)
{
	struct rpi_output *output;
	DISPMANX_MODEINFO_T modeinfo;
	DISPMANX_UPDATE_HANDLE_T update;
	VC_RECT_T dst_rect;
	VC_RECT_T src_rect;
	int ret;
	float mm_width, mm_height;
	VC_DISPMANX_ALPHA_T alpharules = {
		DISPMANX_FLAGS_ALPHA_FIXED_ALL_PIXELS,
		255, /* opacity 0-255 */
		0 /* mask resource handle */
	};
	struct wl_event_loop *loop;

	output = calloc(1, sizeof *output);
	if (!output)
		return -1;

	output->compositor = compositor;

	output->display = vc_dispmanx_display_open(DISPMANX_ID_HDMI);
	if (!output->display) {
		weston_log("Failed to open dispmanx HDMI display.");
		goto out_free;
	}

	ret = vc_dispmanx_display_get_info(output->display, &modeinfo);
	if (ret < 0) {
		weston_log("Failed to get display mode information.\n");
		goto out_dmx_close;
	}

	vc_dispmanx_rect_set(&dst_rect, 0, 0, modeinfo.width, modeinfo.height);
	vc_dispmanx_rect_set(&src_rect, 0, 0,
			     modeinfo.width << 16, modeinfo.height << 16);

	update = vc_dispmanx_update_start(0);
	output->egl_element = vc_dispmanx_element_add(update,
						  output->display,
						  0 /* layer */,
						  &dst_rect,
						  0 /* src resource */,
						  &src_rect,
						  DISPMANX_PROTECTION_NONE,
						  &alpharules,
						  NULL /* clamp */,
						  DISPMANX_NO_ROTATE);
	vc_dispmanx_update_submit_sync(update);

	output->egl_window.element = output->egl_element;
	output->egl_window.width = modeinfo.width;
	output->egl_window.height = modeinfo.height;

	output->base.egl_surface =
		eglCreateWindowSurface(compositor->base.egl_display,
				       compositor->base.egl_config,
				       (EGLNativeWindowType)&output->egl_window,
				       NULL);
	if (output->base.egl_surface == EGL_NO_SURFACE) {
		print_egl_error_state();
		weston_log("Failed to create output surface.\n");
		goto out_dmx;
	}

	if (!eglSurfaceAttrib(compositor->base.egl_display,
			      output->base.egl_surface,
			      EGL_SWAP_BEHAVIOR, EGL_BUFFER_PRESERVED)) {
		print_egl_error_state();
		weston_log("Failed to set swap behaviour to preserved.\n");
		goto out_surface;
	}

	output->base.repaint = rpi_output_repaint;
	output->base.destroy = rpi_output_destroy;
	output->base.assign_planes = NULL;
	output->base.set_backlight = NULL;
	output->base.set_dpms = NULL;
	output->base.switch_mode = NULL;

	/* XXX: use tvservice to get information from and control the
	 * HDMI and SDTV outputs. See:
	 * /opt/vc/include/interface/vmcs_host/vc_tvservice.h
	 */

	/* only one static mode in list */
	output->mode.flags =
		WL_OUTPUT_MODE_CURRENT | WL_OUTPUT_MODE_PREFERRED;
	output->mode.width = modeinfo.width;
	output->mode.height = modeinfo.height;
	output->mode.refresh = 60000;
	wl_list_init(&output->base.mode_list);
	wl_list_insert(&output->base.mode_list, &output->mode.link);

	output->base.current = &output->mode;
	output->base.origin = &output->mode;
	output->base.subpixel = WL_OUTPUT_SUBPIXEL_UNKNOWN;
	output->base.make = "unknown";
	output->base.model = "unknown";

	/* guess 96 dpi */
	mm_width  = modeinfo.width * (25.4f / 96.0f);
	mm_height = modeinfo.height * (25.4f / 96.0f);

	weston_output_init(&output->base, &compositor->base,
			   0, 0, round(mm_width), round(mm_height),
			   WL_OUTPUT_TRANSFORM_NORMAL);
	wl_list_insert(compositor->base.output_list.prev, &output->base.link);

	loop = wl_display_get_event_loop(compositor->base.wl_display);
	output->finish_frame_timer =
		wl_event_loop_add_timer(loop, rpi_finish_frame, output);

	weston_log("Raspberry Pi HDMI output %dx%d px\n",
		   output->mode.width, output->mode.height);
	weston_log_continue(STAMP_SPACE "guessing %d Hz and 96 dpi\n",
			    output->mode.refresh / 1000);

	return 0;

out_surface:
	eglDestroySurface(compositor->base.egl_display,
			  output->base.egl_surface);

out_dmx:
	/* XXX: how to destroy Dispmanx objects? */

out_dmx_close:
	vc_dispmanx_display_close(output->display);

out_free:
	free(output);
	return -1;
}

static void
rpi_led_update(struct weston_seat *seat_base, enum weston_led leds)
{
	struct rpi_seat *seat = to_rpi_seat(seat_base);
	struct evdev_device *device;

	wl_list_for_each(device, &seat->devices_list, link)
		evdev_led_update(device, leds);
}

static const char default_seat[] = "seat0";

static void
device_added(struct udev_device *udev_device, struct rpi_seat *master)
{
	struct evdev_device *device;
	const char *devnode;
	const char *device_seat;
	int fd;

	device_seat = udev_device_get_property_value(udev_device, "ID_SEAT");
	if (!device_seat)
		device_seat = default_seat;

	if (strcmp(device_seat, master->seat_id))
		return;

	devnode = udev_device_get_devnode(udev_device);

	/* Use non-blocking mode so that we can loop on read on
	 * evdev_device_data() until all events on the fd are
	 * read.  mtdev_get() also expects this. */
	fd = open(devnode, O_RDWR | O_NONBLOCK | O_CLOEXEC);
	if (fd < 0) {
		weston_log("opening input device '%s' failed.\n", devnode);
		return;
	}

	device = evdev_device_create(&master->base, devnode, fd);
	if (!device) {
		close(fd);
		weston_log("not using input device '%s'.\n", devnode);
		return;
	}

	wl_list_insert(master->devices_list.prev, &device->link);
}

static void
evdev_add_devices(struct udev *udev, struct weston_seat *seat_base)
{
	struct rpi_seat *seat = to_rpi_seat(seat_base);
	struct udev_enumerate *e;
	struct udev_list_entry *entry;
	struct udev_device *device;
	const char *path, *sysname;

	e = udev_enumerate_new(udev);
	udev_enumerate_add_match_subsystem(e, "input");
	udev_enumerate_scan_devices(e);
	udev_list_entry_foreach(entry, udev_enumerate_get_list_entry(e)) {
		path = udev_list_entry_get_name(entry);
		device = udev_device_new_from_syspath(udev, path);

		sysname = udev_device_get_sysname(device);
		if (strncmp("event", sysname, 5) != 0) {
			udev_device_unref(device);
			continue;
		}

		device_added(device, seat);

		udev_device_unref(device);
	}
	udev_enumerate_unref(e);

	evdev_notify_keyboard_focus(&seat->base, &seat->devices_list);

	if (wl_list_empty(&seat->devices_list)) {
		weston_log(
			"warning: no input devices on entering Weston. "
			"Possible causes:\n"
			"\t- no permissions to read /dev/input/event*\n"
			"\t- seats misconfigured "
			"(Weston backend option 'seat', "
			"udev device property ID_SEAT)\n");
	}
}

static int
evdev_udev_handler(int fd, uint32_t mask, void *data)
{
	struct rpi_seat *seat = data;
	struct udev_device *udev_device;
	struct evdev_device *device, *next;
	const char *action;
	const char *devnode;

	udev_device = udev_monitor_receive_device(seat->udev_monitor);
	if (!udev_device)
		return 1;

	action = udev_device_get_action(udev_device);
	if (!action)
		goto out;

	if (strncmp("event", udev_device_get_sysname(udev_device), 5) != 0)
		goto out;

	if (!strcmp(action, "add")) {
		device_added(udev_device, seat);
	}
	else if (!strcmp(action, "remove")) {
		devnode = udev_device_get_devnode(udev_device);
		wl_list_for_each_safe(device, next, &seat->devices_list, link)
			if (!strcmp(device->devnode, devnode)) {
				weston_log("input device %s, %s removed\n",
					   device->devname, device->devnode);
				evdev_device_destroy(device);
				break;
			}
	}

out:
	udev_device_unref(udev_device);

	return 0;
}

static int
evdev_enable_udev_monitor(struct udev *udev, struct weston_seat *seat_base)
{
	struct rpi_seat *master = to_rpi_seat(seat_base);
	struct wl_event_loop *loop;
	struct weston_compositor *c = master->base.compositor;
	int fd;

	master->udev_monitor = udev_monitor_new_from_netlink(udev, "udev");
	if (!master->udev_monitor) {
		weston_log("udev: failed to create the udev monitor\n");
		return 0;
	}

	udev_monitor_filter_add_match_subsystem_devtype(master->udev_monitor,
			"input", NULL);

	if (udev_monitor_enable_receiving(master->udev_monitor)) {
		weston_log("udev: failed to bind the udev monitor\n");
		udev_monitor_unref(master->udev_monitor);
		return 0;
	}

	loop = wl_display_get_event_loop(c->wl_display);
	fd = udev_monitor_get_fd(master->udev_monitor);
	master->udev_monitor_source =
		wl_event_loop_add_fd(loop, fd, WL_EVENT_READABLE,
				     evdev_udev_handler, master);
	if (!master->udev_monitor_source) {
		udev_monitor_unref(master->udev_monitor);
		return 0;
	}

	return 1;
}

static void
evdev_disable_udev_monitor(struct weston_seat *seat_base)
{
	struct rpi_seat *seat = to_rpi_seat(seat_base);

	if (!seat->udev_monitor)
		return;

	udev_monitor_unref(seat->udev_monitor);
	seat->udev_monitor = NULL;
	wl_event_source_remove(seat->udev_monitor_source);
	seat->udev_monitor_source = NULL;
}

static void
evdev_input_create(struct weston_compositor *c, struct udev *udev,
		   const char *seat_id)
{
	struct rpi_seat *seat;

	seat = malloc(sizeof *seat);
	if (seat == NULL)
		return;

	memset(seat, 0, sizeof *seat);
	weston_seat_init(&seat->base, c);
	seat->base.led_update = rpi_led_update;

	wl_list_init(&seat->devices_list);
	seat->seat_id = strdup(seat_id);
	if (!evdev_enable_udev_monitor(udev, &seat->base)) {
		free(seat->seat_id);
		free(seat);
		return;
	}

	evdev_add_devices(udev, &seat->base);
}

static void
evdev_remove_devices(struct weston_seat *seat_base)
{
	struct rpi_seat *seat = to_rpi_seat(seat_base);
	struct evdev_device *device, *next;

	wl_list_for_each_safe(device, next, &seat->devices_list, link)
		evdev_device_destroy(device);

	if (seat->base.seat.keyboard)
		notify_keyboard_focus_out(&seat->base);
}

static void
evdev_input_destroy(struct weston_seat *seat_base)
{
	struct rpi_seat *seat = to_rpi_seat(seat_base);

	evdev_remove_devices(seat_base);
	evdev_disable_udev_monitor(&seat->base);

	weston_seat_release(seat_base);
	free(seat->seat_id);
	free(seat);
}

static int
rpi_init_egl(struct rpi_compositor *compositor)
{
	EGLint eglmajor, eglminor;
	int ret;
	EGLint num_config;

	static const EGLint config_attrs[] = {
		EGL_SURFACE_TYPE, EGL_WINDOW_BIT |
				  EGL_SWAP_BEHAVIOR_PRESERVED_BIT,
		EGL_RED_SIZE, 1,
		EGL_GREEN_SIZE, 1,
		EGL_BLUE_SIZE, 1,
		EGL_ALPHA_SIZE, 0,
		EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
		EGL_NONE
	};

	compositor->base.egl_display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
	if (compositor->base.egl_display == EGL_NO_DISPLAY) {
		weston_log("Failed to create EGL display.\n");
		print_egl_error_state();
		return -1;
	}

	ret = eglInitialize(compositor->base.egl_display, &eglmajor, &eglminor);
	if (!ret) {
		weston_log("Failed to initialise EGL.\n");
		print_egl_error_state();
		return -1;
	}

	ret = eglChooseConfig(compositor->base.egl_display, config_attrs,
			      &compositor->base.egl_config, 1, &num_config);
	if (ret < 0 || num_config != 1) {
		weston_log("Failed to find an EGL config (found %d configs).\n",
			   num_config);
		print_egl_error_state();
		return -1;
	}

	return 0;
}

static void
rpi_fini_egl(struct rpi_compositor *compositor)
{
	gles2_renderer_destroy(&compositor->base);

	eglMakeCurrent(compositor->base.egl_display,
		       EGL_NO_SURFACE, EGL_NO_SURFACE,
		       EGL_NO_CONTEXT);

	eglTerminate(compositor->base.egl_display);
	eglReleaseThread();
}

static void
rpi_compositor_destroy(struct weston_compositor *base)
{
	struct rpi_compositor *compositor = to_rpi_compositor(base);
	struct weston_seat *seat, *next;

	wl_list_for_each_safe(seat, next, &compositor->base.seat_list, link)
		evdev_input_destroy(seat);

	/* destroys outputs, too */
	weston_compositor_shutdown(&compositor->base);

	rpi_fini_egl(compositor);
	tty_destroy(compositor->tty);

	bcm_host_deinit();
	free(compositor);
}

static void
vt_func(struct weston_compositor *base, int event)
{
	struct rpi_compositor *compositor = to_rpi_compositor(base);
	struct weston_seat *seat;
	struct weston_output *output;

	switch (event) {
	case TTY_ENTER_VT:
		weston_log("entering VT\n");
		compositor->base.focus = 1;
		compositor->base.state = compositor->prev_state;
		weston_compositor_damage_all(&compositor->base);
		wl_list_for_each(seat, &compositor->base.seat_list, link) {
			evdev_add_devices(compositor->udev, seat);
			evdev_enable_udev_monitor(compositor->udev, seat);
		}
		break;
	case TTY_LEAVE_VT:
		weston_log("leaving VT\n");
		wl_list_for_each(seat, &compositor->base.seat_list, link) {
			evdev_disable_udev_monitor(seat);
			evdev_remove_devices(seat);
		}

		compositor->base.focus = 0;
		compositor->prev_state = compositor->base.state;
		compositor->base.state = WESTON_COMPOSITOR_SLEEPING;

		/* If we have a repaint scheduled (either from a
		 * pending pageflip or the idle handler), make sure we
		 * cancel that so we don't try to pageflip when we're
		 * vt switched away.  The SLEEPING state will prevent
		 * further attemps at repainting.  When we switch
		 * back, we schedule a repaint, which will process
		 * pending frame callbacks. */

		wl_list_for_each(output,
				 &compositor->base.output_list, link) {
			output->repaint_needed = 0;
		}

		break;
	};
}

static void
rpi_restore(struct weston_compositor *base)
{
	struct rpi_compositor *compositor = to_rpi_compositor(base);

	tty_reset(compositor->tty);
}

static void
switch_vt_binding(struct wl_seat *seat, uint32_t time, uint32_t key, void *data)
{
	struct rpi_compositor *ec = data;

	tty_activate_vt(ec->tty, key - KEY_F1 + 1);
}

static struct weston_compositor *
rpi_compositor_create(struct wl_display *display, int argc, char *argv[],
		      const char *config_file, int tty)
{
	struct rpi_compositor *compositor;
	struct weston_output *output;
	const char *seat = default_seat;
	uint32_t key;

	weston_log("initializing Raspberry Pi backend\n");

	compositor = calloc(1, sizeof *compositor);
	if (compositor == NULL)
		return NULL;

	if (weston_compositor_init(&compositor->base, display, argc, argv,
				   config_file) < 0)
		goto out_free;

	compositor->udev = udev_new();
	if (compositor->udev == NULL) {
		weston_log("Failed to initialize udev context.\n");
		goto out_compositor;
	}

	compositor->tty = tty_create(&compositor->base, vt_func, tty);
	if (!compositor->tty) {
		weston_log("Failed to initialize tty.\n");
		goto out_udev;
	}

	compositor->base.destroy = rpi_compositor_destroy;
	compositor->base.restore = rpi_restore;

	compositor->base.focus = 1;
	compositor->prev_state = WESTON_COMPOSITOR_ACTIVE;

	for (key = KEY_F1; key < KEY_F9; key++)
		weston_compositor_add_key_binding(&compositor->base, key,
						  MODIFIER_CTRL | MODIFIER_ALT,
						  switch_vt_binding, compositor);

	bcm_host_init();

	if (rpi_init_egl(compositor) < 0)
		goto out_tty;

	if (rpi_output_create(compositor) < 0)
		goto out_egl;

	if (gles2_renderer_init(&compositor->base) < 0)
		goto out_output;

	evdev_input_create(&compositor->base, compositor->udev, seat);

	return &compositor->base;

out_output:
	wl_list_for_each(output, &compositor->base.output_list, link)
		rpi_output_destroy(output);

out_egl:
	rpi_fini_egl(compositor);

out_tty:
	tty_destroy(compositor->tty);

out_udev:
	udev_unref(compositor->udev);

out_compositor:
	weston_compositor_shutdown(&compositor->base);

out_free:
	bcm_host_deinit();
	free(compositor);

	return NULL;
}

WL_EXPORT struct weston_compositor *
backend_init(struct wl_display *display, int argc, char *argv[],
	     const char *config_file)
{
	int tty = 0; /* default to current tty */

	const struct weston_option rpi_options[] = {
		{ WESTON_OPTION_INTEGER, "tty", 0, &tty },
	};

	parse_options(rpi_options, ARRAY_LENGTH(rpi_options), argc, argv);

	return rpi_compositor_create(display, argc, argv, config_file, tty);
}
