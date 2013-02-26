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

#define _GNU_SOURCE

#include <stdlib.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdbool.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <dlfcn.h>
#include <assert.h>
#include <sys/time.h>
#include <fcntl.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <ffi.h>

#include "wayland-private.h"
#include "wayland-server.h"
#include "wayland-server-protocol.h"
#include "wayland-os.h"

/* This is the size of the char array in struct sock_addr_un.
   No Wayland socket can be created with a path longer than this,
   including the null terminator. */
#ifndef UNIX_PATH_MAX
#define UNIX_PATH_MAX	108
#endif

#define LOCK_SUFFIX	".lock"
#define LOCK_SUFFIXLEN	5

struct wl_socket {
	int fd;
	int fd_lock;
	struct sockaddr_un addr;
	char lock_addr[UNIX_PATH_MAX + LOCK_SUFFIXLEN];
	struct wl_list link;
	struct wl_event_source *source;
};

struct wl_client {
	struct wl_connection *connection;
	struct wl_event_source *source;
	struct wl_display *display;
	struct wl_resource *display_resource;
	uint32_t id_count;
	uint32_t mask;
	struct wl_list link;
	struct wl_map objects;
	struct wl_signal destroy_signal;
	struct ucred ucred;
	int error;
};

struct wl_display {
	struct wl_event_loop *loop;
	int run;

	uint32_t id;
	uint32_t serial;

	struct wl_list registry_resource_list;
	struct wl_list global_list;
	struct wl_list socket_list;
	struct wl_list client_list;

	struct wl_signal destroy_signal;
};

struct wl_global {
	const struct wl_interface *interface;
	uint32_t name;
	void *data;
	wl_global_bind_func_t bind;
	struct wl_list link;
};

static int wl_debug = 0;

static void
destroy_client(void *data)
{
	struct wl_client *client = data;

	wl_client_destroy(client);
}

WL_EXPORT void
wl_resource_post_event(struct wl_resource *resource, uint32_t opcode, ...)
{
	struct wl_closure *closure;
	struct wl_object *object = &resource->object;
	va_list ap;

	va_start(ap, opcode);
	closure = wl_closure_vmarshal(object, opcode, ap,
				      &object->interface->events[opcode]);
	va_end(ap);

	if (closure == NULL)
		return;

	if (wl_closure_send(closure, resource->client->connection))
		wl_event_loop_add_idle(resource->client->display->loop,
				       destroy_client, resource->client);

	if (wl_debug)
		wl_closure_print(closure, object, true);

	wl_closure_destroy(closure);
}


WL_EXPORT void
wl_resource_queue_event(struct wl_resource *resource, uint32_t opcode, ...)
{
	struct wl_closure *closure;
	struct wl_object *object = &resource->object;
	va_list ap;

	va_start(ap, opcode);
	closure = wl_closure_vmarshal(object, opcode, ap,
				      &object->interface->events[opcode]);
	va_end(ap);

	if (closure == NULL)
		return;

	if (wl_closure_queue(closure, resource->client->connection))
		wl_event_loop_add_idle(resource->client->display->loop,
				       destroy_client, resource->client);

	if (wl_debug)
		wl_closure_print(closure, object, true);

	wl_closure_destroy(closure);
}

WL_EXPORT void
wl_resource_post_error(struct wl_resource *resource,
		       uint32_t code, const char *msg, ...)
{
	struct wl_client *client = resource->client;
	char buffer[128];
	va_list ap;

	va_start(ap, msg);
	vsnprintf(buffer, sizeof buffer, msg, ap);
	va_end(ap);

	client->error = 1;

	/*
	 * When a client aborts, its resources are destroyed in id order,
	 * which means the display resource is destroyed first. If destruction
	 * of any later resources results in a protocol error, we end up here
	 * with a NULL display_resource. Do not try to send errors to an
	 * already dead client.
	 */
	if (!client->display_resource)
		return;

	wl_resource_post_event(client->display_resource,
			       WL_DISPLAY_ERROR, resource, code, buffer);
}

