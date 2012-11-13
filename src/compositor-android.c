/*
 * Copyright © 2012 Collabora, Ltd.
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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <sys/types.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

#include <EGL/egl.h>

#include "compositor.h"
#include "gl-renderer.h"
#include "android-framebuffer.h"
#include "evdev.h"

struct android_compositor;

struct android_output {
	struct android_compositor *compositor;
	struct weston_output base;

	struct weston_mode mode;
	struct android_framebuffer *fb;
};

struct android_seat {
	struct weston_seat base;
	struct wl_list devices_list;
};

struct android_compositor {
	struct weston_compositor base;

	struct android_seat *seat;
};

static inline struct android_output *
to_android_output(struct weston_output *base)
{
	return container_of(base, struct android_output, base);
}

static inline struct android_seat *
to_android_seat(struct weston_seat *base)
{
	return container_of(base, struct android_seat, base);
}

static inline struct android_compositor *
to_android_compositor(struct weston_compositor *base)
{
	return container_of(base, struct android_compositor, base);
}

static void
android_finish_frame(void *data)
{
	struct android_output *output = data;

	weston_output_finish_frame(&output->base,
				   weston_compositor_get_time());
}

static void
android_output_repaint(struct weston_output *base, pixman_region32_t *damage)
{
	struct android_output *output = to_android_output(base);
        struct android_compositor *compositor = output->compositor;
	struct wl_event_loop *loop;

	compositor->base.renderer->repaint_output(&output->base, damage);

	/* FIXME: does Android have a way to signal page flip done? */
	loop = wl_display_get_event_loop(compositor->base.wl_display);
	wl_event_loop_add_idle(loop, android_finish_frame, output);
}

static void
android_output_destroy(struct weston_output *base)
{
	struct android_output *output = to_android_output(base);

	wl_list_remove(&output->base.link);
	weston_output_destroy(&output->base);

	gl_renderer_output_destroy(base);

	android_framebuffer_destroy(output->fb);

	free(output);
}

static struct android_output *
android_output_create(struct android_compositor *compositor)
{
	struct android_output *output;

	output = calloc(1, sizeof *output);
	if (!output)
		return NULL;

	output->fb = android_framebuffer_create();
	if (!output->fb) {
		free(output);
		return NULL;
	}

	output->compositor = compositor;
	return output;
}

static void
android_compositor_add_output(struct android_compositor *compositor,
			      struct android_output *output)
{
	float mm_width, mm_height;

	output->base.repaint = android_output_repaint;
	output->base.destroy = android_output_destroy;
	output->base.assign_planes = NULL;
	output->base.set_backlight = NULL;
	output->base.set_dpms = NULL;
	output->base.switch_mode = NULL;

	/* only one static mode in list */
	output->mode.flags =
		WL_OUTPUT_MODE_CURRENT | WL_OUTPUT_MODE_PREFERRED;
	output->mode.width = output->fb->width;
	output->mode.height = output->fb->height;
	output->mode.refresh = ceilf(1000.0f * output->fb->refresh_rate);
	wl_list_init(&output->base.mode_list);
	wl_list_insert(&output->base.mode_list, &output->mode.link);

	output->base.current = &output->mode;
	output->base.origin = &output->mode;
	output->base.subpixel = WL_OUTPUT_SUBPIXEL_UNKNOWN;
	output->base.make = "unknown";
	output->base.model = "unknown";

	mm_width  = output->fb->width / output->fb->xdpi * 25.4f;
	mm_height = output->fb->height / output->fb->ydpi * 25.4f;
	weston_output_init(&output->base, &compositor->base,
			   0, 0, round(mm_width), round(mm_height),
			   WL_OUTPUT_TRANSFORM_NORMAL);
	wl_list_insert(compositor->base.output_list.prev, &output->base.link);
}

static void
android_led_update(struct weston_seat *seat_base, enum weston_led leds)
{
	struct android_seat *seat = to_android_seat(seat_base);
	struct evdev_device *device;

	wl_list_for_each(device, &seat->devices_list, link)
		evdev_led_update(device, leds);
}

