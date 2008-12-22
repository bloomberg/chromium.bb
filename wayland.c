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

#include "wayland-protocol.h"
#include "wayland.h"
#include "connection.h"

struct wl_client {
	struct wl_connection *connection;
	struct wl_event_source *source;
	struct wl_display *display;
	struct wl_list object_list;
	struct wl_list link;
	uint32_t id_count;
};

struct wl_display {
	struct wl_object base;
	struct wl_event_loop *loop;
	struct wl_hash *objects;

	struct wl_list pending_frame_list;
	uint32_t client_id_range;
	uint32_t id;

	struct wl_list global_list;
};

struct wl_object_ref {
	struct wl_object *object;
	struct wl_list link;
};

struct wl_global {
	struct wl_object *object;
	wl_client_connect_func_t func;	
	struct wl_list link;
};

void
wl_client_destroy(struct wl_client *client);

static void
wl_client_vmarshal(struct wl_client *client, struct wl_object *sender,
		   uint32_t opcode, va_list ap)
{
	const struct wl_message *event;
	struct wl_object *object;
	uint32_t args[32], length, *p, size;
	const char *s;
	int i, count;

	event = &sender->interface->events[opcode];
	count = strlen(event->signature);
	assert(count <= ARRAY_LENGTH(args));

	p = &args[2];
	for (i = 0; i < count; i++) {
		switch (event->signature[i]) {
		case 'u':
		case 'i':
			*p++ = va_arg(ap, uint32_t);
			break;
		case 's':
			s = va_arg(ap, const char *);
			length = strlen(s);
			*p++ = length;
			memcpy(p, s, length);
			p += DIV_ROUNDUP(length, sizeof(*p));
			break;
		case 'o':
			object = va_arg(ap, struct wl_object *);
			*p++ = object->id;
			break;
		default:
			assert(0);
			break;
		}
	}

	size = (p - args) * sizeof *p;
	args[0] = sender->id;
	args[1] = opcode | (size << 16);
	wl_connection_write(client->connection, args, size);
}

static void
wl_client_marshal(struct wl_client *client, struct wl_object *sender,
		  uint32_t opcode, ...)
{
	va_list ap;

	va_start(ap, opcode);
	wl_client_vmarshal(client, sender, opcode, ap);
	va_end(ap);
}

WL_EXPORT void
wl_client_post_event(struct wl_client *client, struct wl_object *sender,
		     uint32_t opcode, ...)
{
	va_list ap;

	va_start(ap, opcode);
	wl_client_vmarshal(client, sender, opcode, ap);
	va_end(ap);
}

static void
wl_client_demarshal(struct wl_client *client, struct wl_object *target,
		    uint32_t opcode, size_t size)
{
	const struct wl_message *method;
	ffi_type *types[20];
	ffi_cif cif;
	uint32_t *p, result;
	int i, count;
	union {
		uint32_t uint32;
		const char *string;
		void *object;
		uint32_t new_id;
	} values[20];
	void *args[20];
	struct wl_object *object;
	uint32_t data[64];
	void (*func)(void);

	method = &target->interface->methods[opcode];
	count = strlen(method->signature) + 2;
	if (count > ARRAY_LENGTH(types)) {
		printf("too many args (%d)\n", count);
		return;
	}

	if (sizeof data < size) {
		printf("request too big, should malloc tmp buffer here\n");
		return;
	}

	types[0] = &ffi_type_pointer;
	values[0].object = client;
	args[0] =  &values[0];

	types[1] = &ffi_type_pointer;
	values[1].object = target;
	args[1] =  &values[1];

	wl_connection_copy(client->connection, data, size);
	p = &data[2];
	for (i = 2; i < count; i++) {
		switch (method->signature[i - 2]) {
		case 'u':
		case 'i':
			types[i] = &ffi_type_uint32;
			values[i].uint32 = *p;
			p++;
			break;
		case 's':
			types[i] = &ffi_type_pointer;
			/* FIXME */
			values[i].uint32 = *p++;
			break;
		case 'o':
			types[i] = &ffi_type_pointer;
			object = wl_hash_lookup(client->display->objects, *p);
			if (object == NULL)
				printf("unknown object (%d)\n", *p);
			values[i].object = object;
			p++;
			break;
		case 'n':
			types[i] = &ffi_type_uint32;
			values[i].new_id = *p;
			object = wl_hash_lookup(client->display->objects, *p);
			if (object != NULL)
				printf("object already exists (%d)\n", *p);
			p++;
			break;
		default:
			printf("unknown type\n");
			break;
		}
		args[i] = &values[i];
	}

	func = target->implementation[opcode];
	ffi_prep_cif(&cif, FFI_DEFAULT_ABI, count, &ffi_type_uint32, types);
	ffi_call(&cif, func, &result, args);
}

