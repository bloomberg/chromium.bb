/*
 * Copyright © 2008 Kristian Høgsberg
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

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <math.h>
#include <time.h>
#include <cairo.h>
#include <glib.h>

#include <linux/input.h>
#include "wayland-client.h"
#include "wayland-glib.h"

#include "cairo-util.h"

#include "window.h"

struct window {
	struct wl_display *display;
	struct wl_compositor *compositor;
	struct wl_surface *surface;
	const char *title;
	struct rectangle allocation, saved_allocation;
	int minimum_width, minimum_height;
	int margin;
	int drag_x, drag_y;
	int state;
	int fullscreen;
	uint32_t grab_device;
	uint32_t name;
	int fd;

	struct buffer *buffer;

	window_resize_handler_t resize_handler;
	window_frame_handler_t frame_handler;
	window_acknowledge_handler_t acknowledge_handler;
	window_key_handler_t key_handler;
	void *user_data;
};

static void
rounded_rect(cairo_t *cr, int x0, int y0, int x1, int y1, int radius)
{
	cairo_move_to(cr, x0, y0 + radius);
	cairo_arc(cr, x0 + radius, y0 + radius, radius, M_PI, 3 * M_PI / 2);
	cairo_line_to(cr, x1 - radius, y0);
	cairo_arc(cr, x1 - radius, y0 + radius, radius, 3 * M_PI / 2, 2 * M_PI);
	cairo_line_to(cr, x1, y1 - radius);
	cairo_arc(cr, x1 - radius, y1 - radius, radius, 0, M_PI / 2);
	cairo_line_to(cr, x0 + radius, y1);
	cairo_arc(cr, x0 + radius, y1 - radius, radius, M_PI / 2, M_PI);
	cairo_close_path(cr);
}

static void
window_draw_decorations(struct window *window)
{
	cairo_surface_t *surface;
	cairo_t *cr;
	int border = 2, radius = 5;
	cairo_text_extents_t extents;
	cairo_pattern_t *gradient, *outline, *bright, *dim;
	struct wl_visual *visual;
	int width, height;

	surface = cairo_image_surface_create(CAIRO_FORMAT_RGB24,
					     window->allocation.width,
					     window->allocation.height);

	outline = cairo_pattern_create_rgb(0.1, 0.1, 0.1);
	bright = cairo_pattern_create_rgb(0.8, 0.8, 0.8);
	dim = cairo_pattern_create_rgb(0.4, 0.4, 0.4);

	cr = cairo_create(surface);

	width = window->allocation.width - window->margin * 2;
	height = window->allocation.height - window->margin * 2;

	cairo_translate(cr, window->margin + 7, window->margin + 5);
	cairo_set_line_width (cr, border);
	cairo_set_source_rgba(cr, 0, 0, 0, 0.7);
	rounded_rect(cr, 0, 0, width, height, radius);
	cairo_fill(cr);
	blur_surface(surface, 24 + radius);

	cairo_translate(cr, -7, -5);
	cairo_set_line_width (cr, border);
	rounded_rect(cr, 1, 1, width - 1, height - 1, radius);
	cairo_set_source(cr, outline);
	cairo_stroke(cr);
	rounded_rect(cr, 2, 2, width - 2, height - 2, radius - 1);
	cairo_set_source(cr, bright);
	cairo_stroke(cr);
	rounded_rect(cr, 3, 3, width - 2, height - 2, radius - 1);
	cairo_set_source(cr, dim);
	cairo_stroke(cr);

	rounded_rect(cr, 2, 2, width - 2, height - 2, radius - 1);
	gradient = cairo_pattern_create_linear (0, 0, 0, 100);
	cairo_pattern_add_color_stop_rgb(gradient, 0, 0.6, 0.6, 0.4);
	cairo_pattern_add_color_stop_rgb(gradient, 1, 0.8, 0.8, 0.7);
	cairo_set_source(cr, gradient);
	cairo_fill(cr);
	cairo_pattern_destroy(gradient);

	cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
	cairo_move_to(cr, 10, 50);
	cairo_line_to(cr, width - 10, 50);
	cairo_line_to(cr, width - 10, height - 10);
	cairo_line_to(cr, 10, height - 10);
	cairo_close_path(cr);
	cairo_set_source(cr, dim);
	cairo_stroke(cr);

	cairo_move_to(cr, 11, 51);
	cairo_line_to(cr, width - 10, 51);
	cairo_line_to(cr, width - 10, height - 10);
	cairo_line_to(cr, 11, height - 10);
	cairo_close_path(cr);
	cairo_set_source(cr, bright);
	cairo_stroke(cr);

	cairo_set_operator(cr, CAIRO_OPERATOR_OVER);
	cairo_set_font_size(cr, 14);
	cairo_text_extents(cr, window->title, &extents);
	cairo_move_to(cr, (width - extents.width) / 2, 10 - extents.y_bearing);
	cairo_set_line_cap (cr, CAIRO_LINE_CAP_ROUND);
	cairo_set_line_join (cr, CAIRO_LINE_JOIN_ROUND);
	cairo_set_line_width (cr, 4);
	cairo_text_path(cr, window->title);
	cairo_set_source_rgb(cr, 0.1, 0.1, 0.1);
	cairo_stroke_preserve(cr);
	cairo_set_source_rgb(cr, 1, 1, 1);
	cairo_fill(cr);
	cairo_destroy(cr);

	window->buffer = buffer_create_from_cairo_surface(window->fd, surface);
	cairo_surface_destroy(surface);

	visual = wl_display_get_premultiplied_argb_visual(window->display);
	wl_surface_attach(window->surface,
			  window->buffer->name,
			  window->buffer->width,
			  window->buffer->height,
			  window->buffer->stride,
			  visual);

	wl_surface_map(window->surface,
		       window->allocation.x - window->margin,
		       window->allocation.y - window->margin,
		       window->allocation.width,
		       window->allocation.height);
}

static void
window_draw_fullscreen(struct window *window)
{
	struct wl_visual *visual;
	int stride = window->allocation.width * 4;

	window->buffer = buffer_create(window->fd,
				       window->allocation.width,
				       window->allocation.height,
				       stride);

	visual = wl_display_get_premultiplied_argb_visual(window->display);
	wl_surface_attach(window->surface,
			  window->buffer->name,
			  window->buffer->width,
			  window->buffer->height,
			  window->buffer->stride,
			  visual);

	wl_surface_map(window->surface,
		       window->allocation.x,
		       window->allocation.y,
		       window->allocation.width,
		       window->allocation.height);
}

void
window_draw(struct window *window)
{
	if (window->fullscreen)
		window_draw_fullscreen(window);
	else
		window_draw_decorations(window);
}

enum window_state {
	WINDOW_STABLE,
	WINDOW_MOVING,
	WINDOW_RESIZING_UPPER_LEFT,
	WINDOW_RESIZING_UPPER_RIGHT,
	WINDOW_RESIZING_LOWER_LEFT,
	WINDOW_RESIZING_LOWER_RIGHT
};

enum location {
	LOCATION_INTERIOR,
	LOCATION_UPPER_LEFT,
	LOCATION_UPPER_RIGHT,
	LOCATION_LOWER_LEFT,
	LOCATION_LOWER_RIGHT,
	LOCATION_OUTSIDE
};

static void
event_handler(struct wl_display *display,
	      uint32_t object, uint32_t opcode,
	      uint32_t size, uint32_t *p, void *data)
{
	struct window *window = data;
	int location;
	int grip_size = 16;

	/* FIXME: Object ID 2 is the compositor, for anything else we
	 * assume it's an input device. */
	if (object == 2 && opcode == 0) {
		uint32_t key = p[0];

		/* Ignore acknowledge events for window move requests. */
		if (key != 0)
			return;

		/* The acknowledge event means that the server
		 * processed our last commit request and we can now
		 * safely free the old window buffer if we resized and
		 * render the next frame into our back buffer.. */

		if (window->buffer != NULL) {
			buffer_destroy(window->buffer, window->fd);
			window->buffer = NULL;
		}
		if (window->acknowledge_handler)
			(*window->acknowledge_handler)(window, key,
						       window->user_data);

	} else if (object == 2 && opcode == 1) {
		/* The frame event means that the previous frame was
		 * composited, and we can now send the request to copy
		 * the frame we've rendered in the mean time into the
		 * servers surface buffer. */
		if (window->frame_handler)
			(*window->frame_handler)(window, p[0], p[1],
						 window->user_data);
	} else if (object == 1) {
		fprintf(stderr, "unexpected event from display: %d\n",
			opcode);
		exit(-1);
	} else if (opcode == 0) {
		int x = p[0], y = p[1];

		switch (window->state) {
		case WINDOW_MOVING:
			if (window->fullscreen)
				break;
			if (window->grab_device != object)
				break;
			window->allocation.x = window->drag_x + x;
			window->allocation.y = window->drag_y + y;
			wl_surface_map(window->surface,
				       window->allocation.x - window->margin,
				       window->allocation.y - window->margin,
				       window->allocation.width,
				       window->allocation.height);
			wl_compositor_commit(window->compositor, 1);
			break;
		case WINDOW_RESIZING_LOWER_RIGHT:
			if (window->fullscreen)
				break;
			if (window->grab_device != object)
				break;
			window->allocation.width = window->drag_x + x;
			window->allocation.height = window->drag_y + y;

			if (window->resize_handler)
				(*window->resize_handler)(window,
							  window->user_data);

			break;
		}
	} else if (opcode == 1) {
		int button = p[0], state = p[1];
		int32_t x = p[2], y = p[3];
		int32_t left = window->allocation.x;
		int32_t right = window->allocation.x +
			window->allocation.width - window->margin * 2;
		int32_t top = window->allocation.y;
		int32_t bottom = window->allocation.y +
			window->allocation.height - window->margin * 2;

		if (right - grip_size <= x && x < right &&
		    bottom - grip_size <= y && y < bottom) {
			location = LOCATION_LOWER_RIGHT;
		} else if (left <= x && x < right && top <= y && y < bottom) {
			location = LOCATION_INTERIOR;
		} else {
			location = LOCATION_OUTSIDE;
		}

		if (button == BTN_LEFT && state == 1) {
			switch (location) {
			case LOCATION_INTERIOR:
				window->drag_x = window->allocation.x - x;
				window->drag_y = window->allocation.y - y;
				window->state = WINDOW_MOVING;
				window->grab_device = object;
				break;
			case LOCATION_LOWER_RIGHT:
				window->drag_x = window->allocation.width - x;
				window->drag_y = window->allocation.height - y;
				window->state = WINDOW_RESIZING_LOWER_RIGHT;
				window->grab_device = object;
				break;
			default:
				window->state = WINDOW_STABLE;
				break;
			}
		} else if (button == BTN_LEFT &&
			   state == 0 && object == window->grab_device) {
			window->state = WINDOW_STABLE;
		}
	} else if (opcode == 2) {
		if (window->key_handler)
			(*window->key_handler)(window, p[0], p[1],
					       window->user_data);
	}
}

