/*
 * Copyright © 2012 Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial
 * portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT.  IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "config.h"

#include <stdint.h>
#include <stdlib.h>
#include <assert.h>
#include <signal.h>
#include <unistd.h>
#include <string.h>

#include "compositor.h"
#include "compositor/weston.h"
#include "weston-test-server-protocol.h"

#ifdef ENABLE_EGL
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include "weston-egl-ext.h"
#endif /* ENABLE_EGL */

#include "shared/helpers.h"
#include "shared/timespec-util.h"

struct weston_test {
	struct weston_compositor *compositor;
	struct weston_layer layer;
	struct weston_process process;
	struct weston_seat seat;
	bool is_seat_initialized;
};

struct weston_test_surface {
	struct weston_surface *surface;
	struct weston_view *view;
	int32_t x, y;
	struct weston_test *test;
};

static void
test_client_sigchld(struct weston_process *process, int status)
{
	struct weston_test *test =
		container_of(process, struct weston_test, process);

	/* Chain up from weston-test-runner's exit code so that automake
	 * knows the exit status and can report e.g. skipped tests. */
	if (WIFEXITED(status) && WEXITSTATUS(status) != 0)
		exit(WEXITSTATUS(status));

	/* In case the child aborted or segfaulted... */
	assert(status == 0);

	wl_display_terminate(test->compositor->wl_display);
}

static int
test_seat_init(struct weston_test *test)
{
	/* create our own seat */
	weston_seat_init(&test->seat, test->compositor, "test-seat");
	test->is_seat_initialized = true;

	/* add devices */
	weston_seat_init_pointer(&test->seat);
	if (weston_seat_init_keyboard(&test->seat, NULL) < 0)
		return -1;
	weston_seat_init_touch(&test->seat);

	return 0;
}

static struct weston_seat *
get_seat(struct weston_test *test)
{
	return &test->seat;
}

static void
notify_pointer_position(struct weston_test *test, struct wl_resource *resource)
{
	struct weston_seat *seat = get_seat(test);
	struct weston_pointer *pointer = weston_seat_get_pointer(seat);

	weston_test_send_pointer_position(resource, pointer->x, pointer->y);
}

static void
test_surface_committed(struct weston_surface *surface, int32_t sx, int32_t sy)
{
	struct weston_test_surface *test_surface = surface->committed_private;
	struct weston_test *test = test_surface->test;

	if (wl_list_empty(&test_surface->view->layer_link.link))
		weston_layer_entry_insert(&test->layer.view_list,
					  &test_surface->view->layer_link);

	weston_view_set_position(test_surface->view,
				 test_surface->x, test_surface->y);

	weston_view_update_transform(test_surface->view);

	test_surface->surface->is_mapped = true;
	test_surface->view->is_mapped = true;
}

static void
move_surface(struct wl_client *client, struct wl_resource *resource,
	     struct wl_resource *surface_resource,
	     int32_t x, int32_t y)
{
	struct weston_surface *surface =
		wl_resource_get_user_data(surface_resource);
	struct weston_test_surface *test_surface;

	test_surface = surface->committed_private;
	if (!test_surface) {
		test_surface = malloc(sizeof *test_surface);
		if (!test_surface) {
			wl_resource_post_no_memory(resource);
			return;
		}

		test_surface->view = weston_view_create(surface);
		if (!test_surface->view) {
			wl_resource_post_no_memory(resource);
			free(test_surface);
			return;
		}

		surface->committed_private = test_surface;
		surface->committed = test_surface_committed;
	}

	test_surface->surface = surface;
	test_surface->test = wl_resource_get_user_data(resource);
	test_surface->x = x;
	test_surface->y = y;
}

static void
move_pointer(struct wl_client *client, struct wl_resource *resource,
	     uint32_t tv_sec_hi, uint32_t tv_sec_lo, uint32_t tv_nsec,
	     int32_t x, int32_t y)
{
	struct weston_test *test = wl_resource_get_user_data(resource);
	struct weston_seat *seat = get_seat(test);
	struct weston_pointer *pointer = weston_seat_get_pointer(seat);
	struct weston_pointer_motion_event event = { 0 };
	struct timespec time;

	event = (struct weston_pointer_motion_event) {
		.mask = WESTON_POINTER_MOTION_REL,
		.dx = wl_fixed_to_double(wl_fixed_from_int(x) - pointer->x),
		.dy = wl_fixed_to_double(wl_fixed_from_int(y) - pointer->y),
	};

	timespec_from_proto(&time, tv_sec_hi, tv_sec_lo, tv_nsec);

	notify_motion(seat, &time, &event);

	notify_pointer_position(test, resource);
}

