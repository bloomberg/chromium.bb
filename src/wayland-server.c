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
	struct wl_resource display_resource;
	struct wl_list resource_list;
	uint32_t id_count;
	uint32_t mask;
	struct wl_list link;
};

struct wl_display {
	struct wl_resource resource;
	struct wl_event_loop *loop;
	struct wl_hash_table *objects;
	int run;

	struct wl_list callback_list;
	uint32_t client_id_range;
	uint32_t id;

	struct wl_list global_list;
	struct wl_list socket_list;
	struct wl_list client_list;
};

struct wl_global {
	struct wl_object *object;
	wl_global_bind_func_t func;
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
wl_client_post_error(struct wl_client *client, struct wl_object *object,
		     uint32_t code, const char *msg, ...)
{
	char buffer[128];
	va_list ap;

	va_start(ap, msg);
	vsnprintf(buffer, sizeof buffer, msg, ap);
	va_end(ap);

	wl_resource_post_event(&client->display_resource,
			       WL_DISPLAY_ERROR, object, code, buffer);
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

		resource = wl_hash_table_lookup(client->display->objects, p[0]);
		if (resource == NULL) {
			wl_client_post_error(client,
					     &client->display->resource.object,
					     WL_DISPLAY_ERROR_INVALID_OBJECT,
					     "invalid object %d", p[0]);
			wl_connection_consume(connection, size);
			len -= size;
			continue;
		}

		object = &resource->object;
		if (opcode >= object->interface->method_count) {
			wl_client_post_error(client,
					     &client->display->resource.object,
					     WL_DISPLAY_ERROR_INVALID_METHOD,
					     "invalid method %d, object %s@%d",
					     object->interface->name,
					     object->id, opcode);
			wl_connection_consume(connection, size);
			len -= size;
			continue;
		}

		message = &object->interface->methods[opcode];
		closure = wl_connection_demarshal(client->connection, size,
						  client->display->objects,
						  message);
		len -= size;

		if (closure == NULL && errno == EINVAL) {
			wl_client_post_error(client,
					     &client->display->resource.object,
					     WL_DISPLAY_ERROR_INVALID_METHOD,
					     "invalid arguments for %s@%d.%s",
					     object->interface->name,
					     object->id,
					     message->name);
			continue;
		} else if (closure == NULL && errno == ENOMEM) {
			wl_client_post_no_memory(client);
			continue;
		}


		if (wl_debug)
			wl_closure_print(closure, object, false); 

		wl_closure_invoke(closure, object,
				  object->implementation[opcode], client);

		wl_closure_destroy(closure);
	}

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
wl_display_post_range(struct wl_display *display, struct wl_client *client)
{
	wl_resource_post_event(&client->display_resource,
			       WL_DISPLAY_RANGE, display->client_id_range);
	display->client_id_range += 256;
	client->id_count += 256;
}

WL_EXPORT struct wl_client *
wl_client_create(struct wl_display *display, int fd)
{
	struct wl_client *client;
	struct wl_global *global;

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

	client->display_resource.object = display->resource.object;
	client->display_resource.client = client;

	wl_list_insert(display->client_list.prev, &client->link);

	wl_list_init(&client->resource_list);

	wl_display_post_range(display, client);

	wl_list_for_each(global, &display->global_list, link)
		wl_client_post_global(client, global->object);

	return client;
}

WL_EXPORT void
wl_client_add_resource(struct wl_client *client,
		       struct wl_resource *resource)
{
	struct wl_display *display = client->display;

	if (client->id_count-- < 64)
		wl_display_post_range(display, client);

	resource->client = client;
	wl_list_init(&resource->destroy_listener_list);

	wl_hash_table_insert(client->display->objects,
			     resource->object.id, resource);
	wl_list_insert(client->resource_list.prev, &resource->link);
}

WL_EXPORT void
wl_client_post_no_memory(struct wl_client *client)
{
	wl_client_post_error(client, &client->display->resource.object,
			     WL_DISPLAY_ERROR_NO_MEMORY, "no memory");
}

WL_EXPORT void
wl_client_post_global(struct wl_client *client, struct wl_object *object)
{
	wl_resource_post_event(&client->display_resource,
			       WL_DISPLAY_GLOBAL,
			       object->id,
			       object->interface->name,
			       object->interface->version);
}

WL_EXPORT void
wl_resource_destroy(struct wl_resource *resource, uint32_t time)
{
	struct wl_display *display = resource->client->display;
	struct wl_listener *l, *next;

	wl_list_for_each_safe(l, next, &resource->destroy_listener_list, link)
		l->func(l, resource, time);

	wl_list_remove(&resource->link);
	if (resource->object.id > 0)
		wl_hash_table_remove(display->objects, resource->object.id);
	resource->destroy(resource);
}

WL_EXPORT void
wl_client_destroy(struct wl_client *client)
{
	struct wl_resource *resource, *tmp;

	printf("disconnect from client %p\n", client);

	wl_list_for_each_safe(resource, tmp, &client->resource_list, link)
		wl_resource_destroy(resource, 0);

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

	wl_input_device_set_pointer_focus(device, NULL, time, 0, 0, 0, 0);
}

static void
lose_keyboard_focus(struct wl_listener *listener,
		    struct wl_resource *resource, uint32_t time)
{
	struct wl_input_device *device =
		container_of(listener, struct wl_input_device,
			     keyboard_focus_listener);

