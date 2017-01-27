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

#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/mman.h>
#include <cairo.h>

#include "shared/os-compatibility.h"
#include "shared/xalloc.h"
#include "shared/zalloc.h"
#include "weston-test-client-helper.h"

#define max(a, b) (((a) > (b)) ? (a) : (b))
#define min(a, b) (((a) > (b)) ? (b) : (a))
#define clip(x, a, b)  min(max(x, a), b)

int
surface_contains(struct surface *surface, int x, int y)
{
	/* test whether a global x,y point is contained in the surface */
	int sx = surface->x;
	int sy = surface->y;
	int sw = surface->width;
	int sh = surface->height;
	return x >= sx && y >= sy && x < sx + sw && y < sy + sh;
}

static void
frame_callback_handler(void *data, struct wl_callback *callback, uint32_t time)
{
	int *done = data;

	*done = 1;

	wl_callback_destroy(callback);
}

static const struct wl_callback_listener frame_listener = {
	frame_callback_handler
};

struct wl_callback *
frame_callback_set(struct wl_surface *surface, int *done)
{
	struct wl_callback *callback;

	*done = 0;
	callback = wl_surface_frame(surface);
	wl_callback_add_listener(callback, &frame_listener, done);

	return callback;
}

int
frame_callback_wait_nofail(struct client *client, int *done)
{
	while (!*done) {
		if (wl_display_dispatch(client->wl_display) < 0)
			return 0;
	}

	return 1;
}

void
move_client(struct client *client, int x, int y)
{
	struct surface *surface = client->surface;
	int done;

	client->surface->x = x;
	client->surface->y = y;
	weston_test_move_surface(client->test->weston_test, surface->wl_surface,
			     surface->x, surface->y);
	/* The attach here is necessary because commit() will call configure
	 * only on surfaces newly attached, and the one that sets the surface
	 * position is the configure. */
	wl_surface_attach(surface->wl_surface, surface->buffer->proxy, 0, 0);
	wl_surface_damage(surface->wl_surface, 0, 0, surface->width,
			  surface->height);

	frame_callback_set(surface->wl_surface, &done);

	wl_surface_commit(surface->wl_surface);

	frame_callback_wait(client, &done);
}

static void
pointer_handle_enter(void *data, struct wl_pointer *wl_pointer,
		     uint32_t serial, struct wl_surface *wl_surface,
		     wl_fixed_t x, wl_fixed_t y)
{
	struct pointer *pointer = data;

	if (wl_surface)
		pointer->focus = wl_surface_get_user_data(wl_surface);
	else
		pointer->focus = NULL;

	pointer->x = wl_fixed_to_int(x);
	pointer->y = wl_fixed_to_int(y);

	fprintf(stderr, "test-client: got pointer enter %d %d, surface %p\n",
		pointer->x, pointer->y, pointer->focus);
}

static void
pointer_handle_leave(void *data, struct wl_pointer *wl_pointer,
		     uint32_t serial, struct wl_surface *wl_surface)
{
	struct pointer *pointer = data;

	pointer->focus = NULL;

	fprintf(stderr, "test-client: got pointer leave, surface %p\n",
		wl_surface ? wl_surface_get_user_data(wl_surface) : NULL);
}

static void
pointer_handle_motion(void *data, struct wl_pointer *wl_pointer,
		      uint32_t time, wl_fixed_t x, wl_fixed_t y)
{
	struct pointer *pointer = data;

	pointer->x = wl_fixed_to_int(x);
	pointer->y = wl_fixed_to_int(y);

	fprintf(stderr, "test-client: got pointer motion %d %d\n",
		pointer->x, pointer->y);
}

static void
pointer_handle_button(void *data, struct wl_pointer *wl_pointer,
		      uint32_t serial, uint32_t time, uint32_t button,
		      uint32_t state)
{
	struct pointer *pointer = data;

	pointer->button = button;
	pointer->state = state;

	fprintf(stderr, "test-client: got pointer button %u %u\n",
		button, state);
}

static void
pointer_handle_axis(void *data, struct wl_pointer *wl_pointer,
		    uint32_t time, uint32_t axis, wl_fixed_t value)
{
	fprintf(stderr, "test-client: got pointer axis %u %f\n",
		axis, wl_fixed_to_double(value));
}

static void
pointer_handle_frame(void *data, struct wl_pointer *wl_pointer)
{
	fprintf(stderr, "test-client: got pointer frame\n");
}

static void
pointer_handle_axis_source(void *data, struct wl_pointer *wl_pointer,
			     uint32_t source)
{
	fprintf(stderr, "test-client: got pointer axis source %u\n", source);
}

static void
pointer_handle_axis_stop(void *data, struct wl_pointer *wl_pointer,
			 uint32_t time, uint32_t axis)
{
	fprintf(stderr, "test-client: got pointer axis stop\n");
}

