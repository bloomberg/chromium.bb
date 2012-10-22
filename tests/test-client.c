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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include <poll.h>
#include <wayland-client.h>
#include <linux/input.h>

struct display {
	struct wl_display *display;
	struct wl_registry *registry;
	struct wl_compositor *compositor;
	struct input *input;
	struct output *output;
	struct surface *surface;
};

struct input {
	struct wl_seat *seat;
	struct wl_pointer *pointer;
	struct wl_keyboard *keyboard;
	float x, y;
	uint32_t button_mask;
	struct surface *pointer_focus;
	struct surface *keyboard_focus;
	uint32_t last_key, last_key_state;
};

struct output {
	struct wl_output *output;
	int x, y;
	int width, height;
};

struct surface {
	struct wl_surface *surface;
	struct output *output;
};

static void
pointer_handle_enter(void *data, struct wl_pointer *pointer,
		     uint32_t serial, struct wl_surface *surface,
		     wl_fixed_t x, wl_fixed_t y)
{
	struct input *input = data;

	input->pointer_focus = wl_surface_get_user_data(surface);
	input->x = wl_fixed_to_double(x);
	input->y = wl_fixed_to_double(y);
	fprintf(stderr, "test-client: got pointer enter %f %f, surface %p\n",
		input->x, input->y, surface);
}

static void
pointer_handle_leave(void *data, struct wl_pointer *pointer,
		     uint32_t serial, struct wl_surface *surface)
{
	struct input *input = data;

	input->pointer_focus = NULL;
	
	fprintf(stderr, "test-client: got pointer leave, surface %p\n",
		surface);
}

static void
pointer_handle_motion(void *data, struct wl_pointer *pointer,
		      uint32_t time, wl_fixed_t x, wl_fixed_t y)
{
	struct input *input = data;

	input->x = wl_fixed_to_double(x);
	input->y = wl_fixed_to_double(y);
	
	fprintf(stderr, "test-client: got pointer motion %f %f\n",
		input->x, input->y);
}

static void
pointer_handle_button(void *data, struct wl_pointer *pointer,
		      uint32_t serial, uint32_t time, uint32_t button,
		      uint32_t state_w)
{
	struct input *input = data;
	uint32_t bit;
	enum wl_pointer_button_state state = state_w;

	bit = 1 << (button - BTN_LEFT);
	if (state == WL_POINTER_BUTTON_STATE_PRESSED)
		input->button_mask |= bit;
	else
		input->button_mask &= ~bit;
	fprintf(stderr, "test-client: got pointer button %u %u\n",
		button, state_w);
}

static void
pointer_handle_axis(void *data, struct wl_pointer *pointer,
		    uint32_t time, uint32_t axis, wl_fixed_t value)
{
	fprintf(stderr, "test-client: got pointer axis %u %d\n", axis, value);
}

static void
keyboard_handle_keymap(void *data, struct wl_keyboard *keyboard,
		       uint32_t format, int fd, uint32_t size)
{
	close(fd);
	fprintf(stderr, "test-client: got keyboard keymap\n");
}

static void
keyboard_handle_enter(void *data, struct wl_keyboard *keyboard,
		      uint32_t serial, struct wl_surface *surface,
		      struct wl_array *keys)
{
	struct input *input = data;

	input->keyboard_focus = wl_surface_get_user_data(surface);
	fprintf(stderr, "test-client: got keyboard enter, surface %p\n",
		surface);
}

static void
keyboard_handle_leave(void *data, struct wl_keyboard *keyboard,
		      uint32_t serial, struct wl_surface *surface)
{
	struct input *input = data;

	input->keyboard_focus = NULL;
	fprintf(stderr, "test-client: got keyboard leave, surface %p\n",
		surface);
}