void
window_get_child_rectangle(struct window *window,
			   struct rectangle *rectangle)
{
	if (window->fullscreen) {
		*rectangle = window->allocation;
	} else {
		rectangle->x = window->margin + 10;
		rectangle->y = window->margin + 50;
		rectangle->width = window->allocation.width - 20 - window->margin * 2;
		rectangle->height = window->allocation.height - 60 - window->margin * 2;
	}
}

void
window_set_child_size(struct window *window,
		      struct rectangle *rectangle)
{
	if (!window->fullscreen) {
		window->allocation.width = rectangle->width + 20 + window->margin * 2;
		window->allocation.height = rectangle->height + 60 + window->margin * 2;
	}
}

void
window_copy(struct window *window,
	    struct rectangle *rectangle,
	    uint32_t name, uint32_t stride)
{
	wl_surface_copy(window->surface,
			rectangle->x,
			rectangle->y,
			name, stride,
			0, 0,
			rectangle->width,
			rectangle->height);
}

void
window_set_fullscreen(struct window *window, int fullscreen)
{
	window->fullscreen = fullscreen;
	if (window->fullscreen) {
		window->saved_allocation = window->allocation;
		window->allocation.x = 0;
		window->allocation.y = 0;
		wl_display_get_geometry(window->display,
					&window->allocation.width,
					&window->allocation.height);
	} else {
		window->allocation = window->saved_allocation;
	}
}

