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

#include "wayland-server.h"
#include "wayland-server-protocol.h"
#include "connection.h"

struct wl_socket {
	int fd;
	int fd_lock;
	struct sockaddr_un addr;
	char lock_addr[113];
	struct wl_list link;
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
	int error;
};

struct wl_display {
	struct wl_event_loop *loop;
	int run;

	struct wl_list callback_list;
	uint32_t id;

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

WL_EXPORT void
wl_resource_post_event(struct wl_resource *resource, uint32_t opcode, ...)
{
	struct wl_closure *closure;
	struct wl_object *object = &resource->object;
	va_list ap;

	va_start(ap, opcode);
	closure = wl_connection_vmarshal(resource->client->connection,
					 object, opcode, ap,
					 &object->interface->events[opcode]);
	va_end(ap);

	wl_closure_send(closure, resource->client->connection);

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
	closure = wl_connection_vmarshal(resource->client->connection,
					 object, opcode, ap,
					 &object->interface->events[opcode]);
	va_end(ap);

	wl_closure_queue(closure, resource->client->connection);

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
	uint32_t p[2], opcode, size;
	uint32_t cmask = 0;
	int len;

	if (mask & WL_EVENT_READABLE)
		cmask |= WL_CONNECTION_READABLE;
	if (mask & WL_EVENT_WRITEABLE)
		cmask |= WL_CONNECTION_WRITABLE;

	len = wl_connection_data(connection, cmask);
	if (len < 0) {
		wl_client_destroy(client);
		return 1;
	}

	while (len >= sizeof p) {
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
					       object->interface->name,
					       object->id, opcode);
			break;
		}

		message = &object->interface->methods[opcode];
		closure = wl_connection_demarshal(client->connection, size,
						  &client->objects, message);
		len -= size;