static void
keyboard_handle_key(void *data, struct wl_keyboard *keyboard,
		    uint32_t serial, uint32_t time, uint32_t key,
		    uint32_t state)
{
	struct input *input = data;

	input->last_key = key;
	input->last_key_state = state;

	fprintf(stderr, "test-client: got keyboard key %u %u\n", key, state);
}

static void
keyboard_handle_modifiers(void *data, struct wl_keyboard *keyboard,
			  uint32_t serial, uint32_t mods_depressed,
			  uint32_t mods_latched, uint32_t mods_locked,
			  uint32_t group)
{
	fprintf(stderr, "test-client: got keyboard modifier\n");
}

static const struct wl_pointer_listener pointer_listener = {
	pointer_handle_enter,
	pointer_handle_leave,
	pointer_handle_motion,
	pointer_handle_button,
	pointer_handle_axis,
};

static const struct wl_keyboard_listener keyboard_listener = {
	keyboard_handle_keymap,
	keyboard_handle_enter,
	keyboard_handle_leave,
	keyboard_handle_key,
	keyboard_handle_modifiers,
};

static void
seat_handle_capabilities(void *data, struct wl_seat *seat,
			 enum wl_seat_capability caps)
{
	struct input *input = data;

	if ((caps & WL_SEAT_CAPABILITY_POINTER) && !input->pointer) {
		input->pointer = wl_seat_get_pointer(seat);
		wl_pointer_set_user_data(input->pointer, input);
		wl_pointer_add_listener(input->pointer, &pointer_listener,
					input);
	} else if (!(caps & WL_SEAT_CAPABILITY_POINTER) && input->pointer) {
		wl_pointer_destroy(input->pointer);
		input->pointer = NULL;
	}

