/*
 * Copyright © 2012 Intel Corporation
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that copyright
 * notice and this permission notice appear in supporting documentation, and
 * that the name of the copyright holders not be used in advertising or
 * publicity pertaining to distribution of the software without specific,
 * written prior permission.  The copyright holders make no representations
 * about the suitability of this software for any purpose.  It is provided "as
 * is" without express or implied warranty.
 *
 * THE COPYRIGHT HOLDERS DISCLAIM ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL THE COPYRIGHT HOLDERS BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE
 * OF THIS SOFTWARE.
 */

#include <config.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>

#include "../shared/os-compatibility.h"
#include "weston-test-client-helper.h"

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

void
frame_callback_wait(struct client *client, int *done)
{
	while (!*done) {
		assert(wl_display_dispatch(client->wl_display) >= 0);
	}
}

void
move_client(struct client *client, int x, int y)
{
	struct surface *surface = client->surface;
	int done;

	client->surface->x = x;
	client->surface->y = y;
	wl_test_move_surface(client->test->wl_test, surface->wl_surface,
			     surface->x, surface->y);
	/* The attach here is necessary because commit() will call congfigure
	 * only on surfaces newly attached, and the one that sets the surface
	 * position is the configure. */
	wl_surface_attach(surface->wl_surface, surface->wl_buffer, 0, 0);
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

	pointer->focus = wl_surface_get_user_data(wl_surface);
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
		wl_surface_get_user_data(wl_surface));
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
	fprintf(stderr, "test-client: got pointer axis %u %d\n", axis, value);
}

static const struct wl_pointer_listener pointer_listener = {
	pointer_handle_enter,
	pointer_handle_leave,
	pointer_handle_motion,
	pointer_handle_button,
	pointer_handle_axis,
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

	keyboard->focus = wl_surface_get_user_data(wl_surface);

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
		wl_surface_get_user_data(wl_surface));
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

static const struct wl_keyboard_listener keyboard_listener = {
	keyboard_handle_keymap,
	keyboard_handle_enter,
	keyboard_handle_leave,
	keyboard_handle_key,
	keyboard_handle_modifiers,
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

struct wl_buffer *
create_shm_buffer(struct client *client, int width, int height, void **pixels)
{
	struct wl_shm *shm = client->wl_shm;
	int stride = width * 4;
	int size = stride * height;
	struct wl_shm_pool *pool;
	struct wl_buffer *buffer;
	int fd;
	void *data;

	fd = os_create_anonymous_file(size);
	assert(fd >= 0);

	data = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	if (data == MAP_FAILED) {
		close(fd);
		assert(data != MAP_FAILED);
	}

	pool = wl_shm_create_pool(shm, fd, size);
	buffer = wl_shm_pool_create_buffer(pool, 0, width, height, stride,
					   WL_SHM_FORMAT_ARGB8888);
	wl_shm_pool_destroy(pool);

	close(fd);

	if (pixels)
		*pixels = data;

	return buffer;
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
test_handle_pointer_position(void *data, struct wl_test *wl_test,
			     wl_fixed_t x, wl_fixed_t y)
{
	struct test *test = data;
	test->pointer_x = wl_fixed_to_int(x);
	test->pointer_y = wl_fixed_to_int(y);

	fprintf(stderr, "test-client: got global pointer %d %d\n",
		test->pointer_x, test->pointer_y);
}

static const struct wl_test_listener test_listener = {
	test_handle_pointer_position
};

static void
seat_handle_capabilities(void *data, struct wl_seat *seat,
			 enum wl_seat_capability caps)
{
	struct input *input = data;
	struct pointer *pointer;
	struct keyboard *keyboard;

	if ((caps & WL_SEAT_CAPABILITY_POINTER) && !input->pointer) {
		pointer = calloc(1, sizeof *pointer);
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
		keyboard = calloc(1, sizeof *keyboard);
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
}

static const struct wl_seat_listener seat_listener = {
	seat_handle_capabilities,
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

static const struct wl_output_listener output_listener = {
	output_handle_geometry,
	output_handle_mode
};

static void
handle_global(void *data, struct wl_registry *registry,
	      uint32_t id, const char *interface, uint32_t version)
{
	struct client *client = data;
	struct input *input;
	struct output *output;
	struct test *test;
	struct global *global;

	global = malloc(sizeof *global);
	assert(global);
	global->name = id;
	global->interface = strdup(interface);
	assert(interface);
	global->version = version;
	wl_list_insert(client->global_list.prev, &global->link);

	if (strcmp(interface, "wl_compositor") == 0) {
		client->wl_compositor =
			wl_registry_bind(registry, id,
					 &wl_compositor_interface, 1);
	} else if (strcmp(interface, "wl_seat") == 0) {
		input = calloc(1, sizeof *input);
		input->wl_seat =
			wl_registry_bind(registry, id,
					 &wl_seat_interface, 1);
		wl_seat_add_listener(input->wl_seat, &seat_listener, input);
		client->input = input;
	} else if (strcmp(interface, "wl_shm") == 0) {
		client->wl_shm =
			wl_registry_bind(registry, id,
					 &wl_shm_interface, 1);
		wl_shm_add_listener(client->wl_shm, &shm_listener, client);
	} else if (strcmp(interface, "wl_output") == 0) {
		output = malloc(sizeof *output);
		output->wl_output =
			wl_registry_bind(registry, id,
					 &wl_output_interface, 1);
		wl_output_add_listener(output->wl_output,
				       &output_listener, output);
		client->output = output;
	} else if (strcmp(interface, "wl_test") == 0) {
		test = calloc(1, sizeof *test);
		test->wl_test =
			wl_registry_bind(registry, id,
					 &wl_test_interface, 1);
		wl_test_add_listener(test->wl_test, &test_listener, test);
		client->test = test;
	}
}

static const struct wl_registry_listener registry_listener = {
	handle_global
};

static void
log_handler(const char *fmt, va_list args)
{
	fprintf(stderr, "libwayland: ");
	vfprintf(stderr, fmt, args);
}

struct client *
client_create(int x, int y, int width, int height)
{
	struct client *client;
	struct surface *surface;

	wl_log_set_handler_client(log_handler);

	/* connect to display */
	client = calloc(1, sizeof *client);
	client->wl_display = wl_display_connect(NULL);
	assert(client->wl_display);
	wl_list_init(&client->global_list);

	/* setup registry so we can bind to interfaces */
	client->wl_registry = wl_display_get_registry(client->wl_display);
	wl_registry_add_listener(client->wl_registry, &registry_listener, client);

	/* trigger global listener */
	wl_display_dispatch(client->wl_display);
	wl_display_roundtrip(client->wl_display);

	/* must have WL_SHM_FORMAT_ARGB32 */
	assert(client->has_argb);

	/* must have wl_test interface */
	assert(client->test);

	/* must have an output */
	assert(client->output);

	/* initialize the client surface */
	surface = calloc(1, sizeof *surface);

	surface->wl_surface =
		wl_compositor_create_surface(client->wl_compositor);
	assert(surface->wl_surface);

	wl_surface_add_listener(surface->wl_surface, &surface_listener,
				surface);

	client->surface = surface;
	wl_surface_set_user_data(surface->wl_surface, surface);

	surface->width = width;
	surface->height = height;
	surface->wl_buffer = create_shm_buffer(client, width, height,
					       &surface->data);

	memset(surface->data, 64, width * height * 4);

	move_client(client, x, y);

	return client;
}