static void
android_seat_open_device(struct android_seat *seat, const char *devnode)
{
	struct evdev_device *device;
	int fd;

	/* XXX: check the Android excluded list */

	fd = open(devnode, O_RDWR | O_NONBLOCK | O_CLOEXEC);
	if (fd < 0) {
		weston_log_continue("opening '%s' failed: %s\n", devnode,
				    strerror(errno));
		return;
	}

	device = evdev_device_create(&seat->base, devnode, fd);
	if (!device) {
		close(fd);
		return;
	}

	wl_list_insert(seat->devices_list.prev, &device->link);
}

static int
is_dot_or_dotdot(const char *str)
{
	return (str[0] == '.' &&
		(str[1] == 0 || (str[1] == '.' && str[2] == 0)));
}

static void
android_seat_scan_devices(struct android_seat *seat, const char *dirpath)
{
	int ret;
	DIR *dir;
	struct dirent *dent;
	char *devnode = NULL;

	dir = opendir(dirpath);
	if (!dir) {
		weston_log("Could not open input device directory '%s': %s\n",
			   dirpath, strerror(errno));
		return;
	}

	while ((dent = readdir(dir)) != NULL) {
		if (is_dot_or_dotdot(dent->d_name))
			continue;

		ret = asprintf(&devnode, "%s/%s", dirpath, dent->d_name);
		if (ret < 0)
			continue;

		android_seat_open_device(seat, devnode);
		free(devnode);
	}

	closedir(dir);
}

static void
android_seat_destroy(struct android_seat *seat)
{
	struct evdev_device *device, *next;

	wl_list_for_each_safe(device, next, &seat->devices_list, link)
		evdev_device_destroy(device);

	if (seat->base.seat.keyboard)
		notify_keyboard_focus_out(&seat->base);

	weston_seat_release(&seat->base);
	free(seat);
}

static struct android_seat *
android_seat_create(struct android_compositor *compositor)
{
	struct android_seat *seat;

	seat = calloc(1, sizeof *seat);
	if (!seat)
		return NULL;

	weston_seat_init(&seat->base, &compositor->base);
	seat->base.led_update = android_led_update;
	wl_list_init(&seat->devices_list);

	android_seat_scan_devices(seat, "/dev/input");

	evdev_notify_keyboard_focus(&seat->base, &seat->devices_list);

	if (wl_list_empty(&seat->devices_list))
		weston_log("Warning: no input devices found.\n");

	/* XXX: implement hotplug support */

	return seat;
}

static int
android_init_egl(struct android_compositor *compositor,
		 struct android_output *output)
{
	EGLint visual_id = output->fb->format;

	if (gl_renderer_create(&compositor->base,
			EGL_DEFAULT_DISPLAY, gl_renderer_opaque_attribs,
			&visual_id) < 0)
		return -1;

	if (gl_renderer_output_create(&output->base,
			output->fb->native_window) < 0) {
		gl_renderer_destroy(&compositor->base);
		return -1;
	}

	return 0;
}

static void
android_compositor_destroy(struct weston_compositor *base)
{
	struct android_compositor *compositor = to_android_compositor(base);

	android_seat_destroy(compositor->seat);

	gl_renderer_destroy(base);

	/* destroys outputs, too */
	weston_compositor_shutdown(&compositor->base);

	free(compositor);
}

static struct weston_compositor *
android_compositor_create(struct wl_display *display, int argc, char *argv[],
			  const char *config_file)
{
	struct android_compositor *compositor;
	struct android_output *output;

	weston_log("initializing android backend\n");

	compositor = calloc(1, sizeof *compositor);
	if (compositor == NULL)
		return NULL;

	if (weston_compositor_init(&compositor->base, display, argc, argv,
				   config_file) < 0)
		goto err_free;

	compositor->base.destroy = android_compositor_destroy;

	compositor->base.focus = 1;

	output = android_output_create(compositor);
	if (!output)
		goto err_compositor;

	if (android_init_egl(compositor, output) < 0)
		goto err_output;

	android_compositor_add_output(compositor, output);

	compositor->seat = android_seat_create(compositor);
	if (!compositor->seat)
		goto err_gl;

	return &compositor->base;

err_gl:
	gl_renderer_destroy(&compositor->base);
err_output:
	android_output_destroy(&output->base);
err_compositor:
	weston_compositor_shutdown(&compositor->base);
err_free:
	free(compositor);
	return NULL;
}

WL_EXPORT struct weston_compositor *
backend_init(struct wl_display *display, int argc, char *argv[],
	     const char *config_file)
{
	return android_compositor_create(display, argc, argv, config_file);
}