static int
wl_client_connection_data(int fd, uint32_t mask, void *data)
{
	struct wl_client *client = data;
	struct wl_connection *connection = client->connection;
	struct wl_resource *resource;
	struct wl_object *object;
	struct wl_closure *closure;
	const struct wl_message *message;
	uint32_t p[2];
	int opcode, size;
	int len;

	if (mask & (WL_EVENT_ERROR | WL_EVENT_HANGUP)) {
		wl_client_destroy(client);
		return 1;
	}

	if (mask & WL_EVENT_WRITABLE) {
		len = wl_connection_flush(connection);
		if (len < 0 && errno != EAGAIN) {
			wl_client_destroy(client);
			return 1;
		} else if (len >= 0) {
			wl_event_source_fd_update(client->source,
						  WL_EVENT_READABLE);
		}
	}

	len = 0;
	if (mask & WL_EVENT_READABLE) {
		len = wl_connection_read(connection);
		if (len < 0 && errno != EAGAIN) {
			wl_client_destroy(client);
			return 1;
		}
	}

	while ((size_t) len >= sizeof p) {
		wl_connection_copy(connection, p, sizeof p);
		opcode = p[1] & 0xffff;
		size = p[1] >> 16;
		if (len < size)
			break;

		resource = wl_map_lookup(&client->objects, p[0]);
		if (resource == NULL) {
			wl_resource_post_error(client->display_resource,
					       WL_DISPLAY_ERROR_INVALID_OBJECT,
					       "invalid object %u", p[0]);
			break;
		}

		object = &resource->object;
		if (opcode >= object->interface->method_count) {
			wl_resource_post_error(client->display_resource,
					       WL_DISPLAY_ERROR_INVALID_METHOD,
					       "invalid method %d, object %s@%u",
					       opcode,
					       object->interface->name,
					       object->id);
			break;
		}

		message = &object->interface->methods[opcode];
		closure = wl_connection_demarshal(client->connection, size,
						  &client->objects, message);
		len -= size;

		if ((closure == NULL && errno == EINVAL) ||
		    wl_closure_lookup_objects(closure, &client->objects) < 0) {
			wl_resource_post_error(client->display_resource,
					       WL_DISPLAY_ERROR_INVALID_METHOD,
					       "invalid arguments for %s@%u.%s",
					       object->interface->name,
					       object->id,
					       message->name);
			break;
		} else if (closure == NULL && errno == ENOMEM) {
			wl_resource_post_no_memory(resource);
			break;
		}

		if (wl_debug)
			wl_closure_print(closure, object, false);

		wl_closure_invoke(closure, object,
				  object->implementation[opcode], client);

		wl_closure_destroy(closure);

		if (client->error)
			break;
	}

	if (client->error)
		wl_client_destroy(client);

	return 1;
}

WL_EXPORT void
wl_client_flush(struct wl_client *client)
{
	wl_connection_flush(client->connection);
}

WL_EXPORT struct wl_display *
wl_client_get_display(struct wl_client *client)
{
	return client->display;
}

static void
bind_display(struct wl_client *client,
	     void *data, uint32_t version, uint32_t id);

WL_EXPORT struct wl_client *
wl_client_create(struct wl_display *display, int fd)
{
	struct wl_client *client;
	socklen_t len;

	client = malloc(sizeof *client);
	if (client == NULL)
		return NULL;

	memset(client, 0, sizeof *client);
	client->display = display;
	client->source = wl_event_loop_add_fd(display->loop, fd,
					      WL_EVENT_READABLE,
					      wl_client_connection_data, client);

	if (!client->source)
		goto err_client;

	len = sizeof client->ucred;
	if (getsockopt(fd, SOL_SOCKET, SO_PEERCRED,
		       &client->ucred, &len) < 0)
		goto err_source;

	client->connection = wl_connection_create(fd);
	if (client->connection == NULL)
		goto err_source;

	wl_map_init(&client->objects);

	if (wl_map_insert_at(&client->objects, 0, NULL) < 0)
		goto err_map;

	wl_signal_init(&client->destroy_signal);
	bind_display(client, display, 1, 1);

	if (!client->display_resource)
		goto err_map;

	wl_list_insert(display->client_list.prev, &client->link);

	return client;

err_map:
	wl_map_release(&client->objects);
	wl_connection_destroy(client->connection);
err_source:
	wl_event_source_remove(client->source);
err_client:
	free(client);
	return NULL;
}

WL_EXPORT void
wl_client_get_credentials(struct wl_client *client,
			  pid_t *pid, uid_t *uid, gid_t *gid)
{
	if (pid)
		*pid = client->ucred.pid;
	if (uid)
		*uid = client->ucred.uid;
	if (gid)
		*gid = client->ucred.gid;
}

WL_EXPORT uint32_t
wl_client_add_resource(struct wl_client *client,
		       struct wl_resource *resource)
{
	if (resource->object.id == 0) {
		resource->object.id =
			wl_map_insert_new(&client->objects,
					  WL_MAP_SERVER_SIDE, resource);
	} else if (wl_map_insert_at(&client->objects,
				  resource->object.id, resource) < 0) {
		wl_resource_post_error(client->display_resource,
				       WL_DISPLAY_ERROR_INVALID_OBJECT,
				       "invalid new id %d",
				       resource->object.id);
		return 0;
	}

	resource->client = client;
	wl_signal_init(&resource->destroy_signal);

	return resource->object.id;
}

WL_EXPORT struct wl_resource *
wl_client_get_object(struct wl_client *client, uint32_t id)
{
	return wl_map_lookup(&client->objects, id);
}

WL_EXPORT void
wl_resource_post_no_memory(struct wl_resource *resource)
{
	wl_resource_post_error(resource->client->display_resource,
			       WL_DISPLAY_ERROR_NO_MEMORY, "no memory");
}

