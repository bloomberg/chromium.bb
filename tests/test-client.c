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
#include <GLES2/gl2.h> /* needed for GLfloat */

struct display {
	struct wl_display *display;
	struct wl_compositor *compositor;
	struct input *input;
	struct output *output;
};

struct input {
	struct wl_seat *seat;
	struct wl_pointer *pointer;
	struct wl_keyboard *keyboard;
	GLfloat x, y;
	uint32_t button_mask;
	struct surface *pointer_focus;
	struct surface *keyboard_focus;
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
}

static void
pointer_handle_leave(void *data, struct wl_pointer *pointer,
		     uint32_t serial, struct wl_surface *surface)
{
	struct input *input = data;

	input->pointer_focus = NULL;
}

static void
pointer_handle_motion(void *data, struct wl_pointer *pointer,
		      uint32_t time, wl_fixed_t x, wl_fixed_t y)
{
	struct input *input = data;

	input->x = wl_fixed_to_double(x);
	input->y = wl_fixed_to_double(y);
}

static void
pointer_handle_button(void *data, struct wl_pointer *pointer,
		      uint32_t serial, uint32_t time, uint32_t button,
		      uint32_t state)
{
	struct input *input = data;
	uint32_t bit;

	bit = 1 << (button - 272);
	if (state)
		input->button_mask |= bit;
	else
		input->button_mask &= ~bit;
}

static void
pointer_handle_axis(void *data, struct wl_pointer *pointer,
		    uint32_t time, uint32_t axis, int32_t value)
{
}

static void
keyboard_handle_enter(void *data, struct wl_keyboard *keyboard,
		      uint32_t serial, struct wl_surface *surface,
		      struct wl_array *keys)
{
	struct input *input = data;

	input->keyboard_focus = wl_surface_get_user_data(surface);
}

static void
keyboard_handle_leave(void *data, struct wl_keyboard *keyboard,
		      uint32_t serial, struct wl_surface *surface)
{
	struct input *input = data;

	input->keyboard_focus = NULL;
}

static void
keyboard_handle_key(void *data, struct wl_keyboard *keyboard,
		    uint32_t serial, uint32_t time, uint32_t key,
		    uint32_t state)
{
}

static const struct wl_pointer_listener pointer_listener = {
	pointer_handle_enter,
	pointer_handle_leave,
	pointer_handle_motion,
	pointer_handle_button,
	pointer_handle_axis,
};

static const struct wl_keyboard_listener keyboard_listener = {
	keyboard_handle_enter,
	keyboard_handle_leave,
	keyboard_handle_key,
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
		       const char *model)
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
handle_global(struct wl_display *_display, uint32_t id,
	      const char *interface, uint32_t version, void *data)
{
	struct display *display = data;
	struct input *input;
	struct output *output;

	if (strcmp(interface, "wl_compositor") == 0) {
		display->compositor =
			wl_display_bind(display->display,
					id, &wl_compositor_interface);
	} else if (strcmp(interface, "wl_seat") == 0) {
		input = malloc(sizeof *input);
		input->seat = wl_display_bind(display->display, id,
					      &wl_seat_interface);
		input->pointer_focus = NULL;
		input->keyboard_focus = NULL;

		wl_seat_add_listener(input->seat, &seat_listener, input);
		display->input = input;
	} else if (strcmp(interface, "wl_output") == 0) {
		output = malloc(sizeof *output);
		output->output = wl_display_bind(display->display,
						 id, &wl_output_interface);
		wl_output_add_listener(output->output,
				       &output_listener, output);
		display->output = output;

		fprintf(stderr, "created output global %p\n", display->output);
	}
}

static void
surface_enter(void *data,
	      struct wl_surface *wl_surface, struct wl_output *output)
{
	struct surface *surface = data;

	surface->output = wl_output_get_user_data(output);

	fprintf(stderr, "got surface enter, output %p\n", surface->output);
}

static void
surface_leave(void *data,
	      struct wl_surface *wl_surface, struct wl_output *output)
{
	struct surface *surface = data;

	surface->output = NULL;
}

static const struct wl_surface_listener surface_listener = {
	surface_enter,
	surface_leave
};

static void
create_surface(int fd, struct display *display)
{
	struct surface *surface;
	char buf[64];
	int len;

	surface = malloc(sizeof *surface);
	assert(surface);
	surface->surface = wl_compositor_create_surface(display->compositor);
	wl_surface_add_listener(surface->surface, &surface_listener, surface);
	wl_display_flush(display->display);

	len = snprintf(buf, sizeof buf, "surface %d\n",
		       wl_proxy_get_id((struct wl_proxy *) surface->surface));
	assert(write(fd, buf, len) == len);

	poll(NULL, 0, 100); /* Wait for next frame where we'll get events. */
	wl_display_roundtrip(display->display);

	assert(surface->output == display->output);
	assert(display->input->pointer_focus == surface);
	assert(display->input->x == 50);
	assert(display->input->y == 50);
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

	wl_display_add_global_listener(display->display,
				       handle_global, display);
	wl_display_iterate(display->display, WL_DISPLAY_READABLE);
	wl_display_roundtrip(display->display);

	fd = 0;
	p = getenv("TEST_SOCKET");
	if (p)
		fd = strtol(p, NULL, 0);

	while (1) {
		ret = read(fd, buf, sizeof buf);
		if (ret == -1) {
			fprintf(stderr, "read error: fd %d, %m\n", fd);
			return -1;
		}

		fprintf(stderr, "test-client: got %.*s\n", ret - 1, buf);

		if (strncmp(buf, "bye\n", ret) == 0) {
			return 0;
		} else if (strncmp(buf, "create-surface\n", ret) == 0) {
			create_surface(fd, display);
		} else {
			fprintf(stderr, "unknown command %.*s\n", ret, buf);
			return -1;
		}
	}

	assert(0);
}