	wl_input_device_set_keyboard_focus(device, NULL, time);
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
		wl_list_insert(surface->resource.destroy_listener_list.prev,
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
		wl_list_insert(surface->resource.destroy_listener_list.prev,
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
	device->grab->input_device = NULL;
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
	if (device->grab != &device->motion_grab ||
	    device->grab_time != time ||
	    device->pointer_focus != surface)
		return -1;

	device->grab = grab;
	grab->input_device = device;

	return 0;
}

static void
display_bind(struct wl_client *client,
	     struct wl_resource *resource, uint32_t id,
	     const char *interface, uint32_t version)
{
	struct wl_global *global;
	struct wl_display *display = resource->data;

	wl_list_for_each(global, &display->global_list, link)
		if (global->object->id == id)
			break;

	if (&global->link == &display->global_list)
		wl_client_post_error(client, &client->display->resource.object,
				     WL_DISPLAY_ERROR_INVALID_OBJECT,
				     "invalid object %d", id);
	else if (global->func)
		global->func(client, global->object, version);
}

static void
display_sync(struct wl_client *client,
	     struct wl_resource *resource, uint32_t id)
{
	struct wl_resource callback;

	callback.object.interface = &wl_callback_interface;
	callback.object.id = id;
	callback.client = client;

	wl_resource_post_event(&callback, WL_CALLBACK_DONE, 0);
}

struct wl_display_interface display_interface = {
	display_bind,
	display_sync,
};


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

	display->objects = wl_hash_table_create();
	if (display->objects == NULL) {
		wl_event_loop_destroy(display->loop);
		free(display);
		return NULL;
	}

	wl_list_init(&display->callback_list);
	wl_list_init(&display->global_list);
	wl_list_init(&display->socket_list);
	wl_list_init(&display->client_list);

	display->client_id_range = 256; /* Gah, arbitrary... */

	display->id = 1;
	display->resource.object.interface = &wl_display_interface;
	display->resource.object.implementation =
		(void (**)(void)) &display_interface;
	display->resource.data = display;

	wl_display_add_object(display, &display->resource.object);
	if (wl_display_add_global(display, &display->resource.object, NULL)) {
		wl_hash_table_destroy(display->objects);
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
 	wl_hash_table_destroy(display->objects);
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

WL_EXPORT void
wl_display_add_object(struct wl_display *display, struct wl_object *object)
{
	object->id = display->id++;
	wl_hash_table_insert(display->objects, object->id, object);
}

WL_EXPORT int
wl_display_add_global(struct wl_display *display,
		      struct wl_object *object, wl_global_bind_func_t func)
{
	struct wl_global *global;

	global = malloc(sizeof *global);
	if (global == NULL)
		return -1;

	global->object = object;
	global->func = func;
	wl_list_insert(display->global_list.prev, &global->link);

	return 0;
}

WL_EXPORT int
wl_display_remove_global(struct wl_display *display,
                         struct wl_object *object)
{
	struct wl_global *global;
	struct wl_client *client;

	wl_list_for_each(global, &display->global_list, link)
		if (global->object == object)
			break;

	if (&global->link == &display->global_list)
		return -1;

	wl_list_for_each(client, &display->client_list, link)
		wl_resource_post_event(&client->display_resource,
				       WL_DISPLAY_GLOBAL_REMOVE,
				       global->object->id);
	wl_list_remove(&global->link);
	free(global);

	return 0;
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

static void
compositor_bind(struct wl_client *client,
		struct wl_object *global, uint32_t version)
{
	struct wl_compositor *compositor =
		container_of(global, struct wl_compositor, resource.object);

	compositor->resource.client = client;
	wl_resource_post_event(&compositor->resource,
			       WL_COMPOSITOR_TOKEN_VISUAL,
			       &compositor->argb_visual.object,
			       WL_COMPOSITOR_VISUAL_ARGB32);

	wl_resource_post_event(&compositor->resource,
			       WL_COMPOSITOR_TOKEN_VISUAL,
			       &compositor->premultiplied_argb_visual.object,
			       WL_COMPOSITOR_VISUAL_PREMULTIPLIED_ARGB32);

	wl_resource_post_event(&compositor->resource,
			       WL_COMPOSITOR_TOKEN_VISUAL,
			       &compositor->rgb_visual.object,
			       WL_COMPOSITOR_VISUAL_XRGB32);
}

WL_EXPORT int
wl_compositor_init(struct wl_compositor *compositor,
		   const struct wl_compositor_interface *interface,
		   struct wl_display *display)
{
	compositor->resource.object.interface = &wl_compositor_interface;
	compositor->resource.object.implementation =
		(void (**)(void)) interface;
	compositor->resource.data = compositor;
	wl_display_add_object(display, &compositor->resource.object);
	if (wl_display_add_global(display, &compositor->resource.object,
				  compositor_bind))
		return -1;

	compositor->argb_visual.object.interface = &wl_visual_interface;
	compositor->argb_visual.object.implementation = NULL;
	wl_display_add_object(display, &compositor->argb_visual.object);
	if (wl_display_add_global(display,
				  &compositor->argb_visual.object, NULL))
		return -1;

	compositor->premultiplied_argb_visual.object.interface =
		&wl_visual_interface;
	compositor->premultiplied_argb_visual.object.implementation = NULL;
	wl_display_add_object(display,
			      &compositor->premultiplied_argb_visual.object);
       if (wl_display_add_global(display,
                                 &compositor->premultiplied_argb_visual.object,
				 NULL))
	       return -1;

	compositor->rgb_visual.object.interface = &wl_visual_interface;
	compositor->rgb_visual.object.implementation = NULL;
	wl_display_add_object(display, &compositor->rgb_visual.object);
       if (wl_display_add_global(display,
				 &compositor->rgb_visual.object, NULL))
	       return -1;

	return 0;
}