static void
send_button(struct wl_client *client, struct wl_resource *resource,
	    uint32_t tv_sec_hi, uint32_t tv_sec_lo, uint32_t tv_nsec,
	    int32_t button, uint32_t state)
{
	struct timespec time;

	struct weston_test *test = wl_resource_get_user_data(resource);
	struct weston_seat *seat = get_seat(test);

	timespec_from_proto(&time, tv_sec_hi, tv_sec_lo, tv_nsec);

	notify_button(seat, &time, button, state);
}

static void
send_axis(struct wl_client *client, struct wl_resource *resource,
	  uint32_t tv_sec_hi, uint32_t tv_sec_lo, uint32_t tv_nsec,
	  uint32_t axis, wl_fixed_t value)
{
	struct weston_test *test = wl_resource_get_user_data(resource);
	struct weston_seat *seat = get_seat(test);
	struct timespec time;
	struct weston_pointer_axis_event axis_event;

	timespec_from_proto(&time, tv_sec_hi, tv_sec_lo, tv_nsec);
	axis_event.axis = axis;
	axis_event.value = wl_fixed_to_double(value);
	axis_event.has_discrete = false;
	axis_event.discrete = 0;

	notify_axis(seat, &time, &axis_event);
}

static void
activate_surface(struct wl_client *client, struct wl_resource *resource,
		 struct wl_resource *surface_resource)
{
	struct weston_surface *surface = surface_resource ?
		wl_resource_get_user_data(surface_resource) : NULL;
	struct weston_test *test = wl_resource_get_user_data(resource);
	struct weston_seat *seat;
	struct weston_keyboard *keyboard;

	seat = get_seat(test);
	keyboard = weston_seat_get_keyboard(seat);
	if (surface) {
		weston_seat_set_keyboard_focus(seat, surface);
		notify_keyboard_focus_in(seat, &keyboard->keys,
					 STATE_UPDATE_AUTOMATIC);
	}
	else {
		notify_keyboard_focus_out(seat);
		weston_seat_set_keyboard_focus(seat, surface);
	}
}

static void
send_key(struct wl_client *client, struct wl_resource *resource,
	 uint32_t tv_sec_hi, uint32_t tv_sec_lo, uint32_t tv_nsec,
	 uint32_t key, enum wl_keyboard_key_state state)
{
	struct weston_test *test = wl_resource_get_user_data(resource);
	struct weston_seat *seat = get_seat(test);
	struct timespec time;

	timespec_from_proto(&time, tv_sec_hi, tv_sec_lo, tv_nsec);

	notify_key(seat, &time, key, state, STATE_UPDATE_AUTOMATIC);
}

static void
device_release(struct wl_client *client,
	       struct wl_resource *resource, const char *device)
{
	struct weston_test *test = wl_resource_get_user_data(resource);
	struct weston_seat *seat = get_seat(test);

	if (strcmp(device, "pointer") == 0) {
		weston_seat_release_pointer(seat);
	} else if (strcmp(device, "keyboard") == 0) {
		weston_seat_release_keyboard(seat);
	} else if (strcmp(device, "touch") == 0) {
		weston_seat_release_touch(seat);
	} else if (strcmp(device, "seat") == 0) {
		assert(test->is_seat_initialized &&
		       "Trying to release already released test seat");
		weston_seat_release(seat);
		test->is_seat_initialized = false;
	} else {
		assert(0 && "Unsupported device");
	}
}

static void
device_add(struct wl_client *client,
	   struct wl_resource *resource, const char *device)
{
	struct weston_test *test = wl_resource_get_user_data(resource);
	struct weston_seat *seat = get_seat(test);

	if (strcmp(device, "pointer") == 0) {
		weston_seat_init_pointer(seat);
	} else if (strcmp(device, "keyboard") == 0) {
		weston_seat_init_keyboard(seat, NULL);
	} else if (strcmp(device, "touch") == 0) {
		weston_seat_init_touch(seat);
	} else if (strcmp(device, "seat") == 0) {
		assert(!test->is_seat_initialized &&
		       "Trying to add already added test seat");
		test_seat_init(test);
	} else {
		assert(0 && "Unsupported device");
	}
}

enum weston_test_screenshot_outcome {
	WESTON_TEST_SCREENSHOT_SUCCESS,
	WESTON_TEST_SCREENSHOT_NO_MEMORY,
	WESTON_TEST_SCREENSHOT_BAD_BUFFER
	};

typedef void (*weston_test_screenshot_done_func_t)(void *data,
						   enum weston_test_screenshot_outcome outcome);

struct test_screenshot {
	struct weston_compositor *compositor;
	struct wl_global *global;
	struct wl_client *client;
	struct weston_process process;
	struct wl_listener destroy_listener;
};

