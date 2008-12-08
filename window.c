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

#include "wayland-client.h"
#include "wayland-glib.h"

#include "cairo-util.h"

#include "window.h"

struct window {
	struct wl_display *display;
	struct wl_surface *surface;
	const char *title;
	int x, y, width, height;
	int minimum_width, minimum_height;
	int margin;
	int drag_x, drag_y, last_x, last_y;
	int state;
	uint32_t name;
	int fd;

	struct buffer *buffer;

	window_resize_handler_t resize_handler;
	window_frame_handler_t frame_handler;
	window_acknowledge_handler_t acknowledge_handler;
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

void
window_draw(struct window *window)
{
	cairo_surface_t *surface;
	cairo_t *cr;
	int border = 2, radius = 5;
	cairo_text_extents_t extents;
	cairo_pattern_t *gradient, *outline, *bright, *dim;

	surface = cairo_image_surface_create(CAIRO_FORMAT_RGB24,
					     window->width + window->margin * 2,
					     window->height + window->margin * 2);

	outline = cairo_pattern_create_rgb(0.1, 0.1, 0.1);
	bright = cairo_pattern_create_rgb(0.8, 0.8, 0.8);
	dim = cairo_pattern_create_rgb(0.4, 0.4, 0.4);

	cr = cairo_create(surface);

	cairo_translate(cr, window->margin + 7, window->margin + 5);
	cairo_set_line_width (cr, border);
	cairo_set_source_rgba(cr, 0, 0, 0, 0.7);
	rounded_rect(cr, 0, 0, window->width, window->height, radius);
	cairo_fill(cr);
	blur_surface(surface, 24 + radius);

	cairo_translate(cr, -7, -5);
	cairo_set_line_width (cr, border);
	rounded_rect(cr, 1, 1, window->width - 1, window->height - 1, radius);
	cairo_set_source(cr, outline);
	cairo_stroke(cr);
	rounded_rect(cr, 2, 2, window->width - 2, window->height - 2, radius - 1);
	cairo_set_source(cr, bright);
	cairo_stroke(cr);
	rounded_rect(cr, 3, 3, window->width - 2, window->height - 2, radius - 1);
	cairo_set_source(cr, dim);
	cairo_stroke(cr);

	rounded_rect(cr, 2, 2, window->width - 2, window->height - 2, radius - 1);
	gradient = cairo_pattern_create_linear (0, 0, 0, 100);
	cairo_pattern_add_color_stop_rgb(gradient, 0, 0.6, 0.6, 0.4);
	cairo_pattern_add_color_stop_rgb(gradient, 1, 0.8, 0.8, 0.7);
	cairo_set_source(cr, gradient);
	cairo_fill(cr);
	cairo_pattern_destroy(gradient);

	cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
	cairo_move_to(cr, 10, 50);
	cairo_line_to(cr, window->width - 10, 50);
	cairo_line_to(cr, window->width - 10, window->height - 10);
	cairo_line_to(cr, 10, window->height - 10);
	cairo_close_path(cr);
	cairo_set_source(cr, dim);
	cairo_stroke(cr);

	cairo_move_to(cr, 11, 51);
	cairo_line_to(cr, window->width - 10, 51);
	cairo_line_to(cr, window->width - 10, window->height - 10);
	cairo_line_to(cr, 11, window->height - 10);
	cairo_close_path(cr);
	cairo_set_source(cr, bright);
	cairo_stroke(cr);

	cairo_set_operator(cr, CAIRO_OPERATOR_OVER);
	cairo_set_font_size(cr, 14);
	cairo_text_extents(cr, window->title, &extents);
	cairo_move_to(cr, (window->width - extents.width) / 2, 10 - extents.y_bearing);
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

	wl_surface_attach(window->surface,
			  window->buffer->name,
			  window->buffer->width,
			  window->buffer->height,
			  window->buffer->stride);

	wl_surface_map(window->surface,
		       window->x - window->margin,
		       window->y - window->margin,
		       window->width + 2 * window->margin,
		       window->height + 2 * window->margin);
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

	/* FIXME: Object ID 1 is the display, for anything else we
	 * assume it's an input device. */
	if (object == 1 && opcode == 3) {
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

	} else if (object == 1 && opcode == 4) {
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

		window->last_x = x;
		window->last_y = y;
		switch (window->state) {
		case WINDOW_MOVING:
			window->x = window->drag_x + x;
			window->y = window->drag_y + y;
			wl_surface_map(window->surface,
				       window->x - window->margin,
				       window->y - window->margin,
				       window->width + 2 * window->margin,
				       window->height + 2 * window->margin);
			wl_display_commit(window->display, 1);
			break;
		case WINDOW_RESIZING_LOWER_RIGHT:
			window->width = window->drag_x + x;
			window->height = window->drag_y + y;
			if (window->width < window->minimum_width)
				window->width = window->minimum_width;
			if (window->height < window->minimum_height)
				window->height = window->minimum_height;

			if (window->resize_handler)
				(*window->resize_handler)(window,
							  window->width,
							  window->height,
							  window->user_data);
			break;
		}
	} else if (opcode == 1) {
		int button = p[0], state = p[1];

		if (window->x + window->width - grip_size <= window->last_x &&
		    window->last_x < window->x + window->width &&
		    window->y + window->height - grip_size <= window->last_y &&
		    window->last_y < window->y + window->height) {
			location = LOCATION_LOWER_RIGHT;
		} else if (window->x <= window->last_x &&
			   window->last_x < window->x + window->width &&
			   window->y <= window->last_y &&
			   window->last_y < window->y + window->height) {
			location = LOCATION_INTERIOR;
		} else {
			location = LOCATION_OUTSIDE;
		}

		if (button == 0 && state == 1) {
			switch (location) {
			case LOCATION_INTERIOR:
				window->drag_x = window->x - window->last_x;
				window->drag_y = window->y - window->last_y;
				window->state = WINDOW_MOVING;
				break;
			case LOCATION_LOWER_RIGHT:
				window->drag_x = window->width - window->last_x;
				window->drag_y = window->height - window->last_y;
				window->state = WINDOW_RESIZING_LOWER_RIGHT;
				break;
			default:
				window->state = WINDOW_STABLE;
				break;
			}
		} else if (button == 0 && state == 0) {
			window->state = WINDOW_STABLE;
		}
	}
}

void
window_get_child_rectangle(struct window *window,
			   struct rectangle *rectangle)
{
	rectangle->x = 10;
	rectangle->y = 50;
	rectangle->width = window->width - 20;
	rectangle->height = window->height - 60;
}

void
window_copy(struct window *window,
	    struct rectangle *rectangle,
	    uint32_t name, uint32_t stride)
{
	wl_surface_copy(window->surface,
			window->margin + rectangle->x,
			window->margin + rectangle->y,
			name, stride,
			0, 0, rectangle->width, rectangle->height);
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
window_set_minimum_size(struct window *window, uint32_t width, int32_t height)
{
	window->minimum_width = width;
	window->minimum_height = height;
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
	window->surface = wl_display_create_surface(display);
	window->x = x;
	window->y = y;
	window->minimum_width = 100;
	window->minimum_height = 100;
	window->width = width;
	window->height = height;
	window->margin = 16;
	window->state = WINDOW_STABLE;
	window->fd = fd;

	wl_display_set_event_handler(display, event_handler, window);

	return window;
}