static void
destroy_resource(void *element, void *data)
{
	struct wl_resource *resource = element;

	wl_signal_emit(&resource->destroy_signal, resource);

	if (resource->destroy)
		resource->destroy(resource);
}

WL_EXPORT void
wl_resource_destroy(struct wl_resource *resource)
{
	struct wl_client *client = resource->client;
	uint32_t id;

	id = resource->object.id;
	destroy_resource(resource, NULL);

	if (id < WL_SERVER_ID_START) {
		if (client->display_resource) {
			wl_resource_queue_event(client->display_resource,
						WL_DISPLAY_DELETE_ID, id);
		}
		wl_map_insert_at(&client->objects, id, NULL);
	} else {
		wl_map_remove(&client->objects, id);
	}
}

WL_EXPORT void
wl_client_add_destroy_listener(struct wl_client *client,
			       struct wl_listener *listener)
{
	wl_signal_add(&client->destroy_signal, listener);
}

WL_EXPORT struct wl_listener *
wl_client_get_destroy_listener(struct wl_client *client,
			       wl_notify_func_t notify)
{
	return wl_signal_get(&client->destroy_signal, notify);
}

WL_EXPORT void
wl_client_destroy(struct wl_client *client)
{
	uint32_t serial = 0;
	
	wl_log("disconnect from client %p\n", client);

	wl_signal_emit(&client->destroy_signal, client);

	wl_client_flush(client);
	wl_map_for_each(&client->objects, destroy_resource, &serial);
	wl_map_release(&client->objects);
	wl_event_source_remove(client->source);
	wl_connection_destroy(client->connection);
	wl_list_remove(&client->link);
	free(client);
}

static void
lose_pointer_focus(struct wl_listener *listener, void *data)
{
	struct wl_pointer *pointer =
		container_of(listener, struct wl_pointer, focus_listener);

	pointer->focus_resource = NULL;
}

static void
lose_keyboard_focus(struct wl_listener *listener, void *data)
{
	struct wl_keyboard *keyboard =
		container_of(listener, struct wl_keyboard, focus_listener);

	keyboard->focus_resource = NULL;
}

static void
lose_touch_focus(struct wl_listener *listener, void *data)
{
	struct wl_touch *touch =
		container_of(listener, struct wl_touch, focus_listener);

	touch->focus_resource = NULL;
}

static void
default_grab_focus(struct wl_pointer_grab *grab,
		   struct wl_surface *surface, wl_fixed_t x, wl_fixed_t y)
{
	struct wl_pointer *pointer = grab->pointer;

	if (pointer->button_count > 0)
		return;

	wl_pointer_set_focus(pointer, surface, x, y);
}

static void
default_grab_motion(struct wl_pointer_grab *grab,
		    uint32_t time, wl_fixed_t x, wl_fixed_t y)
{
	struct wl_resource *resource;

	resource = grab->pointer->focus_resource;
	if (resource)
		wl_pointer_send_motion(resource, time, x, y);
}

static void
default_grab_button(struct wl_pointer_grab *grab,
		    uint32_t time, uint32_t button, uint32_t state_w)
{
	struct wl_pointer *pointer = grab->pointer;
	struct wl_resource *resource;
	uint32_t serial;
	enum wl_pointer_button_state state = state_w;

	resource = pointer->focus_resource;
	if (resource) {
		serial = wl_display_next_serial(resource->client->display);
		wl_pointer_send_button(resource, serial, time, button, state_w);
	}

	if (pointer->button_count == 0 &&
	    state == WL_POINTER_BUTTON_STATE_RELEASED)
		wl_pointer_set_focus(pointer, pointer->current,
				     pointer->current_x, pointer->current_y);
}

static const struct wl_pointer_grab_interface
				default_pointer_grab_interface = {
	default_grab_focus,
	default_grab_motion,
	default_grab_button
};

static void default_grab_touch_down(struct wl_touch_grab *grab,
		uint32_t time,
		int touch_id,
		wl_fixed_t sx,
		wl_fixed_t sy)
{
	struct wl_touch *touch = grab->touch;
	uint32_t serial;

	if (touch->focus_resource && touch->focus) {
		serial = wl_display_next_serial(touch->focus_resource->client->display);
		wl_touch_send_down(touch->focus_resource, serial, time,
				&touch->focus->resource, touch_id, sx, sy);
	}
}

static void default_grab_touch_up(struct wl_touch_grab *grab,
		uint32_t time,
		int touch_id)
{
	struct wl_touch *touch = grab->touch;
	uint32_t serial;

	if (touch->focus_resource) {
		serial = wl_display_next_serial(touch->focus_resource->client->display);
		wl_touch_send_up(touch->focus_resource, serial, time, touch_id);
	}
}