		if (closure == NULL && errno == EINVAL) {
			wl_resource_post_error(client->display_resource,
					       WL_DISPLAY_ERROR_INVALID_METHOD,
					       "invalid arguments for %s@%d.%s",
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
		emask |= WL_EVENT_WRITEABLE;

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

	client = malloc(sizeof *client);
	if (client == NULL)
		return NULL;

	memset(client, 0, sizeof *client);
	client->display = display;
	client->source = wl_event_loop_add_fd(display->loop, fd,
					      WL_EVENT_READABLE,
					      wl_client_connection_data, client);
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

	bind_display(client, display, 1, 1);

	wl_list_insert(display->client_list.prev, &client->link);

	return client;
}

WL_EXPORT void
wl_client_add_resource(struct wl_client *client,
		       struct wl_resource *resource)
{
	resource->client = client;
	wl_list_init(&resource->destroy_listener_list);
	wl_map_insert_at(&client->objects, resource->object.id, resource);
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
 	struct wl_listener *l, *next;
	uint32_t *time = data;

	wl_list_for_each_safe(l, next, &resource->destroy_listener_list, link)
		l->func(l, resource, *time);

	if (resource->destroy)
		resource->destroy(resource);
}

WL_EXPORT void
wl_resource_destroy(struct wl_resource *resource, uint32_t time)
{
	struct wl_client *client = resource->client;

	wl_resource_queue_event(resource->client->display_resource,
				WL_DISPLAY_DELETE_ID, resource->object.id);
	wl_map_insert_at(&client->objects, resource->object.id, NULL);
	destroy_resource(resource, &time);
}

WL_EXPORT void
wl_client_destroy(struct wl_client *client)
{
	uint32_t time = 0;
	
	printf("disconnect from client %p\n", client);

	wl_client_flush(client);
	wl_map_for_each(&client->objects, destroy_resource, &time);
	wl_map_release(&client->objects);
	wl_event_source_remove(client->source);
	wl_connection_destroy(client->connection);
	wl_list_remove(&client->link);
	free(client);
}

static void
lose_pointer_focus(struct wl_listener *listener,
		   struct wl_resource *resource, uint32_t time)
{
	struct wl_input_device *device =
		container_of(listener, struct wl_input_device,
			     pointer_focus_listener);

	device->pointer_focus_resource = NULL;
}

static void
lose_keyboard_focus(struct wl_listener *listener,
		    struct wl_resource *resource, uint32_t time)
{
	struct wl_input_device *device =
		container_of(listener, struct wl_input_device,
			     keyboard_focus_listener);

	device->keyboard_focus_resource = NULL;
}

WL_EXPORT void
wl_input_device_init(struct wl_input_device *device,
		     struct wl_compositor *compositor)
{
	memset(device, 0, sizeof *device);
	wl_list_init(&device->resource_list);
	device->pointer_focus_listener.func = lose_pointer_focus;
	device->keyboard_focus_listener.func = lose_keyboard_focus;

	device->x = 100;
	device->y = 100;
	device->compositor = compositor;
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
				  uint32_t time,
				  int32_t x, int32_t y,
				  int32_t sx, int32_t sy)
{
	struct wl_resource *resource;

	if (device->pointer_focus == surface)
		return;

	if (device->pointer_focus_resource &&
	    (!surface ||
	     device->pointer_focus->resource.client != surface->resource.client))
		wl_resource_post_event(device->pointer_focus_resource,
				       WL_INPUT_DEVICE_POINTER_FOCUS,
				       time, NULL, 0, 0, 0, 0);
	if (device->pointer_focus_resource)
		wl_list_remove(&device->pointer_focus_listener.link);

	resource = find_resource_for_surface(&device->resource_list, surface);
	if (resource) {
		wl_resource_post_event(resource,
				       WL_INPUT_DEVICE_POINTER_FOCUS,
				       time, surface, x, y, sx, sy);
		wl_list_insert(resource->destroy_listener_list.prev,
			       &device->pointer_focus_listener.link);
	}

	device->pointer_focus_resource = resource;
	device->pointer_focus = surface;
	device->pointer_focus_time = time;
}

WL_EXPORT void
wl_input_device_set_keyboard_focus(struct wl_input_device *device,
				   struct wl_surface *surface,
				   uint32_t time)
{
	struct wl_resource *resource;

	if (device->keyboard_focus == surface)
		return;

	if (device->keyboard_focus_resource &&
	    (!surface ||
	     device->keyboard_focus->resource.client != surface->resource.client))
		wl_resource_post_event(device->keyboard_focus_resource,
				       WL_INPUT_DEVICE_KEYBOARD_FOCUS,
				       time, NULL, &device->keys);
	if (device->keyboard_focus_resource)
		wl_list_remove(&device->keyboard_focus_listener.link);

	resource = find_resource_for_surface(&device->resource_list, surface);
	if (resource) {
		wl_resource_post_event(resource,
				       WL_INPUT_DEVICE_KEYBOARD_FOCUS,
				       time, surface, &device->keys);
		wl_list_insert(resource->destroy_listener_list.prev,
			       &device->keyboard_focus_listener.link);
	}

	device->keyboard_focus_resource = resource;
	device->keyboard_focus = surface;
	device->keyboard_focus_time = time;
}

WL_EXPORT void
wl_input_device_end_grab(struct wl_input_device *device, uint32_t time)
{
	const struct wl_grab_interface *interface;

	interface = device->grab->interface;
	interface->end(device->grab, time);
	device->grab = NULL;

	wl_list_remove(&device->grab_listener.link);
}

static void
lose_grab_surface(struct wl_listener *listener,
		  struct wl_resource *resource, uint32_t time)
{
	struct wl_input_device *device =
		container_of(listener,
			     struct wl_input_device, grab_listener);

	wl_input_device_end_grab(device, time);
}

WL_EXPORT void
wl_input_device_start_grab(struct wl_input_device *device,
			   struct wl_grab *grab,
			   uint32_t button, uint32_t time)
{
	struct wl_surface *focus = device->pointer_focus;

	device->grab = grab;
	device->grab_button = button;
	device->grab_time = time;
	device->grab_x = device->x;
	device->grab_y = device->y;

	device->grab_listener.func = lose_grab_surface;
	wl_list_insert(focus->resource.destroy_listener_list.prev,
		       &device->grab_listener.link);

	grab->input_device = device;
}

WL_EXPORT int
wl_input_device_update_grab(struct wl_input_device *device,
			    struct wl_grab *grab,
			    struct wl_surface *surface, uint32_t time)
{
	if (device->grab != &device->implicit_grab ||
	    device->grab_time != time ||
	    device->pointer_focus != surface)
		return -1;

	device->grab = grab;
	grab->input_device = device;

	return 0;
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

	callback = wl_client_add_object(client,
					&wl_callback_interface, NULL, id, NULL);
	wl_resource_post_event(callback, WL_CALLBACK_DONE, 0);
	wl_resource_destroy(callback, 0);
}

struct wl_display_interface display_interface = {
	display_bind,
	display_sync,
};

static void
bind_display(struct wl_client *client,
	     void *data, uint32_t version, uint32_t id)
{
	struct wl_display *display = data;
	struct wl_global *global;

	client->display_resource =
		wl_client_add_object(client, &wl_display_interface,
				     &display_interface, id, display);

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

	wl_list_init(&display->callback_list);
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

  	wl_event_loop_destroy(display->loop);
	wl_list_for_each_safe(s, next, &display->socket_list, link) {
		close(s->fd);
		unlink(s->addr.sun_path);
		close(s->fd_lock);
		unlink(s->lock_addr);
		free(s);
	}

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

	global = malloc(sizeof *global);
	if (global == NULL)
		return NULL;

	global->name = display->id++;
	global->interface = interface;
	global->data = data;
	global->bind = bind;
	wl_list_insert(display->global_list.prev, &global->link);

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
	client_fd =
		accept4(fd, (struct sockaddr *) &name, &length, SOCK_CLOEXEC);
	if (client_fd < 0 && errno == ENOSYS) {
		client_fd = accept(fd, (struct sockaddr *) &name, &length);
		if (client_fd >= 0 && fcntl(client_fd, F_SETFD, FD_CLOEXEC) == -1)
			fprintf(stderr, "failed to set FD_CLOEXEC flag on client fd, errno: %d\n", errno);
	}

	if (client_fd < 0)
		fprintf(stderr, "failed to accept, errno: %d\n", errno);

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

	s->fd = socket(PF_LOCAL, SOCK_STREAM | SOCK_CLOEXEC, 0);
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

	if (wl_event_loop_add_fd(display->loop, s->fd,
				 WL_EVENT_READABLE,
				 socket_data, display) == NULL) {
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
	wl_list_init(&resource->destroy_listener_list);

	if (wl_map_insert_at(&client->objects, resource->object.id, resource) < 0) {
		wl_resource_post_no_memory(client->display_resource);
		free(resource);
		return NULL;
	}

	return resource;
}

static void
compositor_bind(struct wl_client *client,
		void *data, uint32_t version, uint32_t id)
{
	struct wl_compositor *compositor = data;
	struct wl_resource *resource;

	resource = wl_client_add_object(client, &wl_compositor_interface,
					compositor->interface, id, compositor);
	if (resource == NULL)
		return;
}

WL_EXPORT int
wl_compositor_init(struct wl_compositor *compositor,
		   const struct wl_compositor_interface *interface,
		   struct wl_display *display)
{
	struct wl_global *global;

	compositor->interface = interface;
	global = wl_display_add_global(display, &wl_compositor_interface,
				       compositor, compositor_bind);
	if (!global)
		return -1;

	return 0;
}