static void
pointer_handle_axis_discrete(void *data, struct wl_pointer *wl_pointer,
			     uint32_t axis, int32_t value)
{
	fprintf(stderr, "test-client: got pointer axis discrete %u %d\n",
		axis, value);
}

static const struct wl_pointer_listener pointer_listener = {
	pointer_handle_enter,
	pointer_handle_leave,
	pointer_handle_motion,
	pointer_handle_button,
	pointer_handle_axis,
	pointer_handle_frame,
	pointer_handle_axis_source,
	pointer_handle_axis_stop,
	pointer_handle_axis_discrete,
};

static void
keyboard_handle_keymap(void *data, struct wl_keyboard *wl_keyboard,
		       uint32_t format, int fd, uint32_t size)
{
	close(fd);

	fprintf(stderr, "test-client: got keyboard keymap\n");
}

static void
keyboard_handle_enter(void *data, struct wl_keyboard *wl_keyboard,
		      uint32_t serial, struct wl_surface *wl_surface,
		      struct wl_array *keys)
{
	struct keyboard *keyboard = data;

	if (wl_surface)
		keyboard->focus = wl_surface_get_user_data(wl_surface);
	else
		keyboard->focus = NULL;

	fprintf(stderr, "test-client: got keyboard enter, surface %p\n",
		keyboard->focus);
}

static void
keyboard_handle_leave(void *data, struct wl_keyboard *wl_keyboard,
		      uint32_t serial, struct wl_surface *wl_surface)
{
	struct keyboard *keyboard = data;

	keyboard->focus = NULL;

	fprintf(stderr, "test-client: got keyboard leave, surface %p\n",
		wl_surface ? wl_surface_get_user_data(wl_surface) : NULL);
}

static void
keyboard_handle_key(void *data, struct wl_keyboard *wl_keyboard,
		    uint32_t serial, uint32_t time, uint32_t key,
		    uint32_t state)
{
	struct keyboard *keyboard = data;

	keyboard->key = key;
	keyboard->state = state;

	fprintf(stderr, "test-client: got keyboard key %u %u\n", key, state);
}

static void
keyboard_handle_modifiers(void *data, struct wl_keyboard *wl_keyboard,
			  uint32_t serial, uint32_t mods_depressed,
			  uint32_t mods_latched, uint32_t mods_locked,
			  uint32_t group)
{
	struct keyboard *keyboard = data;

	keyboard->mods_depressed = mods_depressed;
	keyboard->mods_latched = mods_latched;
	keyboard->mods_locked = mods_locked;
	keyboard->group = group;

	fprintf(stderr, "test-client: got keyboard modifiers %u %u %u %u\n",
		mods_depressed, mods_latched, mods_locked, group);
}

static void
keyboard_handle_repeat_info(void *data, struct wl_keyboard *wl_keyboard,
			    int32_t rate, int32_t delay)
{
	struct keyboard *keyboard = data;

	keyboard->repeat_info.rate = rate;
	keyboard->repeat_info.delay = delay;

	fprintf(stderr, "test-client: got keyboard repeat_info %d %d\n",
		rate, delay);
}

static const struct wl_keyboard_listener keyboard_listener = {
	keyboard_handle_keymap,
	keyboard_handle_enter,
	keyboard_handle_leave,
	keyboard_handle_key,
	keyboard_handle_modifiers,
	keyboard_handle_repeat_info,
};

static void
touch_handle_down(void *data, struct wl_touch *wl_touch,
		  uint32_t serial, uint32_t time, struct wl_surface *surface,
		  int32_t id, wl_fixed_t x_w, wl_fixed_t y_w)
{
	struct touch *touch = data;

	touch->down_x = wl_fixed_to_int(x_w);
	touch->down_y = wl_fixed_to_int(y_w);
	touch->id = id;

	fprintf(stderr, "test-client: got touch down %d %d, surf: %p, id: %d\n",
		touch->down_x, touch->down_y, surface, id);
}

static void
touch_handle_up(void *data, struct wl_touch *wl_touch,
		uint32_t serial, uint32_t time, int32_t id)
{
	struct touch *touch = data;
	touch->up_id = id;

	fprintf(stderr, "test-client: got touch up, id: %d\n", id);
}

static void
touch_handle_motion(void *data, struct wl_touch *wl_touch,
		    uint32_t time, int32_t id, wl_fixed_t x_w, wl_fixed_t y_w)
{
	struct touch *touch = data;
	touch->x = wl_fixed_to_int(x_w);
	touch->y = wl_fixed_to_int(y_w);

	fprintf(stderr, "test-client: got touch motion, %d %d, id: %d\n",
		touch->x, touch->y, id);
}

static void
touch_handle_frame(void *data, struct wl_touch *wl_touch)
{
	struct touch *touch = data;

	++touch->frame_no;

	fprintf(stderr, "test-client: got touch frame (%d)\n", touch->frame_no);
}

