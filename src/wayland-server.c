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

struct wl_socket {
	int fd;
	int fd_lock;
	struct sockaddr_un addr;
	char lock_addr[113];
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

	struct wl_list global_list;
	struct wl_list socket_list;
	struct wl_list client_list;
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
	struct wl_closure closure;
	struct wl_object *object = &resource->object;
	va_list ap;
	int ret;

	va_start(ap, opcode);
	ret = wl_closure_vmarshal(&closure, object, opcode, ap,
				  &object->interface->events[opcode]);
	va_end(ap);

	if (ret)
		return;

	if (wl_closure_send(&closure, resource->client->connection))
		wl_event_loop_add_idle(resource->client->display->loop,
				       destroy_client, resource->client);

	if (wl_debug)
		wl_closure_print(&closure, object, true);

	wl_closure_destroy(&closure);
}


WL_EXPORT void
wl_resource_queue_event(struct wl_resource *resource, uint32_t opcode, ...)
{
	struct wl_closure closure;
	struct wl_object *object = &resource->object;
	va_list ap;
	int ret;

	va_start(ap, opcode);
	ret = wl_closure_vmarshal(&closure, object, opcode, ap,
				  &object->interface->events[opcode]);
	va_end(ap);

	if (ret)
		return;

	if (wl_closure_queue(&closure, resource->client->connection))
		wl_event_loop_add_idle(resource->client->display->loop,
				       destroy_client, resource->client);

	if (wl_debug)
		wl_closure_print(&closure, object, true);

	wl_closure_destroy(&closure);
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
	struct wl_closure closure;
	const struct wl_message *message;
	uint32_t p[2];
	int opcode, size;
	uint32_t cmask = 0;
	int len, ret;

	if (mask & WL_EVENT_READABLE)
		cmask |= WL_CONNECTION_READABLE;
	if (mask & WL_EVENT_WRITABLE)
		cmask |= WL_CONNECTION_WRITABLE;

	len = wl_connection_data(connection, cmask);
	if (len < 0) {
		wl_client_destroy(client);
		return 1;
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
					       "invalid object %d", p[0]);
			break;
		}

		object = &resource->object;
		if (opcode >= object->interface->method_count) {
			wl_resource_post_error(client->display_resource,
					       WL_DISPLAY_ERROR_INVALID_METHOD,
					       "invalid method %d, object %s@%d",
					       opcode,
					       object->interface->name,
					       object->id);
			break;
		}

		message = &object->interface->methods[opcode];
		ret = wl_connection_demarshal(client->connection, &closure,
					      size, &client->objects, message);
		len -= size;

		if (ret && errno == EINVAL) {
			wl_resource_post_error(client->display_resource,
					       WL_DISPLAY_ERROR_INVALID_METHOD,
					       "invalid arguments for %s@%d.%s",
					       object->interface->name,
					       object->id,
					       message->name);
			break;
		} else if (ret && errno == ENOMEM) {
			wl_resource_post_no_memory(resource);
			break;
		}

		if (wl_debug)
			wl_closure_print(&closure, object, false);

		wl_closure_invoke(&closure, object,
				  object->implementation[opcode], client);

		wl_closure_destroy(&closure);

		if (client->error)
			break;
	}

	if (client->error)
		wl_client_destroy(client);

	return 1;
}

static int
wl_client_connection_update(struct wl_connection *connection,
			    uint32_t mask, void *data)
{
	struct wl_client *client = data;
	uint32_t emask = 0;

	client->mask = mask;
	if (mask & WL_CONNECTION_READABLE)
		emask |= WL_EVENT_READABLE;
	if (mask & WL_CONNECTION_WRITABLE)
		emask |= WL_EVENT_WRITABLE;

	return wl_event_source_fd_update(client->source, emask);
}