static void default_grab_touch_motion(struct wl_touch_grab *grab,
		uint32_t time,
		int touch_id,
		wl_fixed_t sx,
		wl_fixed_t sy)
{
	struct wl_touch *touch = grab->touch;

	if (touch->focus_resource) {
		wl_touch_send_motion(touch->focus_resource, time,
				touch_id, sx, sy);
	}
}

static const struct wl_touch_grab_interface default_touch_grab_interface = {
	default_grab_touch_down,
	default_grab_touch_up,
	default_grab_touch_motion
};

static void
default_grab_key(struct wl_keyboard_grab *grab,
		 uint32_t time, uint32_t key, uint32_t state)
{
	struct wl_keyboard *keyboard = grab->keyboard;
	struct wl_resource *resource;
	uint32_t serial;

	resource = keyboard->focus_resource;
	if (resource) {
		serial = wl_display_next_serial(resource->client->display);
		wl_keyboard_send_key(resource, serial, time, key, state);
	}
}

static struct wl_resource *
find_resource_for_surface(struct wl_list *list, struct wl_surface *surface)
{
	struct wl_resource *r;

	if (!surface)
		return NULL;

	wl_list_for_each(r, list, link) {
		if (r->client == surface->resource.client)
			return r;
	}

	return NULL;
}

static void
default_grab_modifiers(struct wl_keyboard_grab *grab, uint32_t serial,
		       uint32_t mods_depressed, uint32_t mods_latched,
		       uint32_t mods_locked, uint32_t group)
{
	struct wl_keyboard *keyboard = grab->keyboard;
	struct wl_pointer *pointer = keyboard->seat->pointer;
	struct wl_resource *resource, *pr;

	resource = keyboard->focus_resource;
	if (!resource)
		return;

	wl_keyboard_send_modifiers(resource, serial, mods_depressed,
				   mods_latched, mods_locked, group);

	if (pointer && pointer->focus && pointer->focus != keyboard->focus) {
		pr = find_resource_for_surface(&keyboard->resource_list,
					       pointer->focus);
		if (pr) {
			wl_keyboard_send_modifiers(pr,
						   serial,
						   keyboard->modifiers.mods_depressed,
						   keyboard->modifiers.mods_latched,
						   keyboard->modifiers.mods_locked,
						   keyboard->modifiers.group);
		}
	}
}

static const struct wl_keyboard_grab_interface
				default_keyboard_grab_interface = {
	default_grab_key,
	default_grab_modifiers,
};

WL_EXPORT void
wl_pointer_init(struct wl_pointer *pointer)
{
	memset(pointer, 0, sizeof *pointer);
	wl_list_init(&pointer->resource_list);
	pointer->focus_listener.notify = lose_pointer_focus;
	pointer->default_grab.interface = &default_pointer_grab_interface;
	pointer->default_grab.pointer = pointer;
	pointer->grab = &pointer->default_grab;
	wl_signal_init(&pointer->focus_signal);

	/* FIXME: Pick better co-ords. */
	pointer->x = wl_fixed_from_int(100);
	pointer->y = wl_fixed_from_int(100);
}

WL_EXPORT void
wl_pointer_release(struct wl_pointer *pointer)
{
	/* XXX: What about pointer->resource_list? */
	if (pointer->focus_resource)
		wl_list_remove(&pointer->focus_listener.link);
}

WL_EXPORT void
wl_keyboard_init(struct wl_keyboard *keyboard)
{
	memset(keyboard, 0, sizeof *keyboard);
	wl_list_init(&keyboard->resource_list);
	wl_array_init(&keyboard->keys);
	keyboard->focus_listener.notify = lose_keyboard_focus;
	keyboard->default_grab.interface = &default_keyboard_grab_interface;
	keyboard->default_grab.keyboard = keyboard;
	keyboard->grab = &keyboard->default_grab;
	wl_signal_init(&keyboard->focus_signal);
}

WL_EXPORT void
wl_keyboard_release(struct wl_keyboard *keyboard)
{
	/* XXX: What about keyboard->resource_list? */
	if (keyboard->focus_resource)
		wl_list_remove(&keyboard->focus_listener.link);
	wl_array_release(&keyboard->keys);
}

WL_EXPORT void
wl_touch_init(struct wl_touch *touch)
{
	memset(touch, 0, sizeof *touch);
	wl_list_init(&touch->resource_list);
	touch->focus_listener.notify = lose_touch_focus;
	touch->default_grab.interface = &default_touch_grab_interface;
	touch->default_grab.touch = touch;
	touch->grab = &touch->default_grab;
	wl_signal_init(&touch->focus_signal);
}

WL_EXPORT void
wl_touch_release(struct wl_touch *touch)
{
	/* XXX: What about touch->resource_list? */
	if (touch->focus_resource)
		wl_list_remove(&touch->focus_listener.link);
}