static void
wl_client_connection_data(int fd, uint32_t mask, void *data)
{
	struct wl_client *client = data;
	struct wl_connection *connection = client->connection;
	struct wl_object *object;
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
		return;
	}

	while (len >= sizeof p) {
		wl_connection_copy(connection, p, sizeof p);
		opcode = p[1] & 0xffff;
		size = p[1] >> 16;
		if (len < size)
			break;

		object = wl_hash_lookup(client->display->objects, p[0]);
		if (object == NULL) {
			wl_client_marshal(client, &client->display->base,
					  WL_DISPLAY_INVALID_OBJECT, p[0]);
			wl_connection_consume(connection, size);
			len -= size;
			continue;
		}
				
		if (opcode >= object->interface->method_count) {
			wl_client_marshal(client, &client->display->base,
					  WL_DISPLAY_INVALID_METHOD, p[0], opcode);
			wl_connection_consume(connection, size);
			len -= size;
			continue;
		}
				
		wl_client_demarshal(client, object, opcode, size);
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
	wl_client_marshal(client, &client->display->base,
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

	wl_list_init(&client->object_list);
	wl_list_init(&client->link);

	wl_display_post_range(display, client);

	global = container_of(display->global_list.next,
			      struct wl_global, link);
	while (&global->link != &display->global_list) {
		wl_client_marshal(client, &client->display->base,
				  WL_DISPLAY_GLOBAL,
				  global->object,
				  global->object->interface->name,
				  global->object->interface->version);
		global = container_of(global->link.next,
				      struct wl_global, link);
	}

	global = container_of(display->global_list.next,
			      struct wl_global, link);
	while (&global->link != &display->global_list) {
		if (global->func)
			global->func(client, global->object);
		global = container_of(global->link.next,
				      struct wl_global, link);
	}

	return client;
}

static void
wl_object_destroy(struct wl_object *object)
{
	const struct wl_surface_interface *interface =
		(const struct wl_surface_interface *) object->implementation;

	/* FIXME: Need generic object destructor. */
	interface->destroy(NULL, (struct wl_surface *) object);
}

void
wl_client_destroy(struct wl_client *client)
{
	struct wl_object_ref *ref;

	printf("disconnect from client %p\n", client);

	wl_list_remove(&client->link);

	while (client->object_list.next != &client->object_list) {
		ref = container_of(client->object_list.next,
				   struct wl_object_ref, link);
		wl_list_remove(&ref->link);
		wl_object_destroy(ref->object);
		free(ref);
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
	struct wl_display *display = client->display;
	struct wl_object_ref *ref;

	if (client->id_count-- < 64)
		wl_display_post_range(display, client);

	surface->base.id = id;
	surface->base.interface = &wl_surface_interface;
	surface->base.implementation = (void (**)(void)) implementation;
	surface->client = client;

	ref = malloc(sizeof *ref);
	if (ref == NULL) {
		wl_client_marshal(client, &display->base,
				  WL_DISPLAY_NO_MEMORY);
		return -1;
	}

	ref->object = &surface->base;
	wl_hash_insert(display->objects, &surface->base);
	wl_list_insert(client->object_list.prev, &ref->link);

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
	wl_client_marshal(client, &compositor->base,
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

	display->objects = wl_hash_create();
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
	wl_hash_insert(display->objects, object);
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

	va_start(ap, event);
	wl_client_vmarshal(surface->client, sender, event, ap);
	va_end(ap);
}

WL_EXPORT void
wl_display_post_frame(struct wl_display *display,
		      struct wl_compositor *compositor,
		      uint32_t frame, uint32_t msecs)
{
	struct wl_client *client;

	client = container_of(display->pending_frame_list.next,
			      struct wl_client, link);

	while (&client->link != &display->pending_frame_list) {
		wl_client_marshal(client, &compositor->base,
				  WL_COMPOSITOR_FRAME, frame, msecs);
		client = container_of(client->link.next,
				      struct wl_client, link);
	}

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