WL_EXPORT void
wl_client_flush(struct wl_client *client)
{
	if (client->mask & WL_CONNECTION_WRITABLE)
		wl_connection_data(client->connection, WL_CONNECTION_WRITABLE);
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

	len = sizeof client->ucred;
	if (getsockopt(fd, SOL_SOCKET, SO_PEERCRED,
		       &client->ucred, &len) < 0) {
		free(client);
		return NULL;
	}

	client->connection =
		wl_connection_create(fd, wl_client_connection_update, client);
	if (client->connection == NULL) {
		free(client);
		return NULL;
	}

	wl_map_init(&client->objects);

	if (wl_map_insert_at(&client->objects, 0, NULL) < 0) {
		wl_map_release(&client->objects);
		free(client);
		return NULL;
	}

	wl_signal_init(&client->destroy_signal);
	bind_display(client, display, 1, 1);

	wl_list_insert(display->client_list.prev, &client->link);

	return client;
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

WL_EXPORT void
wl_client_add_resource(struct wl_client *client,
		       struct wl_resource *resource)
{
	if (resource->object.id == 0)
		resource->object.id =
			wl_map_insert_new(&client->objects,
					  WL_MAP_SERVER_SIDE, resource);
	else
		wl_map_insert_at(&client->objects,
				 resource->object.id, resource);

	resource->client = client;
	wl_signal_init(&resource->destroy_signal);
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
	
	printf("disconnect from client %p\n", client);

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
	struct wl_input_device *device =
		container_of(listener, struct wl_input_device,
			     pointer_focus_listener);

	device->pointer_focus_resource = NULL;
}

static void
lose_keyboard_focus(struct wl_listener *listener, void *data)
{
	struct wl_input_device *device =
		container_of(listener, struct wl_input_device,
			     keyboard_focus_listener);

	device->keyboard_focus_resource = NULL;
}

static void
default_grab_focus(struct wl_pointer_grab *grab,
		   struct wl_surface *surface, int32_t x, int32_t y)
{
	struct wl_input_device *device = grab->input_device;

	if (device->button_count > 0)
		return;

	wl_input_device_set_pointer_focus(device, surface, x, y);
}

static void
default_grab_motion(struct wl_pointer_grab *grab,
		    uint32_t time, int32_t x, int32_t y)
{
	struct wl_resource *resource;

	resource = grab->input_device->pointer_focus_resource;
	if (resource)
		wl_input_device_send_motion(resource, time, x, y);
}

static void
default_grab_button(struct wl_pointer_grab *grab,
		    uint32_t time, uint32_t button, uint32_t state)
{
	struct wl_input_device *device = grab->input_device;
	struct wl_resource *resource;
	uint32_t serial;

	resource = device->pointer_focus_resource;
	if (resource) {
		serial = wl_display_next_serial(resource->client->display);
		wl_input_device_send_button(resource, serial, time,
					    button, state);
	}

	if (device->button_count == 0 && state == 0)
		wl_input_device_set_pointer_focus(device,
						  device->current,
						  device->current_x,
						  device->current_y);
}

static const struct wl_pointer_grab_interface
				default_pointer_grab_interface = {
	default_grab_focus,
	default_grab_motion,
	default_grab_button
};

static void
default_grab_key(struct wl_keyboard_grab *grab,
		 uint32_t time, uint32_t key, uint32_t state)
{
	struct wl_input_device *device = grab->input_device;
	struct wl_resource *resource;
	uint32_t serial;

	resource = device->keyboard_focus_resource;
	if (resource) {
		serial = wl_display_next_serial(resource->client->display);
		wl_input_device_send_key(resource, serial, time, key, state);
	}
}

static const struct wl_keyboard_grab_interface
				default_keyboard_grab_interface = {
	default_grab_key
};

WL_EXPORT void
wl_input_device_init(struct wl_input_device *device)
{
	memset(device, 0, sizeof *device);
	wl_list_init(&device->resource_list);
	wl_array_init(&device->keys);
	device->pointer_focus_listener.notify = lose_pointer_focus;
	device->keyboard_focus_listener.notify = lose_keyboard_focus;

	device->default_pointer_grab.interface = &default_pointer_grab_interface;
	device->default_pointer_grab.input_device = device;
	device->pointer_grab = &device->default_pointer_grab;

	device->default_keyboard_grab.interface = &default_keyboard_grab_interface;
	device->default_keyboard_grab.input_device = device;
	device->keyboard_grab = &device->default_keyboard_grab;

	wl_list_init(&device->drag_resource_list);
	device->selection_data_source = NULL;
	wl_signal_init(&device->selection_signal);
	wl_signal_init(&device->drag_icon_signal);

	device->x = 100;
	device->y = 100;
}

WL_EXPORT void
wl_input_device_release(struct wl_input_device *device)
{
	if (device->keyboard_focus_resource)
		wl_list_remove(&device->keyboard_focus_listener.link);

	if (device->pointer_focus_resource)
		wl_list_remove(&device->pointer_focus_listener.link);

	/* XXX: What about device->resource_list? */

	wl_array_release(&device->keys);
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

WL_EXPORT void
wl_input_device_set_pointer_focus(struct wl_input_device *device,
				  struct wl_surface *surface,
				  int32_t sx, int32_t sy)
{
	struct wl_resource *resource;
	uint32_t serial;

	if (device->pointer_focus == surface)
		return;

	resource = device->pointer_focus_resource;
	if (resource) {
		serial = wl_display_next_serial(resource->client->display);
		wl_input_device_send_pointer_leave(resource, serial,
			&device->pointer_focus->resource);
		wl_list_remove(&device->pointer_focus_listener.link);
	}


	resource = find_resource_for_surface(&device->resource_list, surface);
	if (resource) {
		serial = wl_display_next_serial(resource->client->display);
		wl_input_device_send_pointer_enter(resource, serial,
						   &surface->resource,
						   sx, sy);
		wl_signal_add(&resource->destroy_signal,
			      &device->pointer_focus_listener);
		device->pointer_focus_serial = serial;
	}

	device->pointer_focus_resource = resource;
	device->pointer_focus = surface;
	device->default_pointer_grab.focus = surface;
}

WL_EXPORT void
wl_input_device_set_keyboard_focus(struct wl_input_device *device,
				   struct wl_surface *surface)
{
	struct wl_resource *resource;
	uint32_t serial;

	if (device->keyboard_focus == surface)
		return;

	if (device->keyboard_focus_resource) {
		resource = device->keyboard_focus_resource;
		serial = wl_display_next_serial(resource->client->display);
		wl_input_device_send_keyboard_leave(resource, serial,
						    &device->keyboard_focus->resource);
		wl_list_remove(&device->keyboard_focus_listener.link);
	}

	resource = find_resource_for_surface(&device->resource_list, surface);
	if (resource) {
		serial = wl_display_next_serial(resource->client->display);
		wl_input_device_send_keyboard_enter(resource, serial,
						    &surface->resource,
						    &device->keys);
		wl_signal_add(&resource->destroy_signal,
			      &device->keyboard_focus_listener);
		device->keyboard_focus_serial = serial;
	}

	device->keyboard_focus_resource = resource;
	device->keyboard_focus = surface;
}

WL_EXPORT void
wl_input_device_start_keyboard_grab(struct wl_input_device *device,
				    struct wl_keyboard_grab *grab)
{
	device->keyboard_grab = grab;
	grab->input_device = device;

}

WL_EXPORT void
wl_input_device_end_keyboard_grab(struct wl_input_device *device)
{
	device->keyboard_grab = &device->default_keyboard_grab;
}

WL_EXPORT void
wl_input_device_start_pointer_grab(struct wl_input_device *device,
				   struct wl_pointer_grab *grab)
{
	const struct wl_pointer_grab_interface *interface;

	device->pointer_grab = grab;
	interface = device->pointer_grab->interface;
	grab->input_device = device;

	if (device->current)
		interface->focus(device->pointer_grab, device->current,
				 device->current_x, device->current_y);
}

WL_EXPORT void
wl_input_device_end_pointer_grab(struct wl_input_device *device)
{
	const struct wl_pointer_grab_interface *interface;

	device->pointer_grab = &device->default_pointer_grab;
	interface = device->pointer_grab->interface;
	interface->focus(device->pointer_grab, device->current,
			 device->current_x, device->current_y);
}

static void
display_bind(struct wl_client *client,
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

struct wl_display_interface display_interface = {
	display_bind,
	display_sync,
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
	struct wl_global *global;

	client->display_resource =
		wl_client_add_object(client, &wl_display_interface,
				     &display_interface, id, display);
	client->display_resource->destroy = destroy_client_display_resource;

	wl_list_for_each(global, &display->global_list, link)
		wl_resource_post_event(client->display_resource,
				       WL_DISPLAY_GLOBAL,
				       global->name,
				       global->interface->name,
				       global->interface->version);
}

WL_EXPORT struct wl_display *
wl_display_create(void)
{
	struct wl_display *display;
	const char *debug;

	debug = getenv("WAYLAND_DEBUG");
	if (debug)
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

	display->id = 1;

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

	wl_list_for_each_safe(s, next, &display->socket_list, link) {
		wl_event_source_remove(s->source);
		close(s->fd);
		unlink(s->addr.sun_path);
		close(s->fd_lock);
		unlink(s->lock_addr);
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
	struct wl_client *client;

	global = malloc(sizeof *global);
	if (global == NULL)
		return NULL;

	global->name = display->id++;
	global->interface = interface;
	global->data = data;
	global->bind = bind;
	wl_list_insert(display->global_list.prev, &global->link);

	wl_list_for_each(client, &display->client_list, link)
		wl_resource_post_event(client->display_resource,
				       WL_DISPLAY_GLOBAL,
				       global->name,
				       global->interface->name,
				       global->interface->version);

	return global;
}

WL_EXPORT void
wl_display_remove_global(struct wl_display *display, struct wl_global *global)
{
	struct wl_client *client;

	wl_list_for_each(client, &display->client_list, link)
		wl_resource_post_event(client->display_resource,
				       WL_DISPLAY_GLOBAL_REMOVE, global->name);
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

	while (display->run)
		wl_event_loop_dispatch(display->loop, -1);
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
		fprintf(stderr, "failed to accept, errno: %d\n", errno);
	else
		wl_client_create(display, client_fd);

	return 1;
}

static int
get_socket_lock(struct wl_socket *socket, socklen_t name_size)
{
	struct stat socket_stat;
	int lock_size = name_size + 5;

	snprintf(socket->lock_addr, lock_size,
		 "%s.lock", socket->addr.sun_path);

	socket->fd_lock = open(socket->lock_addr, O_CREAT | O_CLOEXEC,
			       (S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP));

	if (socket->fd_lock < 0) {
		fprintf(stderr,
			"unable to open lockfile %s check permissions\n",
			socket->lock_addr);
		return -1;
	}

	if (flock(socket->fd_lock, LOCK_EX | LOCK_NB) < 0) {
		fprintf(stderr,
			"unable to lock lockfile %s, maybe another compositor is running\n",
			socket->lock_addr);
		close(socket->fd_lock);
		return -1;
	}

	if (stat(socket->addr.sun_path, &socket_stat) < 0 ) {
		if (errno != ENOENT) {
			fprintf(stderr, "did not manage to stat file %s\n",
				socket->addr.sun_path);
			close(socket->fd_lock);
			return -1;
		}
	} else if (socket_stat.st_mode & S_IWUSR ||
		   socket_stat.st_mode & S_IWGRP) {
		unlink(socket->addr.sun_path);
	}

	return 0;
}

WL_EXPORT int
wl_display_add_socket(struct wl_display *display, const char *name)
{
	struct wl_socket *s;
	socklen_t size, name_size;
	const char *runtime_dir;

	s = malloc(sizeof *s);
	if (s == NULL)
		return -1;

	s->fd = wl_os_socket_cloexec(PF_LOCAL, SOCK_STREAM, 0);
	if (s->fd < 0) {
		free(s);
		return -1;
	}

	runtime_dir = getenv("XDG_RUNTIME_DIR");
	if (runtime_dir == NULL) {
		runtime_dir = ".";
		fprintf(stderr,
			"XDG_RUNTIME_DIR not set, falling back to %s\n",
			runtime_dir);
	}

	if (name == NULL)
		name = getenv("WAYLAND_DISPLAY");
	if (name == NULL)
		name = "wayland-0";

	memset(&s->addr, 0, sizeof s->addr);
	s->addr.sun_family = AF_LOCAL;
	name_size = snprintf(s->addr.sun_path, sizeof s->addr.sun_path,
			     "%s/%s", runtime_dir, name) + 1;
	fprintf(stderr, "using socket %s\n", s->addr.sun_path);

	if (get_socket_lock(s,name_size) < 0) {
		close(s->fd);
		free(s);
		return -1;
	}

	size = offsetof (struct sockaddr_un, sun_path) + name_size;
	if (bind(s->fd, (struct sockaddr *) &s->addr, size) < 0) {
		close(s->fd);
		free(s);
		return -1;
	}

	if (listen(s->fd, 1) < 0) {
		close(s->fd);
		unlink(s->addr.sun_path);
		free(s);
		return -1;
	}

	s->source = wl_event_loop_add_fd(display->loop, s->fd,
					 WL_EVENT_READABLE,
					 socket_data, display);
	if (s->source == NULL) {
		close(s->fd);
		unlink(s->addr.sun_path);
		free(s);
		return -1;
	}
	wl_list_insert(display->socket_list.prev, &s->link);

	return 0;
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
		wl_resource_post_no_memory(client->display_resource);
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
