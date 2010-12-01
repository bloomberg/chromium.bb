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

#ifndef WAYLAND_H
#define WAYLAND_H

#ifdef  __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include "wayland-util.h"
#include "wayland-server-protocol.h"

enum {
	WL_EVENT_READABLE = 0x01,
	WL_EVENT_WRITEABLE = 0x02
};

/* FIXME: We really want in-process objects here, so that the
 * compositor grabs can be implemented as passive grabs and the events
 * be delivered to an in-process listener.  For now, we use this
 * special case as the grabbing surface. */
extern struct wl_surface wl_grab_surface;

struct wl_event_loop;
struct wl_event_source;
typedef void (*wl_event_loop_fd_func_t)(int fd, uint32_t mask, void *data);
typedef void (*wl_event_loop_timer_func_t)(void *data);
typedef void (*wl_event_loop_signal_func_t)(int signal_number, void *data);
typedef void (*wl_event_loop_idle_func_t)(void *data);

struct wl_event_loop *wl_event_loop_create(void);
void wl_event_loop_destroy(struct wl_event_loop *loop);
struct wl_event_source *wl_event_loop_add_fd(struct wl_event_loop *loop,
					     int fd, uint32_t mask,
					     wl_event_loop_fd_func_t func,
					     void *data);
int wl_event_source_fd_update(struct wl_event_source *source, uint32_t mask);
struct wl_event_source *wl_event_loop_add_timer(struct wl_event_loop *loop,
						wl_event_loop_timer_func_t func,
						void *data);
struct wl_event_source *
wl_event_loop_add_signal(struct wl_event_loop *loop,
			int signal_number,
			wl_event_loop_signal_func_t func,
			void *data);

int wl_event_source_timer_update(struct wl_event_source *source,
				 int ms_delay);
int wl_event_source_remove(struct wl_event_source *source);


int wl_event_loop_dispatch(struct wl_event_loop *loop, int timeout);
struct wl_event_source *wl_event_loop_add_idle(struct wl_event_loop *loop,
					       wl_event_loop_idle_func_t func,
					       void *data);
int wl_event_loop_get_fd(struct wl_event_loop *loop);

struct wl_client;
struct wl_display;
struct wl_input_device;

struct wl_display *wl_display_create(void);
void wl_display_destroy(struct wl_display *display);
struct wl_event_loop *wl_display_get_event_loop(struct wl_display *display);
int wl_display_add_socket(struct wl_display *display, const char *name);
void wl_display_terminate(struct wl_display *display);
void wl_display_run(struct wl_display *display);

void wl_display_add_object(struct wl_display *display, struct wl_object *object);

typedef void (*wl_client_connect_func_t)(struct wl_client *client, struct wl_object *global);

int wl_display_add_global(struct wl_display *display, struct wl_object *object, wl_client_connect_func_t func);

void wl_client_destroy(struct wl_client *client);
void wl_client_post_no_memory(struct wl_client *client);
void wl_client_post_global(struct wl_client *client, struct wl_object *object);

struct wl_compositor {
	struct wl_object object;
};

struct wl_resource {
	struct wl_object object;
	void (*destroy)(struct wl_resource *resource,
			struct wl_client *client);
	struct wl_list link;
};

struct wl_buffer {
	struct wl_resource resource;
	struct wl_compositor *compositor;
	struct wl_visual *visual;
	int32_t width, height;
	void (*attach)(struct wl_buffer *buffer, struct wl_surface *surface);
	void (*damage)(struct wl_buffer *buffer,
		       struct wl_surface *surface,
		       int32_t x, int32_t y, int32_t width, int32_t height);
};

struct wl_surface {
	struct wl_resource resource;
	struct wl_client *client;
};

struct wl_shell {
	struct wl_object object;
};

struct wl_input_device {
	struct wl_object object;
	struct wl_surface *pointer_focus;
	struct wl_surface *keyboard_focus;
	struct wl_array keys;
	uint32_t pointer_focus_time;
	uint32_t keyboard_focus_time;
};

struct wl_visual {
	struct wl_object object;
};

struct wl_drag_offer {
	struct wl_object object;
};

struct wl_drag {
	struct wl_resource resource;
	struct wl_drag_offer drag_offer;
	struct wl_surface *source;
	struct wl_surface *pointer_focus;
	struct wl_client *target;
	int32_t x, y, sx, sy;
	struct wl_input_device *input_device;
	struct wl_array types;
	const char *type;
	uint32_t pointer_focus_time;
};

void
wl_client_post_event(struct wl_client *client,
		      struct wl_object *sender,
		      uint32_t event, ...);

int
wl_display_set_compositor(struct wl_display *display,
			  struct wl_compositor *compositor,
			  const struct wl_compositor_interface *implementation);

void
wl_display_post_frame(struct wl_display *display, uint32_t msecs);

void
wl_client_add_resource(struct wl_client *client,
		       struct wl_resource *resource);

struct wl_display *
wl_client_get_display(struct wl_client *client);

void
wl_resource_destroy(struct wl_resource *resource, struct wl_client *client);

void
wl_input_device_set_pointer_focus(struct wl_input_device *device,
				  struct wl_surface *surface,
				  uint32_t time,
				  int32_t x, int32_t y,
				  int32_t sx, int32_t sy);

void
wl_input_device_set_keyboard_focus(struct wl_input_device *device,
				   struct wl_surface *surface,
				   uint32_t time);


#ifdef  __cplusplus
}
#endif

#endif