struct test_screenshot_frame_listener {
	struct wl_listener listener;
	struct weston_buffer *buffer;
	weston_test_screenshot_done_func_t done;
	void *data;
};

static void
copy_bgra_yflip(uint8_t *dst, uint8_t *src, int height, int stride)
{
	uint8_t *end;

	end = dst + height * stride;
	while (dst < end) {
		memcpy(dst, src, stride);
		dst += stride;
		src -= stride;
	}
}


static void
copy_bgra(uint8_t *dst, uint8_t *src, int height, int stride)
{
	/* TODO: optimize this out */
	memcpy(dst, src, height * stride);
}

static void
copy_row_swap_RB(void *vdst, void *vsrc, int bytes)
{
	uint32_t *dst = vdst;
	uint32_t *src = vsrc;
	uint32_t *end = dst + bytes / 4;

	while (dst < end) {
		uint32_t v = *src++;
		/*                    A R G B */
		uint32_t tmp = v & 0xff00ff00;
		tmp |= (v >> 16) & 0x000000ff;
		tmp |= (v << 16) & 0x00ff0000;
		*dst++ = tmp;
	}
}

static void
copy_rgba_yflip(uint8_t *dst, uint8_t *src, int height, int stride)
{
	uint8_t *end;

	end = dst + height * stride;
	while (dst < end) {
		copy_row_swap_RB(dst, src, stride);
		dst += stride;
		src -= stride;
	}
}

static void
copy_rgba(uint8_t *dst, uint8_t *src, int height, int stride)
{
	uint8_t *end;

	end = dst + height * stride;
	while (dst < end) {
		copy_row_swap_RB(dst, src, stride);
		dst += stride;
		src += stride;
	}
}

static void
test_screenshot_frame_notify(struct wl_listener *listener, void *data)
{
	struct test_screenshot_frame_listener *l =
		container_of(listener,
			     struct test_screenshot_frame_listener, listener);
	struct weston_output *output = data;
	struct weston_compositor *compositor = output->compositor;
	int32_t stride;
	uint8_t *pixels, *d, *s;

	output->disable_planes--;
	wl_list_remove(&listener->link);
	stride = l->buffer->width * (PIXMAN_FORMAT_BPP(compositor->read_format) / 8);
	pixels = malloc(stride * l->buffer->height);

	if (pixels == NULL) {
		l->done(l->data, WESTON_TEST_SCREENSHOT_NO_MEMORY);
		free(l);
		return;
	}

	/* FIXME: Needs to handle output transformations */

	compositor->renderer->read_pixels(output,
					  compositor->read_format,
					  pixels,
					  0, 0,
					  output->current_mode->width,
					  output->current_mode->height);

	stride = wl_shm_buffer_get_stride(l->buffer->shm_buffer);

	d = wl_shm_buffer_get_data(l->buffer->shm_buffer);
	s = pixels + stride * (l->buffer->height - 1);

	wl_shm_buffer_begin_access(l->buffer->shm_buffer);

	/* XXX: It would be nice if we used Pixman to do all this rather
	 *  than our own implementation
	 */
	switch (compositor->read_format) {
	case PIXMAN_a8r8g8b8:
	case PIXMAN_x8r8g8b8:
		if (compositor->capabilities & WESTON_CAP_CAPTURE_YFLIP)
			copy_bgra_yflip(d, s, output->current_mode->height, stride);
		else
			copy_bgra(d, pixels, output->current_mode->height, stride);
		break;
	case PIXMAN_x8b8g8r8:
	case PIXMAN_a8b8g8r8:
		if (compositor->capabilities & WESTON_CAP_CAPTURE_YFLIP)
			copy_rgba_yflip(d, s, output->current_mode->height, stride);
		else
			copy_rgba(d, pixels, output->current_mode->height, stride);
		break;
	default:
		break;
	}

	wl_shm_buffer_end_access(l->buffer->shm_buffer);

	l->done(l->data, WESTON_TEST_SCREENSHOT_SUCCESS);
	free(pixels);
	free(l);
}