WL_EXPORT void
wl_seat_init(struct wl_seat *seat)
{
	memset(seat, 0, sizeof *seat);

	wl_signal_init(&seat->destroy_signal);

	seat->selection_data_source = NULL;
	wl_list_init(&seat->base_resource_list);
	wl_signal_init(&seat->selection_signal);
	wl_list_init(&seat->drag_resource_list);
	wl_signal_init(&seat->drag_icon_signal);
}

WL_EXPORT void
wl_seat_release(struct wl_seat *seat)
{
	wl_signal_emit(&seat->destroy_signal, seat);

	if (seat->pointer)
		wl_pointer_release(seat->pointer);
	if (seat->keyboard)
		wl_keyboard_release(seat->keyboard);
	if (seat->touch)
		wl_touch_release(seat->touch);
}

static void
seat_send_updated_caps(struct wl_seat *seat)
{
	struct wl_resource *r;
	enum wl_seat_capability caps = 0;

	if (seat->pointer)
		caps |= WL_SEAT_CAPABILITY_POINTER;
	if (seat->keyboard)
		caps |= WL_SEAT_CAPABILITY_KEYBOARD;
	if (seat->touch)
		caps |= WL_SEAT_CAPABILITY_TOUCH;

	wl_list_for_each(r, &seat->base_resource_list, link)
		wl_seat_send_capabilities(r, caps);
}

WL_EXPORT void
wl_seat_set_pointer(struct wl_seat *seat, struct wl_pointer *pointer)
{
	if (pointer && (seat->pointer || pointer->seat))
		return; /* XXX: error? */
	if (!pointer && !seat->pointer)
		return;

	seat->pointer = pointer;
	if (pointer)
		pointer->seat = seat;

	seat_send_updated_caps(seat);
}

WL_EXPORT void
wl_seat_set_keyboard(struct wl_seat *seat, struct wl_keyboard *keyboard)
{
	if (keyboard && (seat->keyboard || keyboard->seat))
		return; /* XXX: error? */
	if (!keyboard && !seat->keyboard)
		return;

	seat->keyboard = keyboard;
	if (keyboard)
		keyboard->seat = seat;

	seat_send_updated_caps(seat);
}

WL_EXPORT void
wl_seat_set_touch(struct wl_seat *seat, struct wl_touch *touch)
{
	if (touch && (seat->touch || touch->seat))
		return; /* XXX: error? */
	if (!touch && !seat->touch)
		return;

	seat->touch = touch;
	if (touch)
		touch->seat = seat;

	seat_send_updated_caps(seat);
}

WL_EXPORT void
wl_pointer_set_focus(struct wl_pointer *pointer, struct wl_surface *surface,
		     wl_fixed_t sx, wl_fixed_t sy)
{
	struct wl_keyboard *kbd = pointer->seat->keyboard;
	struct wl_resource *resource, *kr;
	uint32_t serial;

	resource = pointer->focus_resource;
	if (resource && pointer->focus != surface) {
		serial = wl_display_next_serial(resource->client->display);
		wl_pointer_send_leave(resource, serial,
				      &pointer->focus->resource);
		wl_list_remove(&pointer->focus_listener.link);
	}

	resource = find_resource_for_surface(&pointer->resource_list,
					     surface);
	if (resource &&
	    (pointer->focus != surface ||
	     pointer->focus_resource != resource)) {
		serial = wl_display_next_serial(resource->client->display);
		if (kbd) {
			kr = find_resource_for_surface(&kbd->resource_list,
						       surface);
			if (kr) {
				wl_keyboard_send_modifiers(kr,
							   serial,
							   kbd->modifiers.mods_depressed,
							   kbd->modifiers.mods_latched,
							   kbd->modifiers.mods_locked,
							   kbd->modifiers.group);
			}
		}
		wl_pointer_send_enter(resource, serial, &surface->resource,
				      sx, sy);
		wl_signal_add(&resource->destroy_signal,
			      &pointer->focus_listener);
		pointer->focus_serial = serial;
	}

	pointer->focus_resource = resource;
	pointer->focus = surface;
	pointer->default_grab.focus = surface;
	wl_signal_emit(&pointer->focus_signal, pointer);
}

WL_EXPORT void
wl_keyboard_set_focus(struct wl_keyboard *keyboard, struct wl_surface *surface)
{
	struct wl_resource *resource;
	uint32_t serial;

	if (keyboard->focus_resource && keyboard->focus != surface) {
		resource = keyboard->focus_resource;
		serial = wl_display_next_serial(resource->client->display);
		wl_keyboard_send_leave(resource, serial,
				       &keyboard->focus->resource);
		wl_list_remove(&keyboard->focus_listener.link);
	}

	resource = find_resource_for_surface(&keyboard->resource_list,
					     surface);
	if (resource &&
	    (keyboard->focus != surface ||
	     keyboard->focus_resource != resource)) {
		serial = wl_display_next_serial(resource->client->display);
		wl_keyboard_send_modifiers(resource, serial,
					   keyboard->modifiers.mods_depressed,
					   keyboard->modifiers.mods_latched,
					   keyboard->modifiers.mods_locked,
					   keyboard->modifiers.group);
		wl_keyboard_send_enter(resource, serial, &surface->resource,
				       &keyboard->keys);
		wl_signal_add(&resource->destroy_signal,
			      &keyboard->focus_listener);
		keyboard->focus_serial = serial;
	}

	keyboard->focus_resource = resource;
	keyboard->focus = surface;
	wl_signal_emit(&keyboard->focus_signal, keyboard);
}

