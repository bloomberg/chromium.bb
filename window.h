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

#ifndef _WINDOW_H_
#define _WINDOW_H_

struct window;

struct rectangle {
	int32_t x;
	int32_t y;
	int32_t width;
	int32_t height;
};

typedef void (*window_resize_handler_t)(struct window *window, void *data);
typedef void (*window_frame_handler_t)(struct window *window, uint32_t frame, uint32_t timestamp, void *data);
typedef void (*window_acknowledge_handler_t)(struct window *window, uint32_t key, void *data);
typedef void (*window_key_handler_t)(struct window *window, uint32_t key, uint32_t state, void *data);


struct window *
window_create(struct wl_display *display, int fd,
	      const char *title,
	      int32_t x, int32_t y, int32_t width, int32_t height);

void
window_draw(struct window *window);
void
window_get_child_rectangle(struct window *window,
			   struct rectangle *rectangle);
void
window_set_child_size(struct window *window,
		      struct rectangle *rectangle);
void
window_copy(struct window *window,
	    struct rectangle *rectangle,
	    uint32_t name, uint32_t stride);

cairo_surface_t *
window_create_surface(struct window *window,
		      struct rectangle *rectangle);

void
window_copy_surface(struct window *window,
		    struct rectangle *rectangle,
		    cairo_surface_t *surface);

void
window_set_fullscreen(struct window *window, int fullscreen);

void
window_set_resize_handler(struct window *window,
			  window_resize_handler_t handler, void *data);
void
window_set_frame_handler(struct window *window,
			 window_frame_handler_t handler, void *data);
void
window_set_acknowledge_handler(struct window *window,
			       window_acknowledge_handler_t handler, void *data);
void
window_set_key_handler(struct window *window,
		       window_key_handler_t handler, void *data);

#endif
