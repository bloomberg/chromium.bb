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

#include <stdlib.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdarg.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <dlfcn.h>
#include <assert.h>
#include <ffi.h>

#include "wayland-server.h"
#include "wayland-server-protocol.h"
#include "connection.h"

struct wl_client {
	struct wl_connection *connection;
	struct wl_event_source *source;
	struct wl_display *display;
	struct wl_list resource_list;
	struct wl_list link;
	uint32_t id_count;
};

struct wl_display {
	struct wl_object base;
	struct wl_event_loop *loop;
	struct wl_hash_table *objects;

	struct wl_list pending_frame_list;
	uint32_t client_id_range;
	uint32_t id;

	struct wl_list global_list;
};

struct wl_global {
	struct wl_object *object;
	wl_client_connect_func_t func;
	struct wl_list link;
};

WL_EXPORT struct wl_surface wl_grab_surface;

WL_EXPORT void
wl_client_post_event(struct wl_client *client, struct wl_object *sender,
		     uint32_t opcode, ...)
{
	va_list ap;

	va_start(ap, opcode);
	wl_connection_vmarshal(client->connection,
			       sender, opcode, ap,
			       &sender->interface->events[opcode]);
	va_end(ap);
}

static void
wl_client_connection_data(int fd, uint32_t mask, void *data)
{
	struct wl_client *client = data;
	struct wl_connection *connection = client->connection;
	struct wl_object *object;
	uint32_t p[2], opcode, size;
	uint32_t cmask = 0;
	int len, ret;

	if (mask & WL_EVENT_READABLE)
		cmask |= WL_CONNECTION_READABLE;
	if (mask & WL_EVENT_WRITEABLE)
		cmask |= WL_CONNECTION_WRITABLE;

	len = wl_connection_data(connection, cmask);
	if (len < 0) {
		wl_client_destroy(client);
		return;
	}

	while (len >= sizeof p) {
		wl_connection_copy(connection, p, sizeof p);
		opcode = p[1] & 0xffff;
		size = p[1] >> 16;
		if (len < size)
			break;

		object = wl_hash_table_lookup(client->display->objects, p[0]);
		if (object == NULL) {
			wl_client_post_event(client, &client->display->base,
					     WL_DISPLAY_INVALID_OBJECT, p[0]);
			wl_connection_consume(connection, size);
			len -= size;
			continue;
		}

		if (opcode >= object->interface->method_count) {
			wl_client_post_event(client, &client->display->base,
					     WL_DISPLAY_INVALID_METHOD, p[0], opcode);
			wl_connection_consume(connection, size);
			len -= size;
			continue;
		}

		ret = wl_connection_demarshal(client->connection,
					      size,
					      client->display->objects,
					      object->implementation[opcode],
					      client,
					      object, 
					      &object->interface->methods[opcode]);

		if (ret < 0 && errno == EINVAL)
			wl_client_post_event(client, &client->display->base,
					     WL_DISPLAY_INVALID_METHOD,
					     p[0], opcode);
		if (ret < 0 && errno == ENOMEM)
			wl_client_post_event(client, &client->display->base,
					     WL_DISPLAY_NO_MEMORY);

		wl_connection_consume(connection, size);
		len -= size;
	}
}

static int
wl_client_connection_update(struct wl_connection *connection,
			    uint32_t mask, void *data)
{
	struct wl_client *client = data;
	uint32_t emask = 0;

	if (mask & WL_CONNECTION_READABLE)
		emask |= WL_EVENT_READABLE;
	if (mask & WL_CONNECTION_WRITABLE)
		emask |= WL_EVENT_WRITEABLE;

	return wl_event_source_fd_update(client->source, mask);
}

static void
wl_display_post_range(struct wl_display *display, struct wl_client *client)
{
	wl_client_post_event(client, &client->display->base,
			     WL_DISPLAY_RANGE, display->client_id_range);
	display->client_id_range += 256;
	client->id_count += 256;
}

static struct wl_client *
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

	wl_list_init(&client->resource_list);
	wl_list_init(&client->link);

	wl_display_post_range(display, client);

	wl_list_for_each(global, &display->global_list, link)
		wl_client_post_event(client, &client->display->base,
				     WL_DISPLAY_GLOBAL,
				     global->object,
				     global->object->interface->name,
				     global->object->interface->version);

	wl_list_for_each(global, &display->global_list, link)
		if (global->func)
			global->func(client, global->object);

	return client;
}

WL_EXPORT void
wl_client_add_resource(struct wl_client *client,
		       struct wl_resource *resource)
{
	struct wl_display *display = client->display;

	if (client->id_count-- < 64)
		wl_display_post_range(display, client);

	wl_hash_table_insert(client->display->objects,
			     resource->base.id, resource);
	wl_list_insert(client->resource_list.prev, &resource->link);
}

WL_EXPORT void
wl_resource_destroy(struct wl_resource *resource, struct wl_client *client)
{
	struct wl_display *display = client->display;

	wl_list_remove(&resource->link);
	wl_hash_table_remove(display->objects, resource->base.id);
	resource->destroy(resource, client);
}