WL_EXPORT void
wl_keyboard_start_grab(struct wl_keyboard *keyboard,
		       struct wl_keyboard_grab *grab)
{
	keyboard->grab = grab;
	grab->keyboard = keyboard;

	/* XXX focus? */
}

WL_EXPORT void
wl_keyboard_end_grab(struct wl_keyboard *keyboard)
{
	keyboard->grab = &keyboard->default_grab;
}

WL_EXPORT void
wl_pointer_start_grab(struct wl_pointer *pointer, struct wl_pointer_grab *grab)
{
	const struct wl_pointer_grab_interface *interface;

	pointer->grab = grab;
	interface = pointer->grab->interface;
	grab->pointer = pointer;

	if (pointer->current)
		interface->focus(pointer->grab, pointer->current,
				 pointer->current_x, pointer->current_y);
}

WL_EXPORT void
wl_pointer_end_grab(struct wl_pointer *pointer)
{
	const struct wl_pointer_grab_interface *interface;

	pointer->grab = &pointer->default_grab;
	interface = pointer->grab->interface;
	interface->focus(pointer->grab, pointer->current,
			 pointer->current_x, pointer->current_y);
}

WL_EXPORT void
wl_touch_start_grab(struct wl_touch *touch, struct wl_touch_grab *grab)
{
	touch->grab = grab;
	grab->touch = touch;
}

WL_EXPORT void
wl_touch_end_grab(struct wl_touch *touch)
{
	touch->grab = &touch->default_grab;
}

static void
registry_bind(struct wl_client *client,
	      struct wl_resource *resource, uint32_t name,
	      const char *interface, uint32_t version, uint32_t id)
{
	struct wl_global *global;
	struct wl_display *display = resource->data;

	wl_list_for_each(global, &display->global_list, link)
		if (global->name == name)
			break;

	if (&global->link == &display->global_list)
		wl_resource_post_error(resource,
				       WL_DISPLAY_ERROR_INVALID_OBJECT,
				       "invalid global %d", name);
	else
		global->bind(client, global->data, version, id);
}

static const struct wl_registry_interface registry_interface = {
	registry_bind
};

static void
display_sync(struct wl_client *client,
	     struct wl_resource *resource, uint32_t id)
{
	struct wl_resource *callback;
	uint32_t serial;

	callback = wl_client_add_object(client,
					&wl_callback_interface, NULL, id, NULL);
	serial = wl_display_get_serial(client->display);
	wl_callback_send_done(callback, serial);
	wl_resource_destroy(callback);
}

static void
unbind_resource(struct wl_resource *resource)
{
	wl_list_remove(&resource->link);
	free(resource);
}

static void
display_get_registry(struct wl_client *client,
		     struct wl_resource *resource, uint32_t id)
{
	struct wl_display *display = resource->data;
	struct wl_resource *registry_resource;
	struct wl_global *global;

	registry_resource =
		wl_client_add_object(client, &wl_registry_interface,
				     &registry_interface, id, display);
	registry_resource->destroy = unbind_resource;

	wl_list_insert(&display->registry_resource_list,
		       &registry_resource->link);

	wl_list_for_each(global, &display->global_list, link)
		wl_resource_post_event(registry_resource,
				       WL_REGISTRY_GLOBAL,
				       global->name,
				       global->interface->name,
				       global->interface->version);
}

static const struct wl_display_interface display_interface = {
	display_sync,
	display_get_registry
};

static void
destroy_client_display_resource(struct wl_resource *resource)
{
	resource->client->display_resource = NULL;
	free(resource);
}

static void
bind_display(struct wl_client *client,
	     void *data, uint32_t version, uint32_t id)
{
	struct wl_display *display = data;

	client->display_resource =
		wl_client_add_object(client, &wl_display_interface,
				     &display_interface, id, display);

	if(client->display_resource)
		client->display_resource->destroy = destroy_client_display_resource;
}