static bool
weston_test_screenshot_shoot(struct weston_output *output,
			     struct weston_buffer *buffer,
			     weston_test_screenshot_done_func_t done,
			     void *data)
{
	struct test_screenshot_frame_listener *l;

	/* Get the shm buffer resource the client created */
	if (!wl_shm_buffer_get(buffer->resource)) {
		done(data, WESTON_TEST_SCREENSHOT_BAD_BUFFER);
		return false;
	}

	buffer->shm_buffer = wl_shm_buffer_get(buffer->resource);
	buffer->width = wl_shm_buffer_get_width(buffer->shm_buffer);
	buffer->height = wl_shm_buffer_get_height(buffer->shm_buffer);

	/* Verify buffer is big enough */
	if (buffer->width < output->current_mode->width ||
		buffer->height < output->current_mode->height) {
		done(data, WESTON_TEST_SCREENSHOT_BAD_BUFFER);
		return false;
	}

	/* allocate the frame listener */
	l = malloc(sizeof *l);
	if (l == NULL) {
		done(data, WESTON_TEST_SCREENSHOT_NO_MEMORY);
		return false;
	}

	/* Set up the listener */
	l->buffer = buffer;
	l->done = done;
	l->data = data;
	l->listener.notify = test_screenshot_frame_notify;
	wl_signal_add(&output->frame_signal, &l->listener);

	/* Fire off a repaint */
	output->disable_planes++;
	weston_output_schedule_repaint(output);

	return true;
}

static void
capture_screenshot_done(void *data, enum weston_test_screenshot_outcome outcome)
{
	struct wl_resource *resource = data;

	switch (outcome) {
	case WESTON_TEST_SCREENSHOT_SUCCESS:
		weston_test_send_capture_screenshot_done(resource);
		break;
	case WESTON_TEST_SCREENSHOT_NO_MEMORY:
		wl_resource_post_no_memory(resource);
		break;
	default:
		break;
	}
}


/**
 * Grabs a snapshot of the screen.
 */
static void
capture_screenshot(struct wl_client *client,
		   struct wl_resource *resource,
		   struct wl_resource *output_resource,
		   struct wl_resource *buffer_resource)
{
	struct weston_output *output =
		weston_output_from_resource(output_resource);
	struct weston_buffer *buffer =
		weston_buffer_from_resource(buffer_resource);

	if (buffer == NULL) {
		wl_resource_post_no_memory(resource);
		return;
	}

	weston_test_screenshot_shoot(output, buffer,
				     capture_screenshot_done, resource);
}

static void
send_touch(struct wl_client *client, struct wl_resource *resource,
	   uint32_t tv_sec_hi, uint32_t tv_sec_lo, uint32_t tv_nsec,
	   int32_t touch_id, wl_fixed_t x, wl_fixed_t y, uint32_t touch_type)
{
	struct weston_test *test = wl_resource_get_user_data(resource);
	struct weston_seat *seat = get_seat(test);
	struct timespec time;

	timespec_from_proto(&time, tv_sec_hi, tv_sec_lo, tv_nsec);

	notify_touch(seat, &time, touch_id, wl_fixed_to_double(x),
		     wl_fixed_to_double(y), touch_type);
}

static const struct weston_test_interface test_implementation = {
	move_surface,
	move_pointer,
	send_button,
	send_axis,
	activate_surface,
	send_key,
	device_release,
	device_add,
	capture_screenshot,
	send_touch,
};

static void
bind_test(struct wl_client *client, void *data, uint32_t version, uint32_t id)
{
	struct weston_test *test = data;
	struct wl_resource *resource;

	resource = wl_resource_create(client, &weston_test_interface, 1, id);
	if (!resource) {
		wl_client_post_no_memory(client);
		return;
	}

	wl_resource_set_implementation(resource,
				       &test_implementation, test, NULL);

	notify_pointer_position(test, resource);
}

static void
idle_launch_client(void *data)
{
	struct weston_test *test = data;
	pid_t pid;
	sigset_t allsigs;
	char *path;

	path = getenv("WESTON_TEST_CLIENT_PATH");
	if (path == NULL)
		return;
	pid = fork();
	if (pid == -1)
		exit(EXIT_FAILURE);
	if (pid == 0) {
		sigfillset(&allsigs);
		sigprocmask(SIG_UNBLOCK, &allsigs, NULL);
		execl(path, path, NULL);
		weston_log("compositor: executing '%s' failed: %m\n", path);
		exit(EXIT_FAILURE);
	}

	test->process.pid = pid;
	test->process.cleanup = test_client_sigchld;
	weston_watch_process(&test->process);
}

WL_EXPORT int
wet_module_init(struct weston_compositor *ec,
		int *argc, char *argv[])
{
	struct weston_test *test;
	struct wl_event_loop *loop;

	test = zalloc(sizeof *test);
	if (test == NULL)
		return -1;

	test->compositor = ec;
	weston_layer_init(&test->layer, ec);
	weston_layer_set_position(&test->layer, WESTON_LAYER_POSITION_CURSOR - 1);

	if (wl_global_create(ec->wl_display, &weston_test_interface, 1,
			     test, bind_test) == NULL)
		return -1;

	if (test_seat_init(test) == -1)
		return -1;

	loop = wl_display_get_event_loop(ec->wl_display);
	wl_event_loop_add_idle(loop, idle_launch_client, test);

	return 0;
}