void
window_set_resize_handler(struct window *window,
			  window_resize_handler_t handler, void *data)
{
	window->resize_handler = handler;
	window->user_data = data;
}

void
window_set_frame_handler(struct window *window,
			 window_frame_handler_t handler, void *data)
{
	window->frame_handler = handler;
	window->user_data = data;
}

void
window_set_acknowledge_handler(struct window *window,
			       window_acknowledge_handler_t handler, void *data)
{
	window->acknowledge_handler = handler;
	window->user_data = data;
}

void
window_set_key_handler(struct window *window,
		       window_key_handler_t handler, void *data)
{
	window->key_handler = handler;
	window->user_data = data;
}

struct window *
window_create(struct wl_display *display, int fd,
	      const char *title,
	      int32_t x, int32_t y, int32_t width, int32_t height)
{
	struct window *window;

	window = malloc(sizeof *window);
	if (window == NULL)
		return NULL;

	memset(window, 0, sizeof *window);
	window->display = display;
	window->title = strdup(title);
	window->compositor = wl_display_get_compositor(display);
	window->surface = wl_compositor_create_surface(window->compositor);
	window->allocation.x = x;
	window->allocation.y = y;
	window->allocation.width = width;
	window->allocation.height = height;
	window->saved_allocation = window->allocation;
	window->margin = 16;
	window->state = WINDOW_STABLE;
	window->fd = fd;

	wl_display_set_event_handler(display, event_handler, window);

	return window;
}