WL_EXPORT struct wl_display *
wl_display_create(void)
{
	struct wl_display *display;
	const char *debug;

	debug = getenv("WAYLAND_DEBUG");
	if (debug && (strstr(debug, "server") || strstr(debug, "1")))
		wl_debug = 1;

	display = malloc(sizeof *display);
	if (display == NULL)
		return NULL;

	display->loop = wl_event_loop_create();
	if (display->loop == NULL) {
		free(display);
		return NULL;
	}

	wl_list_init(&display->global_list);
	wl_list_init(&display->socket_list);
	wl_list_init(&display->client_list);
	wl_list_init(&display->registry_resource_list);

	wl_signal_init(&display->destroy_signal);

	display->id = 1;
	display->serial = 0;

	if (!wl_display_add_global(display, &wl_display_interface, 
				   display, bind_display)) {
		wl_event_loop_destroy(display->loop);
		free(display);
		return NULL;
	}

	return display;
}

WL_EXPORT void
wl_display_destroy(struct wl_display *display)
{
	struct wl_socket *s, *next;
	struct wl_global *global, *gnext;

	wl_signal_emit(&display->destroy_signal, display);

	wl_list_for_each_safe(s, next, &display->socket_list, link) {
		wl_event_source_remove(s->source);
		unlink(s->addr.sun_path);
		close(s->fd);
		unlink(s->lock_addr);
		close(s->fd_lock);
		free(s);
	}
	wl_event_loop_destroy(display->loop);

	wl_list_for_each_safe(global, gnext, &display->global_list, link)
		free(global);

	free(display);
}

WL_EXPORT struct wl_global *
wl_display_add_global(struct wl_display *display,
		      const struct wl_interface *interface,
		      void *data, wl_global_bind_func_t bind)
{
	struct wl_global *global;
	struct wl_resource *resource;

	global = malloc(sizeof *global);
	if (global == NULL)
		return NULL;

	global->name = display->id++;
	global->interface = interface;
	global->data = data;
	global->bind = bind;
	wl_list_insert(display->global_list.prev, &global->link);

	wl_list_for_each(resource, &display->registry_resource_list, link)
		wl_resource_post_event(resource,
				       WL_REGISTRY_GLOBAL,
				       global->name,
				       global->interface->name,
				       global->interface->version);

	return global;
}

WL_EXPORT void
wl_display_remove_global(struct wl_display *display, struct wl_global *global)
{
	struct wl_resource *resource;

	wl_list_for_each(resource, &display->registry_resource_list, link)
		wl_resource_post_event(resource, WL_REGISTRY_GLOBAL_REMOVE,
				       global->name);
	wl_list_remove(&global->link);
	free(global);
}

WL_EXPORT uint32_t
wl_display_get_serial(struct wl_display *display)
{
	return display->serial;
}

WL_EXPORT uint32_t
wl_display_next_serial(struct wl_display *display)
{
	display->serial++;

	return display->serial;
}

WL_EXPORT struct wl_event_loop *
wl_display_get_event_loop(struct wl_display *display)
{
	return display->loop;
}

WL_EXPORT void
wl_display_terminate(struct wl_display *display)
{
	display->run = 0;
}

WL_EXPORT void
wl_display_run(struct wl_display *display)
{
	display->run = 1;

	while (display->run) {
		wl_display_flush_clients(display);
		wl_event_loop_dispatch(display->loop, -1);
	}
}

WL_EXPORT void
wl_display_flush_clients(struct wl_display *display)
{
	struct wl_client *client, *next;
	int ret;

	wl_list_for_each_safe(client, next, &display->client_list, link) {
		ret = wl_connection_flush(client->connection);
		if (ret < 0 && errno == EAGAIN) {
			wl_event_source_fd_update(client->source,
						  WL_EVENT_WRITABLE |
						  WL_EVENT_READABLE);
		} else if (ret < 0) {
			wl_client_destroy(client);
		}
	}
}

static int
socket_data(int fd, uint32_t mask, void *data)
{
	struct wl_display *display = data;
	struct sockaddr_un name;
	socklen_t length;
	int client_fd;

	length = sizeof name;
	client_fd = wl_os_accept_cloexec(fd, (struct sockaddr *) &name,
					 &length);
	if (client_fd < 0)
		wl_log("failed to accept: %m\n");
	else
		if (!wl_client_create(display, client_fd))
			close(client_fd);

	return 1;
}

static int
get_socket_lock(struct wl_socket *socket)
{
	struct stat socket_stat;
	int fd_lock;

	snprintf(socket->lock_addr, sizeof socket->lock_addr,
		 "%s%s", socket->addr.sun_path, LOCK_SUFFIX);

	fd_lock = open(socket->lock_addr, O_CREAT | O_CLOEXEC,
	               (S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP));

	if (fd_lock < 0) {
		wl_log("unable to open lockfile %s check permissions\n",
			socket->lock_addr);
		return -1;
	}

	if (flock(fd_lock, LOCK_EX | LOCK_NB) < 0) {
		wl_log("unable to lock lockfile %s, maybe another compositor is running\n",
			socket->lock_addr);
		close(fd_lock);
		return -1;
	}

	if (stat(socket->addr.sun_path, &socket_stat) < 0 ) {
		if (errno != ENOENT) {
			wl_log("did not manage to stat file %s\n",
				socket->addr.sun_path);
			close(fd_lock);
			return -1;
		}
	} else if (socket_stat.st_mode & S_IWUSR ||
		   socket_stat.st_mode & S_IWGRP) {
		unlink(socket->addr.sun_path);
	}

	return fd_lock;
}