WL_EXPORT void
wl_client_destroy(struct wl_client *client)
{
	struct wl_resource *resource, *tmp;

	printf("disconnect from client %p\n", client);

	wl_list_remove(&client->link);

	wl_list_for_each_safe(resource, tmp, &client->resource_list, link) {
		wl_list_remove(&resource->link);
		resource->destroy(resource, client);
	}

	wl_event_source_remove(client->source);
	wl_connection_destroy(client->connection);
	free(client);
}

WL_EXPORT int
wl_client_add_surface(struct wl_client *client,
		      struct wl_surface *surface,
		      const struct wl_surface_interface *implementation, 
		      uint32_t id)
{
	surface->base.base.id = id;
	surface->base.base.interface = &wl_surface_interface;
	surface->base.base.implementation = (void (**)(void)) implementation;
	surface->client = client;

	wl_client_add_resource(client, &surface->base);

	return 0;
}

WL_EXPORT void
wl_client_send_acknowledge(struct wl_client *client,
			   struct wl_compositor *compositor,
			   uint32_t key, uint32_t frame)
{
	wl_list_remove(&client->link);
	wl_list_insert(client->display->pending_frame_list.prev,
		       &client->link);
	wl_client_post_event(client, &compositor->base,
			     WL_COMPOSITOR_ACKNOWLEDGE, key, frame);
}

WL_EXPORT int
wl_display_set_compositor(struct wl_display *display,
			  struct wl_compositor *compositor,
			  const struct wl_compositor_interface *implementation)
{
	compositor->base.interface = &wl_compositor_interface;
	compositor->base.implementation = (void (**)(void)) implementation;

	wl_display_add_object(display, &compositor->base);
	if (wl_display_add_global(display, &compositor->base, NULL))
		return -1;

	return 0;
}

WL_EXPORT struct wl_display *
wl_display_create(void)
{
	struct wl_display *display;

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
		free(display);
		return NULL;
	}

	wl_list_init(&display->pending_frame_list);
	wl_list_init(&display->global_list);

	display->client_id_range = 256; /* Gah, arbitrary... */

	display->id = 1;
	display->base.interface = &wl_display_interface;
	display->base.implementation = NULL;
	wl_display_add_object(display, &display->base);
	if (wl_display_add_global(display, &display->base, NULL)) {
		wl_event_loop_destroy(display->loop);
		free(display);
		return NULL;
	}		

	return display;		
}

WL_EXPORT void
wl_display_add_object(struct wl_display *display, struct wl_object *object)
{
	object->id = display->id++;
	wl_hash_table_insert(display->objects, object->id, object);
}

WL_EXPORT int
wl_display_add_global(struct wl_display *display,
		      struct wl_object *object, wl_client_connect_func_t func)
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

WL_EXPORT void
wl_surface_post_event(struct wl_surface *surface,
		      struct wl_object *sender,
		      uint32_t event, ...)
{
	va_list ap;

	if (surface == &wl_grab_surface)
		return;

	va_start(ap, event);
	wl_connection_vmarshal(surface->client->connection,
			       sender, event, ap,
			       &sender->interface->events[event]);
	va_end(ap);
}

WL_EXPORT void
wl_display_post_frame(struct wl_display *display,
		      struct wl_compositor *compositor,
		      uint32_t frame, uint32_t msecs)
{
	struct wl_client *client;

	wl_list_for_each(client, &display->pending_frame_list, link)
		wl_client_post_event(client, &compositor->base,
				     WL_COMPOSITOR_FRAME, frame, msecs);

	wl_list_remove(&display->pending_frame_list);
	wl_list_init(&display->pending_frame_list);
}

WL_EXPORT struct wl_event_loop *
wl_display_get_event_loop(struct wl_display *display)
{
	return display->loop;
}

WL_EXPORT void
wl_display_run(struct wl_display *display)
{
	while (1)
		wl_event_loop_wait(display->loop);
}

static void
socket_data(int fd, uint32_t mask, void *data)
{
	struct wl_display *display = data;
	struct sockaddr_un name;
	socklen_t length;
	int client_fd;

	length = sizeof name;
	client_fd = accept (fd, (struct sockaddr *) &name, &length);
	if (client_fd < 0)
		fprintf(stderr, "failed to accept\n");

	wl_client_create(display, client_fd);
}

WL_EXPORT int
wl_display_add_socket(struct wl_display *display,
		      const char *name, size_t name_size)
{
	struct sockaddr_un addr;
	int sock;
	socklen_t size;

	sock = socket(PF_LOCAL, SOCK_STREAM, 0);
	if (sock < 0)
		return -1;

	addr.sun_family = AF_LOCAL;
	memcpy(addr.sun_path, name, name_size);

	size = offsetof (struct sockaddr_un, sun_path) + name_size;
	if (bind(sock, (struct sockaddr *) &addr, size) < 0)
		return -1;

	if (listen(sock, 1) < 0)
		return -1;

	wl_event_loop_add_fd(display->loop, sock,
			     WL_EVENT_READABLE,
			     socket_data, display);

	return 0;
}