static void
touch_handle_cancel(void *data, struct wl_touch *wl_touch)
{
	struct touch *touch = data;

	++touch->cancel_no;

	fprintf(stderr, "test-client: got touch cancel (%d)\n", touch->cancel_no);
}

static const struct wl_touch_listener touch_listener = {
	touch_handle_down,
	touch_handle_up,
	touch_handle_motion,
	touch_handle_frame,
	touch_handle_cancel,
};

static void
surface_enter(void *data,
	      struct wl_surface *wl_surface, struct wl_output *output)
{
	struct surface *surface = data;

	surface->output = wl_output_get_user_data(output);

	fprintf(stderr, "test-client: got surface enter output %p\n",
		surface->output);
}

static void
surface_leave(void *data,
	      struct wl_surface *wl_surface, struct wl_output *output)
{
	struct surface *surface = data;

	surface->output = NULL;

	fprintf(stderr, "test-client: got surface leave output %p\n",
		wl_output_get_user_data(output));
}

static const struct wl_surface_listener surface_listener = {
	surface_enter,
	surface_leave
};

static struct buffer *
create_shm_buffer(struct client *client, int width, int height,
		  pixman_format_code_t format, uint32_t wlfmt)
{
	struct wl_shm *shm = client->wl_shm;
	struct buffer *buf;
	size_t stride_bytes;
	struct wl_shm_pool *pool;
	int fd;
	void *data;
	size_t bytes_pp;

	assert(width > 0);
	assert(height > 0);

	buf = xzalloc(sizeof *buf);

	bytes_pp = PIXMAN_FORMAT_BPP(format) / 8;
	stride_bytes = width * bytes_pp;
	/* round up to multiple of 4 bytes for Pixman */
	stride_bytes = (stride_bytes + 3) & ~3u;
	assert(stride_bytes / bytes_pp >= (unsigned)width);

	buf->len = stride_bytes * height;
	assert(buf->len / stride_bytes == (unsigned)height);

	fd = os_create_anonymous_file(buf->len);
	assert(fd >= 0);

	data = mmap(NULL, buf->len, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	if (data == MAP_FAILED) {
		close(fd);
		assert(data != MAP_FAILED);
	}

	pool = wl_shm_create_pool(shm, fd, buf->len);
	buf->proxy = wl_shm_pool_create_buffer(pool, 0, width, height,
					       stride_bytes, wlfmt);
	wl_shm_pool_destroy(pool);
	close(fd);

	buf->image = pixman_image_create_bits(format, width, height,
					      data, stride_bytes);

	assert(buf->proxy);
	assert(buf->image);

	return buf;
}

struct buffer *
create_shm_buffer_a8r8g8b8(struct client *client, int width, int height)
{
	assert(client->has_argb);

	return create_shm_buffer(client, width, height,
				 PIXMAN_a8r8g8b8, WL_SHM_FORMAT_ARGB8888);
}

void
buffer_destroy(struct buffer *buf)
{
	void *pixels;

	pixels = pixman_image_get_data(buf->image);

	if (buf->proxy) {
		wl_buffer_destroy(buf->proxy);
		assert(munmap(pixels, buf->len) == 0);
	}

	assert(pixman_image_unref(buf->image));

	free(buf);
}

static void
shm_format(void *data, struct wl_shm *wl_shm, uint32_t format)
{
	struct client *client = data;

	if (format == WL_SHM_FORMAT_ARGB8888)
		client->has_argb = 1;
}

struct wl_shm_listener shm_listener = {
	shm_format
};

static void
test_handle_pointer_position(void *data, struct weston_test *weston_test,
			     wl_fixed_t x, wl_fixed_t y)
{
	struct test *test = data;
	test->pointer_x = wl_fixed_to_int(x);
	test->pointer_y = wl_fixed_to_int(y);

	fprintf(stderr, "test-client: got global pointer %d %d\n",
		test->pointer_x, test->pointer_y);
}

static void
test_handle_capture_screenshot_done(void *data, struct weston_test *weston_test)
{
	struct test *test = data;

	printf("Screenshot has been captured\n");
	test->buffer_copy_done = 1;
}

static const struct weston_test_listener test_listener = {
	test_handle_pointer_position,
	test_handle_capture_screenshot_done,
};

static void
input_update_devices(struct input *input)
{
	struct pointer *pointer;
	struct keyboard *keyboard;
	struct touch *touch;

	struct wl_seat *seat = input->wl_seat;
	enum wl_seat_capability caps = input->caps;

	if ((caps & WL_SEAT_CAPABILITY_POINTER) && !input->pointer) {
		pointer = xzalloc(sizeof *pointer);
		pointer->wl_pointer = wl_seat_get_pointer(seat);
		wl_pointer_set_user_data(pointer->wl_pointer, pointer);
		wl_pointer_add_listener(pointer->wl_pointer, &pointer_listener,
					pointer);
		input->pointer = pointer;
	} else if (!(caps & WL_SEAT_CAPABILITY_POINTER) && input->pointer) {
		wl_pointer_destroy(input->pointer->wl_pointer);
		free(input->pointer);
		input->pointer = NULL;
	}

	if ((caps & WL_SEAT_CAPABILITY_KEYBOARD) && !input->keyboard) {
		keyboard = xzalloc(sizeof *keyboard);
		keyboard->wl_keyboard = wl_seat_get_keyboard(seat);
		wl_keyboard_set_user_data(keyboard->wl_keyboard, keyboard);
		wl_keyboard_add_listener(keyboard->wl_keyboard, &keyboard_listener,
					 keyboard);
		input->keyboard = keyboard;
	} else if (!(caps & WL_SEAT_CAPABILITY_KEYBOARD) && input->keyboard) {
		wl_keyboard_destroy(input->keyboard->wl_keyboard);
		free(input->keyboard);
		input->keyboard = NULL;
	}

	if ((caps & WL_SEAT_CAPABILITY_TOUCH) && !input->touch) {
		touch = xzalloc(sizeof *touch);
		touch->wl_touch = wl_seat_get_touch(seat);
		wl_touch_set_user_data(touch->wl_touch, touch);
		wl_touch_add_listener(touch->wl_touch, &touch_listener,
					 touch);
		input->touch = touch;
	} else if (!(caps & WL_SEAT_CAPABILITY_TOUCH) && input->touch) {
		wl_touch_destroy(input->touch->wl_touch);
		free(input->touch);
		input->touch = NULL;
	}
}

static void
seat_handle_capabilities(void *data, struct wl_seat *seat,
			 enum wl_seat_capability caps)
{
	struct input *input = data;

	input->caps = caps;

	/* we will create/update the devices only with the right (test) seat.
	 * If we haven't discovered which seat is the test seat, just
	 * store capabilities and bail out */
	if (input->seat_name && strcmp(input->seat_name, "test-seat") == 0)
		input_update_devices(input);

	fprintf(stderr, "test-client: got seat %p capabilities: %x\n",
		input, caps);
}

static void
seat_handle_name(void *data, struct wl_seat *seat, const char *name)
{
	struct input *input = data;

	input->seat_name = strdup(name);
	assert(input->seat_name && "No memory");

	fprintf(stderr, "test-client: got seat %p name: \'%s\'\n",
		input, name);
}

static const struct wl_seat_listener seat_listener = {
	seat_handle_capabilities,
	seat_handle_name,
};

static void
output_handle_geometry(void *data,
		       struct wl_output *wl_output,
		       int x, int y,
		       int physical_width,
		       int physical_height,
		       int subpixel,
		       const char *make,
		       const char *model,
		       int32_t transform)
{
	struct output *output = data;

	output->x = x;
	output->y = y;
}

static void
output_handle_mode(void *data,
		   struct wl_output *wl_output,
		   uint32_t flags,
		   int width,
		   int height,
		   int refresh)
{
	struct output *output = data;

	if (flags & WL_OUTPUT_MODE_CURRENT) {
		output->width = width;
		output->height = height;
	}
}

static void
output_handle_scale(void *data,
		    struct wl_output *wl_output,
		    int scale)
{
	struct output *output = data;

	output->scale = scale;
}

static void
output_handle_done(void *data,
		   struct wl_output *wl_output)
{
	struct output *output = data;

	output->initialized = 1;
}

static const struct wl_output_listener output_listener = {
	output_handle_geometry,
	output_handle_mode,
	output_handle_done,
	output_handle_scale,
};

static void
handle_global(void *data, struct wl_registry *registry,
	      uint32_t id, const char *interface, uint32_t version)
{
	struct client *client = data;
	struct output *output;
	struct test *test;
	struct global *global;
	struct input *input;

	global = xzalloc(sizeof *global);
	global->name = id;
	global->interface = strdup(interface);
	assert(interface);
	global->version = version;
	wl_list_insert(client->global_list.prev, &global->link);

	if (strcmp(interface, "wl_compositor") == 0) {
		client->wl_compositor =
			wl_registry_bind(registry, id,
					 &wl_compositor_interface, version);
	} else if (strcmp(interface, "wl_seat") == 0) {
		input = xzalloc(sizeof *input);
		input->wl_seat =
			wl_registry_bind(registry, id,
					 &wl_seat_interface, version);
		wl_seat_add_listener(input->wl_seat, &seat_listener, input);
		wl_list_insert(&client->inputs, &input->link);
	} else if (strcmp(interface, "wl_shm") == 0) {
		client->wl_shm =
			wl_registry_bind(registry, id,
					 &wl_shm_interface, version);
		wl_shm_add_listener(client->wl_shm, &shm_listener, client);
	} else if (strcmp(interface, "wl_output") == 0) {
		output = xzalloc(sizeof *output);
		output->wl_output =
			wl_registry_bind(registry, id,
					 &wl_output_interface, version);
		wl_output_add_listener(output->wl_output,
				       &output_listener, output);
		client->output = output;
	} else if (strcmp(interface, "weston_test") == 0) {
		test = xzalloc(sizeof *test);
		test->weston_test =
			wl_registry_bind(registry, id,
					 &weston_test_interface, version);
		weston_test_add_listener(test->weston_test, &test_listener, test);
		client->test = test;
	} else if (strcmp(interface, "wl_drm") == 0) {
		client->has_wl_drm = true;
	}
}

static const struct wl_registry_listener registry_listener = {
	handle_global
};

void
skip(const char *fmt, ...)
{
	va_list argp;

	va_start(argp, fmt);
	vfprintf(stderr, fmt, argp);
	va_end(argp);

	/* automake tests uses exit code 77. weston-test-runner will see
	 * this and use it, and then weston-test's sigchld handler (in the
	 * weston process) will use that as an exit status, which is what
	 * automake will see in the end. */
	exit(77);
}

void
expect_protocol_error(struct client *client,
		      const struct wl_interface *intf,
		      uint32_t code)
{
	int err;
	uint32_t errcode, failed = 0;
	const struct wl_interface *interface;
	unsigned int id;

	/* if the error has not come yet, make it happen */
	wl_display_roundtrip(client->wl_display);

	err = wl_display_get_error(client->wl_display);

	assert(err && "Expected protocol error but nothing came");
	assert(err == EPROTO && "Expected protocol error but got local error");

	errcode = wl_display_get_protocol_error(client->wl_display,
						&interface, &id);

	/* check error */
	if (errcode != code) {
		fprintf(stderr, "Should get error code %d but got %d\n",
			code, errcode);
		failed = 1;
	}

	/* this should be definitely set */
	assert(interface);

	if (strcmp(intf->name, interface->name) != 0) {
		fprintf(stderr, "Should get interface '%s' but got '%s'\n",
			intf->name, interface->name);
		failed = 1;
	}

	if (failed) {
		fprintf(stderr, "Expected other protocol error\n");
		abort();
	}

	/* all OK */
	fprintf(stderr, "Got expected protocol error on '%s' (object id: %d) "
			"with code %d\n", interface->name, id, errcode);
}

static void
log_handler(const char *fmt, va_list args)
{
	fprintf(stderr, "libwayland: ");
	vfprintf(stderr, fmt, args);
}

static void
input_destroy(struct input *inp)
{
	wl_list_remove(&inp->link);
	wl_seat_destroy(inp->wl_seat);
	free(inp);
}

/* find the test-seat and set it in client.
 * Destroy other inputs */
static void
client_set_input(struct client *cl)
{
	struct input *inp, *inptmp;
	wl_list_for_each_safe(inp, inptmp, &cl->inputs, link) {
		assert(inp->seat_name && "BUG: input with no name");
		if (strcmp(inp->seat_name, "test-seat") == 0) {
			cl->input = inp;
			input_update_devices(inp);
		} else {
			input_destroy(inp);
		}
	}

	/* we keep only one input */
	assert(wl_list_length(&cl->inputs) == 1);
}

struct client *
create_client(void)
{
	struct client *client;

	wl_log_set_handler_client(log_handler);

	/* connect to display */
	client = xzalloc(sizeof *client);
	client->wl_display = wl_display_connect(NULL);
	assert(client->wl_display);
	wl_list_init(&client->global_list);
	wl_list_init(&client->inputs);

	/* setup registry so we can bind to interfaces */
	client->wl_registry = wl_display_get_registry(client->wl_display);
	wl_registry_add_listener(client->wl_registry, &registry_listener,
				 client);

	/* this roundtrip makes sure we have all globals and we bound to them */
	client_roundtrip(client);
	/* this roundtrip makes sure we got all wl_shm.format and wl_seat.*
	 * events */
	client_roundtrip(client);

	/* find the right input for us */
	client_set_input(client);

	/* must have WL_SHM_FORMAT_ARGB32 */
	assert(client->has_argb);

	/* must have weston_test interface */
	assert(client->test);

	/* must have an output */
	assert(client->output);

	/* the output must be initialized */
	assert(client->output->initialized == 1);

	/* must have seat set */
	assert(client->input);

	return client;
}

struct client *
create_client_and_test_surface(int x, int y, int width, int height)
{
	struct client *client;
	struct surface *surface;
	pixman_color_t color = { 16384, 16384, 16384, 16384 }; /* uint16_t */
	pixman_image_t *solid;

	client = create_client();

	/* initialize the client surface */
	surface = xzalloc(sizeof *surface);
	surface->wl_surface =
		wl_compositor_create_surface(client->wl_compositor);
	assert(surface->wl_surface);

	wl_surface_add_listener(surface->wl_surface, &surface_listener,
				surface);

	client->surface = surface;
	wl_surface_set_user_data(surface->wl_surface, surface);

	surface->width = width;
	surface->height = height;
	surface->buffer = create_shm_buffer_a8r8g8b8(client, width, height);

	solid = pixman_image_create_solid_fill(&color);
	pixman_image_composite32(PIXMAN_OP_SRC,
				 solid, /* src */
				 NULL, /* mask */
				 surface->buffer->image, /* dst */
				 0, 0, /* src x,y */
				 0, 0, /* mask x,y */
				 0, 0, /* dst x,y */
				 width, height);
	pixman_image_unref(solid);

	move_client(client, x, y);

	return client;
}

static const char*
output_path(void)
{
	char *path = getenv("WESTON_TEST_OUTPUT_PATH");

	if (!path)
		return "./logs";

	return path;
}

char*
screenshot_output_filename(const char *basename, uint32_t seq)
{
	char *filename;

	if (asprintf(&filename, "%s/%s-%02d.png",
				 output_path(), basename, seq) < 0)
		return NULL;
	return filename;
}

static const char*
reference_path(void)
{
	char *path = getenv("WESTON_TEST_REFERENCE_PATH");

	if (!path)
		return "./tests/reference";
	return path;
}

char*
screenshot_reference_filename(const char *basename, uint32_t seq)
{
	char *filename;

	if (asprintf(&filename, "%s/%s-%02d.png",
				 reference_path(), basename, seq) < 0)
		return NULL;
	return filename;
}

struct format_map_entry {
	cairo_format_t cairo;
	pixman_format_code_t pixman;
};

static const struct format_map_entry format_map[] = {
	{ CAIRO_FORMAT_ARGB32,    PIXMAN_a8r8g8b8 },
	{ CAIRO_FORMAT_RGB24,     PIXMAN_x8r8g8b8 },
	{ CAIRO_FORMAT_A8,        PIXMAN_a8 },
	{ CAIRO_FORMAT_RGB16_565, PIXMAN_r5g6b5 },
};

static pixman_format_code_t
format_cairo2pixman(cairo_format_t fmt)
{
	unsigned i;

	for (i = 0; i < ARRAY_LENGTH(format_map); i++)
		if (format_map[i].cairo == fmt)
			return format_map[i].pixman;

	assert(0 && "unknown Cairo pixel format");
}

static cairo_format_t
format_pixman2cairo(pixman_format_code_t fmt)
{
	unsigned i;

	for (i = 0; i < ARRAY_LENGTH(format_map); i++)
		if (format_map[i].pixman == fmt)
			return format_map[i].cairo;

	assert(0 && "unknown Pixman pixel format");
}

/**
 * Compute the ROI for image comparisons
 *
 * \param img_a An image.
 * \param img_b Another image.
 * \param clip_rect Explicit ROI, or NULL for using the whole
 * image area.
 *
 * \return The region of interest (ROI) that is guaranteed to be inside both
 * images.
 *
 * If clip_rect is given, it must fall inside of both images.
 * If clip_rect is NULL, the images must be of the same size.
 * If any precondition is violated, this function aborts with an error.
 *
 * The ROI is given as pixman_box32_t, where x2,y2 are non-inclusive.
 */
static pixman_box32_t
image_check_get_roi(pixman_image_t *img_a, pixman_image_t *img_b,
		   const struct rectangle *clip_rect)
{
	int width_a;
	int width_b;
	int height_a;
	int height_b;
	pixman_box32_t box;

	width_a = pixman_image_get_width(img_a);
	height_a = pixman_image_get_height(img_a);

	width_b = pixman_image_get_width(img_b);
	height_b = pixman_image_get_height(img_b);

	if (clip_rect) {
		box.x1 = clip_rect->x;
		box.y1 = clip_rect->y;
		box.x2 = clip_rect->x + clip_rect->width;
		box.y2 = clip_rect->y + clip_rect->height;
	} else {
		box.x1 = 0;
		box.y1 = 0;
		box.x2 = max(width_a, width_b);
		box.y2 = max(height_a, height_b);
	}

	assert(box.x1 >= 0);
	assert(box.y1 >= 0);
	assert(box.x2 > box.x1);
	assert(box.y2 > box.y1);
	assert(box.x2 <= width_a);
	assert(box.x2 <= width_b);
	assert(box.y2 <= height_a);
	assert(box.y2 <= height_b);

	return box;
}

struct image_iterator {
	char *data;
	int stride; /* bytes */
};

static void
image_iter_init(struct image_iterator *it, pixman_image_t *image)
{
	pixman_format_code_t fmt;

	it->stride = pixman_image_get_stride(image);
	it->data = (void *)pixman_image_get_data(image);

	fmt = pixman_image_get_format(image);
	assert(PIXMAN_FORMAT_BPP(fmt) == 32);
}

static uint32_t *
image_iter_get_row(struct image_iterator *it, int y)
{
	return (uint32_t *)(it->data + y * it->stride);
}

/**
 * Test if a given region within two images are pixel-identical.
 *
 * Returns true if the two images pixel-wise identical, and false otherwise.
 *
 * \param img_a First image.
 * \param img_b Second image.
 * \param clip_rect The region of interest, or NULL for comparing the whole
 * images.
 *
 * This function hard-fails if clip_rect is not inside both images. If clip_rect
 * is given, the images do not have to match in size, otherwise size mismatch
 * will be a hard failure.
 */
bool
check_images_match(pixman_image_t *img_a, pixman_image_t *img_b,
		   const struct rectangle *clip_rect)
{
	struct image_iterator it_a;
	struct image_iterator it_b;
	pixman_box32_t box;
	int x, y;
	uint32_t *pix_a;
	uint32_t *pix_b;

	box = image_check_get_roi(img_a, img_b, clip_rect);

	image_iter_init(&it_a, img_a);
	image_iter_init(&it_b, img_b);

	for (y = box.y1; y < box.y2; y++) {
		pix_a = image_iter_get_row(&it_a, y) + box.x1;
		pix_b = image_iter_get_row(&it_b, y) + box.x1;

		for (x = box.x1; x < box.x2; x++) {
			if (*pix_a != *pix_b)
				return false;

			pix_a++;
			pix_b++;
		}
	}

	return true;
}

/**
 * Tint a color
 *
 * \param src Source pixel as x8r8g8b8.
 * \param add The tint as x8r8g8b8, x8 must be zero; r8, g8 and b8 must be
 * no greater than 0xc0 to avoid overflow to another channel.
 * \return The tinted pixel color as x8r8g8b8, x8 guaranteed to be 0xff.
 *
 * The source pixel RGB values are divided by 4, and then the tint is added.
 * To achieve colors outside of the range of src, a tint color channel must be
 * at least 0x40. (0xff / 4 = 0x3f, 0xff - 0x3f = 0xc0)
 */
static uint32_t
tint(uint32_t src, uint32_t add)
{
	uint32_t v;

	v = ((src & 0xfcfcfcfc) >> 2) | 0xff000000;

	return v + add;
}

/**
 * Create a visualization of image differences.
 *
 * \param img_a First image, which is used as the basis for the output.
 * \param img_b Second image.
 * \param clip_rect The region of interest, or NULL for comparing the whole
 * images.
 * \return A new image with the differences highlighted.
 *
 * Regions outside of the region of interest are shaded with black, matching
 * pixels are shaded with green, and differing pixels are shaded with
 * bright red.
 *
 * This function hard-fails if clip_rect is not inside both images. If clip_rect
 * is given, the images do not have to match in size, otherwise size mismatch
 * will be a hard failure.
 */
pixman_image_t *
visualize_image_difference(pixman_image_t *img_a, pixman_image_t *img_b,
			   const struct rectangle *clip_rect)
{
	pixman_image_t *diffimg;
	pixman_image_t *shade;
	struct image_iterator it_a;
	struct image_iterator it_b;
	struct image_iterator it_d;
	int width;
	int height;
	pixman_box32_t box;
	int x, y;
	uint32_t *pix_a;
	uint32_t *pix_b;
	uint32_t *pix_d;
	pixman_color_t shade_color = { 0, 0, 0, 32768 };

	width = pixman_image_get_width(img_a);
	height = pixman_image_get_height(img_a);
	box = image_check_get_roi(img_a, img_b, clip_rect);

	diffimg = pixman_image_create_bits_no_clear(PIXMAN_x8r8g8b8,
						    width, height, NULL, 0);

	/* Fill diffimg with a black-shaded copy of img_a, and then fill
	 * the clip_rect area with original img_a.
	 */
	shade = pixman_image_create_solid_fill(&shade_color);
	pixman_image_composite32(PIXMAN_OP_SRC, img_a, shade, diffimg,
				 0, 0, 0, 0, 0, 0, width, height);
	pixman_image_unref(shade);
	pixman_image_composite32(PIXMAN_OP_SRC, img_a, NULL, diffimg,
				 box.x1, box.y1, 0, 0, box.x1, box.y1,
				 box.x2 - box.x1, box.y2 - box.y1);

	image_iter_init(&it_a, img_a);
	image_iter_init(&it_b, img_b);
	image_iter_init(&it_d, diffimg);

	for (y = box.y1; y < box.y2; y++) {
		pix_a = image_iter_get_row(&it_a, y) + box.x1;
		pix_b = image_iter_get_row(&it_b, y) + box.x1;
		pix_d = image_iter_get_row(&it_d, y) + box.x1;

		for (x = box.x1; x < box.x2; x++) {
			if (*pix_a == *pix_b)
				*pix_d = tint(*pix_d, 0x00008000); /* green */
			else
				*pix_d = tint(*pix_d, 0x00c00000); /* red */

			pix_a++;
			pix_b++;
			pix_d++;
		}
	}

	return diffimg;
}

/**
 * Write an image into a PNG file.
 *
 * \param image The image.
 * \param fname The name and path for the file.
 *
 * \returns true if successfully saved file; false otherwise.
 *
 * \note Only image formats directly supported by Cairo are accepted, not all
 * Pixman formats.
 */
bool
write_image_as_png(pixman_image_t *image, const char *fname)
{
	cairo_surface_t *cairo_surface;
	cairo_status_t status;
	cairo_format_t fmt;

	fmt = format_pixman2cairo(pixman_image_get_format(image));

	cairo_surface = cairo_image_surface_create_for_data(
			(void *)pixman_image_get_data(image),
			fmt,
			pixman_image_get_width(image),
			pixman_image_get_height(image),
			pixman_image_get_stride(image));

	status = cairo_surface_write_to_png(cairo_surface, fname);
	if (status != CAIRO_STATUS_SUCCESS) {
		fprintf(stderr, "Failed to save image '%s': %s\n", fname,
			cairo_status_to_string(status));

		return false;
	}

	cairo_surface_destroy(cairo_surface);

	return true;
}

static pixman_image_t *
image_convert_to_a8r8g8b8(pixman_image_t *image)
{
	pixman_image_t *ret;
	int width;
	int height;

	if (pixman_image_get_format(image) == PIXMAN_a8r8g8b8)
		return pixman_image_ref(image);

	width = pixman_image_get_width(image);
	height = pixman_image_get_height(image);

	ret = pixman_image_create_bits_no_clear(PIXMAN_a8r8g8b8, width, height,
						NULL, 0);
	assert(ret);

	pixman_image_composite32(PIXMAN_OP_SRC, image, NULL, ret,
				 0, 0, 0, 0, 0, 0, width, height);

	return ret;
}

static void
destroy_cairo_surface(pixman_image_t *image, void *data)
{
	cairo_surface_t *surface = data;

	cairo_surface_destroy(surface);
}

/**
 * Load an image from a PNG file
 *
 * Reads a PNG image from disk using the given filename (and path)
 * and returns as a Pixman image. Use pixman_image_unref() to free it.
 *
 * The returned image is always in PIXMAN_a8r8g8b8 format.
 *
 * @returns Pixman image, or NULL in case of error.
 */
pixman_image_t *
load_image_from_png(const char *fname)
{
	pixman_image_t *image;
	pixman_image_t *converted;
	cairo_format_t cairo_fmt;
	pixman_format_code_t pixman_fmt;
	cairo_surface_t *reference_cairo_surface;
	cairo_status_t status;
	int width;
	int height;
	int stride;
	void *data;

	reference_cairo_surface = cairo_image_surface_create_from_png(fname);
	cairo_surface_flush(reference_cairo_surface);
	status = cairo_surface_status(reference_cairo_surface);
	if (status != CAIRO_STATUS_SUCCESS) {
		printf("Could not open %s: %s\n", fname, cairo_status_to_string(status));
		cairo_surface_destroy(reference_cairo_surface);
		return NULL;
	}

	cairo_fmt = cairo_image_surface_get_format(reference_cairo_surface);
	pixman_fmt = format_cairo2pixman(cairo_fmt);

	width = cairo_image_surface_get_width(reference_cairo_surface);
	height = cairo_image_surface_get_height(reference_cairo_surface);
	stride = cairo_image_surface_get_stride(reference_cairo_surface);
	data = cairo_image_surface_get_data(reference_cairo_surface);

	/* The Cairo surface will own the data, so we keep it around. */
	image = pixman_image_create_bits_no_clear(pixman_fmt,
						  width, height, data, stride);
	assert(image);

	pixman_image_set_destroy_function(image, destroy_cairo_surface,
					  reference_cairo_surface);

	converted = image_convert_to_a8r8g8b8(image);
	pixman_image_unref(image);

	return converted;
}

/**
 * Take screenshot of a single output
 *
 * Requests a screenshot from the server of the output that the
 * client appears on. This implies that the compositor goes through an output
 * repaint to provide the screenshot before this function returns. This
 * function is therefore both a server roundtrip and a wait for a repaint.
 *
 * @returns A new buffer object, that should be freed with buffer_destroy().
 */
struct buffer *
capture_screenshot_of_output(struct client *client)
{
	struct buffer *buffer;

	buffer = create_shm_buffer_a8r8g8b8(client,
					    client->output->width,
					    client->output->height);

	client->test->buffer_copy_done = 0;
	weston_test_capture_screenshot(client->test->weston_test,
				       client->output->wl_output,
				       buffer->proxy);
	while (client->test->buffer_copy_done == 0)
		if (wl_display_dispatch(client->wl_display) < 0)
			break;

	/* FIXME: Document somewhere the orientation the screenshot is taken
	 * and how the clip coords are interpreted, in case of scaling/transform.
	 * If we're using read_pixels() just make sure it is documented somewhere.
	 * Protocol docs in the XML, comparison function docs in Doxygen style.
	 */

	return buffer;
}