WL_EXPORT int
wl_display_add_socket(struct wl_display *display, const char *name)
{
	struct wl_socket *s;
	socklen_t size;
	int name_size;
	const char *runtime_dir;

	runtime_dir = getenv("XDG_RUNTIME_DIR");
	if (!runtime_dir) {
		wl_log("error: XDG_RUNTIME_DIR not set in the environment\n");

		/* to prevent programs reporting
		 * "failed to add socket: Success" */
		errno = ENOENT;
		return -1;
	}

	s = malloc(sizeof *s);
	if (s == NULL)
		return -1;

	s->fd = wl_os_socket_cloexec(PF_LOCAL, SOCK_STREAM, 0);
	if (s->fd < 0) {
		free(s);
		return -1;
	}

	if (name == NULL)
		name = getenv("WAYLAND_DISPLAY");
	if (name == NULL)
		name = "wayland-0";

	memset(&s->addr, 0, sizeof s->addr);
	s->addr.sun_family = AF_LOCAL;
	name_size = snprintf(s->addr.sun_path, sizeof s->addr.sun_path,
			     "%s/%s", runtime_dir, name) + 1;

	assert(name_size > 0);
	if (name_size > (int)sizeof s->addr.sun_path) {
		wl_log("error: socket path \"%s/%s\" plus null terminator"
		       " exceeds 108 bytes\n", runtime_dir, name);
		close(s->fd);
		free(s);
		/* to prevent programs reporting
		 * "failed to add socket: Success" */
		errno = ENAMETOOLONG;
		return -1;
	};

	wl_log("using socket %s\n", s->addr.sun_path);

	s->fd_lock = get_socket_lock(s);
	if (s->fd_lock < 0) {
		close(s->fd);
		free(s);
		return -1;
	}

	size = offsetof (struct sockaddr_un, sun_path) + name_size;
	if (bind(s->fd, (struct sockaddr *) &s->addr, size) < 0) {
		wl_log("bind() failed with error: %m\n");
		close(s->fd);
		unlink(s->lock_addr);
		close(s->fd_lock);
		free(s);
		return -1;
	}

	if (listen(s->fd, 1) < 0) {
		wl_log("listen() failed with error: %m\n");
		unlink(s->addr.sun_path);
		close(s->fd);
		unlink(s->lock_addr);
		close(s->fd_lock);
		free(s);
		return -1;
	}

	s->source = wl_event_loop_add_fd(display->loop, s->fd,
					 WL_EVENT_READABLE,
					 socket_data, display);
	if (s->source == NULL) {
		unlink(s->addr.sun_path);
		close(s->fd);
		unlink(s->lock_addr);
		close(s->fd_lock);
		free(s);
		return -1;
	}
	wl_list_insert(display->socket_list.prev, &s->link);

	return 0;
}

WL_EXPORT void
wl_display_add_destroy_listener(struct wl_display *display,
				struct wl_listener *listener)
{
	wl_signal_add(&display->destroy_signal, listener);
}

WL_EXPORT struct wl_listener *
wl_display_get_destroy_listener(struct wl_display *display,
				wl_notify_func_t notify)
{
	return wl_signal_get(&display->destroy_signal, notify);
}

WL_EXPORT struct wl_resource *
wl_client_add_object(struct wl_client *client,
		     const struct wl_interface *interface,
		     const void *implementation,
		     uint32_t id, void *data)
{
	struct wl_resource *resource;

	resource = malloc(sizeof *resource);
	if (resource == NULL) {
		wl_resource_post_no_memory(client->display_resource);
		return NULL;
	}

	resource->object.interface = interface;
	resource->object.implementation = implementation;
	resource->object.id = id;
	resource->client = client;
	resource->data = data;
	resource->destroy = (void *) free;
	wl_signal_init(&resource->destroy_signal);

	if (wl_map_insert_at(&client->objects, resource->object.id, resource) < 0) {
		wl_resource_post_error(client->display_resource,
				       WL_DISPLAY_ERROR_INVALID_OBJECT,
				       "invalid new id %d",
				       resource->object.id);
		free(resource);
		return NULL;
	}

	return resource;
}

WL_EXPORT struct wl_resource *
wl_client_new_object(struct wl_client *client,
		     const struct wl_interface *interface,
		     const void *implementation, void *data)
{
	uint32_t id;

	id = wl_map_insert_new(&client->objects, WL_MAP_SERVER_SIDE, NULL);
	return wl_client_add_object(client,
				    interface, implementation, id, data);

}

WL_EXPORT void
wl_log_set_handler_server(wl_log_func_t handler)
{
	wl_log_handler = handler;
}