	if ((caps & WL_SEAT_CAPABILITY_KEYBOARD) && !input->keyboard) {
		input->keyboard = wl_seat_get_keyboard(seat);
		wl_keyboard_set_user_data(input->keyboard, input);
		wl_keyboard_add_listener(input->keyboard, &keyboard_listener,
					 input);
	} else if (!(caps & WL_SEAT_CAPABILITY_KEYBOARD) && input->keyboard) {
		wl_keyboard_destroy(input->keyboard);
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
handle_global(void *data, struct wl_registry *registry, uint32_t id,
	      const char *interface, uint32_t version)
{
	struct display *display = data;
	struct input *input;
	struct output *output;

	if (strcmp(interface, "wl_compositor") == 0) {
		display->compositor =
			wl_registry_bind(display->registry,
					 id, &wl_compositor_interface, 1);
	} else if (strcmp(interface, "wl_seat") == 0) {
		input = calloc(1, sizeof *input);
		input->seat = wl_registry_bind(display->registry, id,
					       &wl_seat_interface, 1);
		input->pointer_focus = NULL;
		input->keyboard_focus = NULL;

		wl_seat_add_listener(input->seat, &seat_listener, input);
		display->input = input;
	} else if (strcmp(interface, "wl_output") == 0) {
		output = malloc(sizeof *output);
		output->output = wl_registry_bind(display->registry,
						  id, &wl_output_interface, 1);
		wl_output_add_listener(output->output,
				       &output_listener, output);
		display->output = output;

		fprintf(stderr, "test-client: created output global %p\n",
			display->output);
	}
}

static const struct wl_registry_listener registry_listener = {
	handle_global
};

static void
surface_enter(void *data,
	      struct wl_surface *wl_surface, struct wl_output *output)
{
	struct surface *surface = data;

	surface->output = wl_output_get_user_data(output);

	fprintf(stderr, "test-client: got surface enter, output %p\n",
		surface->output);
}

static void
surface_leave(void *data,
	      struct wl_surface *wl_surface, struct wl_output *output)
{
	struct surface *surface = data;

	surface->output = NULL;

	fprintf(stderr, "test-client: got surface leave, output %p\n",
		wl_output_get_user_data(output));
}

static const struct wl_surface_listener surface_listener = {
	surface_enter,
	surface_leave
};

static void
send_keyboard_state(int fd, struct display *display)
{
	char buf[64];
	int len;
	int focus = display->input->keyboard_focus != NULL;

	if (focus) {
		assert(display->input->keyboard_focus == display->surface);
	}

	wl_display_flush(display->display);

	len = snprintf(buf, sizeof buf, "%u %u %d\n", display->input->last_key,
		       display->input->last_key_state, focus);
	assert(write(fd, buf, len) == len);

	wl_display_roundtrip(display->display);
}

static void
send_button_state(int fd, struct display *display)
{
	char buf[64];
	int len;

	wl_display_roundtrip(display->display);

	len = snprintf(buf, sizeof buf, "%u\n", display->input->button_mask);
	assert(write(fd, buf, len) == len);

	wl_display_roundtrip(display->display);
}

static void
send_state(int fd, struct display* display)
{
	char buf[64];
	int len;
	int visible = display->surface->output != NULL;
	wl_fixed_t x = wl_fixed_from_int(-1);
	wl_fixed_t y = wl_fixed_from_int(-1);

	if (display->input->pointer_focus != NULL) {
		assert(display->input->pointer_focus == display->surface);
		x = wl_fixed_from_double(display->input->x);
		y = wl_fixed_from_double(display->input->y);
	}

	if (visible) {
		/* FIXME: this fails on multi-display setup */
		/* assert(display->surface->output == display->output); */
	}

	wl_display_flush(display->display);

	len = snprintf(buf, sizeof buf, "%d %d %d\n", x, y, visible);
	assert(write(fd, buf, len) == len);

	wl_display_roundtrip(display->display);
}

static void
create_surface(int fd, struct display *display)
{
	struct surface *surface;
	char buf[64];
	int len;

	surface = malloc(sizeof *surface);
	assert(surface);
	display->surface = surface;
	surface->surface = wl_compositor_create_surface(display->compositor);
	wl_surface_add_listener(surface->surface, &surface_listener, surface);

	wl_display_flush(display->display);

	len = snprintf(buf, sizeof buf, "surface %d\n",
		       wl_proxy_get_id((struct wl_proxy *) surface->surface));
	assert(write(fd, buf, len) == len);

	poll(NULL, 0, 100); /* Wait for next frame where we'll get events. */

	wl_display_roundtrip(display->display);
}

int main(int argc, char *argv[])
{
	struct display *display;
	char buf[256], *p;
	int ret, fd;

	display = malloc(sizeof *display);
	assert(display);

	display->display = wl_display_connect(NULL);
	assert(display->display);

	display->registry = wl_display_get_registry(display->display);
	wl_registry_add_listener(display->registry,
				 &registry_listener, display);
	wl_display_dispatch(display->display);
	wl_display_dispatch(display->display);

	fd = 0;
	p = getenv("TEST_SOCKET");
	if (p)
		fd = strtol(p, NULL, 0);

	while (1) {
		ret = read(fd, buf, sizeof buf);
		if (ret == -1) {
			fprintf(stderr, "test-client: read error: fd %d, %m\n",
				fd);
			return -1;
		}

		fprintf(stderr, "test-client: got %.*s\n", ret - 1, buf);

		if (strncmp(buf, "bye\n", ret) == 0) {
			return 0;
		} else if (strncmp(buf, "create-surface\n", ret) == 0) {
			create_surface(fd, display);
		} else if (strncmp(buf, "send-state\n", ret) == 0) {
			send_state(fd, display);
		} else if (strncmp(buf, "send-button-state\n", ret) == 0) {
			send_button_state(fd, display);
		} else if (strncmp(buf, "send-keyboard-state\n", ret) == 0) {
			send_keyboard_state(fd, display);
		} else {
			fprintf(stderr, "test-client: unknown command %.*s\n",
				ret, buf);
			return -1;
		}
	}

	assert(0);
}
