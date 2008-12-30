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

#ifndef _WAYLAND_CLIENT_H
#define _WAYLAND_CLIENT_H

struct wl_object;
struct wl_display;
struct wl_surface;
struct wl_visual;

#define WL_DISPLAY_READABLE 0x01
#define WL_DISPLAY_WRITABLE 0x02

int
wl_object_implements(struct wl_object *object,
		     const char *interface, int version);

typedef int (*wl_display_update_func_t)(uint32_t mask, void *data);

struct wl_display *wl_display_create(const char *name, size_t name_size);
void wl_display_destroy(struct wl_display *display);
int wl_display_get_fd(struct wl_display *display,
		      wl_display_update_func_t update, void *data);

void wl_display_iterate(struct wl_display *display, uint32_t mask);

struct wl_global_listener;
typedef void (*wl_display_global_func_t)(struct wl_display *display,
					 struct wl_object *object,
					 void *data);
void
wl_display_remove_global_listener(struct wl_display *display,
				  struct wl_global_listener *listener);

struct wl_global_listener *
wl_display_add_global_listener(struct wl_display *display,
			       wl_display_global_func_t handler, void *data);
struct wl_compositor *
wl_display_get_compositor(struct wl_display *display);
struct wl_visual *
wl_display_get_argb_visual(struct wl_display *display);
struct wl_visual *
wl_display_get_premultiplied_argb_visual(struct wl_display *display);
struct wl_visual *
wl_display_get_rgb_visual(struct wl_display *display);

struct wl_compositor_listener {
	void (*acknowledge)(void *data,
			    struct wl_compositor *compositor,
			    uint32_t key, uint32_t frame);
	void (*frame)(void *data,
		      struct wl_compositor *compositor,
		      uint32_t frame, uint32_t timestamp);
};

struct wl_surface *
wl_compositor_create_surface(struct wl_compositor *compositor);
void
wl_compositor_commit(struct wl_compositor *compositor, uint32_t key);
int
wl_compositor_add_listener(struct wl_compositor *compostior,
			   const struct wl_compositor_listener *listener,
			   void *data);


void wl_surface_destroy(struct wl_surface *surface);
void wl_surface_attach(struct wl_surface *surface, uint32_t name,
		       int32_t width, int32_t height, uint32_t stride,
		       struct wl_visual *visual);
void wl_surface_map(struct wl_surface *surface,
		    int32_t x, int32_t y, int32_t width, int32_t height);
void wl_surface_copy(struct wl_surface *surface, int32_t dst_x, int32_t dst_y,
		     uint32_t name, uint32_t stride,
		     int32_t x, int32_t y, int32_t width, int32_t height);
void wl_surface_damage(struct wl_surface *surface,
		       int32_t x, int32_t y, int32_t width, int32_t height);

struct wl_output;
struct wl_output_listener {
	void (*geometry)(void *data,
			 struct wl_output *output,
			 int32_t width, int32_t height);
};

int
wl_output_add_listener(struct wl_output *output,
		       const struct wl_output_listener *listener,
		       void *data);

struct wl_input_device;
struct wl_input_device_listener {
	void (*motion)(void *data,
		       struct wl_input_device *input_device,
		       int32_t x, int32_t y, int32_t sx, int32_t sy);
	void (*button)(void *data,
		       struct wl_input_device *input_device,
		       uint32_t button, uint32_t state,
		       int32_t x, int32_t y, int32_t sx, int32_t sy);
	void (*key)(void *data,
		    struct wl_input_device *input_device,
		    uint32_t button, uint32_t state);
};

int
wl_input_device_add_listener(struct wl_input_device *input_device,
			     const struct wl_input_device_listener *listener,
			     void *data);


/* These entry points are for client side implementation of custom
 * objects. */

uint32_t wl_display_get_object_id(struct wl_display *display,
				  const char *interface, uint32_t version);
uint32_t wl_display_allocate_id(struct wl_display *display);
void wl_display_write(struct wl_display *display,
		      const void *data,
		      size_t count);
void wl_display_advertise_global(struct wl_display *display,
				 struct wl_object *object);
#endif
