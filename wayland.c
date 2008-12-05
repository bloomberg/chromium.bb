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

#include "wayland.h"
#include "connection.h"

struct wl_client {
	struct wl_connection *connection;
	struct wl_event_source *source;
	struct wl_display *display;
	struct wl_list object_list;
	struct wl_list link;
	uint32_t pending_frame;
};

struct wl_display {
	struct wl_object base;
	struct wl_event_loop *loop;
	struct wl_hash objects;

	struct wl_object *pointer;

	struct wl_compositor *compositor;
	struct wl_compositor_interface *compositor_interface;

	struct wl_list surface_list;
	struct wl_list client_list;
	uint32_t client_id_range;
	uint32_t id;

	struct wl_list global_list;

	int32_t pointer_x;
	int32_t pointer_y;
};

struct wl_surface {
	struct wl_object base;

	/* provided by client */
	int width, height;
	int buffer;
	int stride;
	
	struct wl_map map;
	struct wl_list link;

	/* how to convert buffer contents to pixels in screen format;
	 * yuv->rgb, indexed->rgb, svg->rgb, but mostly just rgb->rgb. */

	/* how to transform/render rectangular contents to polygons. */

	void *compositor_data;
};

struct wl_object_ref {
	struct wl_object *object;
	struct wl_list link;
};
				   
static void
wl_surface_destroy(struct wl_client *client,
		   struct wl_surface *surface)
{
	const struct wl_compositor_interface *interface;

	interface = client->display->compositor->interface;
	interface->notify_surface_destroy(client->display->compositor,
					  surface);
	wl_list_remove(&surface->link);
}

static void
wl_surface_attach(struct wl_client *client,
		  struct wl_surface *surface, uint32_t name, 
		  uint32_t width, uint32_t height, uint32_t stride)
{
	const struct wl_compositor_interface *interface;

	interface = client->display->compositor->interface;
	interface->notify_surface_attach(client->display->compositor,
					 surface, name, width, height, stride);
}

static void
wl_surface_map(struct wl_client *client, struct wl_surface *surface,
	       int32_t x, int32_t y, int32_t width, int32_t height)
{
	const struct wl_compositor_interface *interface;

	/* FIXME: This needs to take a tri-mesh argument... - count
	 * and a list of tris. 0 tris means unmap. */

	surface->map.x = x;
	surface->map.y = y;
	surface->map.width = width;
	surface->map.height = height;

	interface = client->display->compositor->interface;
	interface->notify_surface_map(client->display->compositor,
				      surface, &surface->map);
}

static void
wl_surface_copy(struct wl_client *client, struct wl_surface *surface,
		int32_t dst_x, int32_t dst_y, uint32_t name, uint32_t stride,
		int32_t x, int32_t y, int32_t width, int32_t height)
{
	const struct wl_compositor_interface *interface;

	interface = client->display->compositor->interface;
	interface->notify_surface_copy(client->display->compositor,
				       surface, dst_x, dst_y,
				       name, stride, x, y, width, height);
}

static void
wl_surface_damage(struct wl_client *client, struct wl_surface *surface,
		  int32_t x, int32_t y, int32_t width, int32_t height)
{
	const struct wl_compositor_interface *interface;

	interface = client->display->compositor->interface;
	interface->notify_surface_damage(client->display->compositor,
					 surface, x, y, width, height);
}

static const struct wl_method surface_methods[] = {
	{ "destroy", wl_surface_destroy, "" },
	{ "attach", wl_surface_attach, "uuuu" },
	{ "map", wl_surface_map, "iiii" },
	{ "copy", wl_surface_copy, "iiuuiiii" },
	{ "damage", wl_surface_damage, "iiii" }
};

static const struct wl_interface surface_interface = {
	"surface", 1,
	ARRAY_LENGTH(surface_methods),
	surface_methods,
};

static struct wl_surface *
wl_surface_create(struct wl_display *display, uint32_t id)
{
	struct wl_surface *surface;
	const struct wl_compositor_interface *interface;

	surface = malloc(sizeof *surface);
	if (surface == NULL)
		return NULL;

	surface->base.id = id;
	surface->base.interface = &surface_interface;

	wl_list_insert(display->surface_list.prev, &surface->link);

	interface = display->compositor->interface;
	interface->notify_surface_create(display->compositor, surface);

	return surface;
}

WL_EXPORT void
wl_surface_set_data(struct wl_surface *surface, void *data)
{
	surface->compositor_data = data;
}

WL_EXPORT void *
wl_surface_get_data(struct wl_surface *surface)
{
	return surface->compositor_data;
}

void
wl_client_destroy(struct wl_client *client);

static void
wl_client_marshal(struct wl_client *client, struct wl_object *sender,
		  uint32_t opcode, ...)
{
	const struct wl_event *event;
	struct wl_object *object;
	uint32_t args[10], size;
	va_list ap;
	int i, count;

	event = &sender->interface->events[opcode];
	count = strlen(event->signature) + 2;
	assert(count <= ARRAY_LENGTH(args));

	size = 0;
	va_start(ap, opcode);
	for (i = 2; i < count; i++) {
		switch (event->signature[i - 2]) {
		case 'u':
		case 'i':
			args[i] = va_arg(ap, uint32_t);
			size += sizeof args[i];
			break;
		case 's':
			/* FIXME */
			args[i] = 0;
			size += sizeof args[i];
			break;
		case 'o':
			object = va_arg(ap, struct wl_object *);
			args[i] = object->id;
			size += sizeof args[i];
			break;
		default:
			assert(0);
			break;
		}
	}
	va_end(ap);

	size += 2 * sizeof args[0];
	args[0] = sender->id;
	args[1] = opcode | (size << 16);
	wl_connection_write(client->connection, args, size);
}

static void
wl_client_demarshal(struct wl_client *client, struct wl_object *target,
		    const struct wl_method *method, size_t size)
{
	ffi_type *types[20];
	ffi_cif cif;
	uint32_t *p, result;
	int i, j, count;
	union {
		uint32_t uint32;
		const char *string;
		void *object;
		uint32_t new_id;
	} values[20];
	void *args[20];
	struct wl_object *object;
	uint32_t data[64];

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
	j = 0;
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
			object = wl_hash_lookup(&client->display->objects, *p);
			if (object == NULL)
				printf("unknown object (%d)\n", *p);
			if (object->interface != method->types[j])
				printf("wrong object type\n");
			values[i].object = object;
			p++;
			j++;
			break;
		case 'n':
			types[i] = &ffi_type_uint32;
			values[i].new_id = *p;
			object = wl_hash_lookup(&client->display->objects, *p);
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

	ffi_prep_cif(&cif, FFI_DEFAULT_ABI, count, &ffi_type_uint32, types);
	ffi_call(&cif, FFI_FN(method->func), &result, args);
}

#define WL_DISPLAY_INVALID_OBJECT 0
#define WL_DISPLAY_INVALID_METHOD 1
#define WL_DISPLAY_NO_MEMORY 2
#define WL_DISPLAY_ACKNOWLEDGE 3
#define WL_DISPLAY_FRAME 4

static void
wl_client_connection_data(int fd, uint32_t mask, void *data)
{
	struct wl_client *client = data;
	struct wl_connection *connection = client->connection;
	const struct wl_method *method;
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

		object = wl_hash_lookup(&client->display->objects, p[0]);
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
				
		method = &object->interface->methods[opcode];
		wl_client_demarshal(client, object, method, size);
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
advertise_object(struct wl_client *client, struct wl_object *object)
{
	const struct wl_interface *interface;
	static const char pad[4];
	uint32_t length, p[2];

	interface = object->interface;
	length = strlen(interface->name);
	p[0] = object->id;
	p[1] = length;
	wl_connection_write(client->connection, p, sizeof p);
	wl_connection_write(client->connection, interface->name, length);
	wl_connection_write(client->connection, pad, -length & 3);
}

static struct wl_client *
wl_client_create(struct wl_display *display, int fd)
{
	struct wl_client *client;
	struct wl_object_ref *ref;
	uint32_t count;

	client = malloc(sizeof *client);
	if (client == NULL)
		return NULL;

	memset(client, 0, sizeof *client);
	client->display = display;
	client->source = wl_event_loop_add_fd(display->loop, fd,
					      WL_EVENT_READABLE,
					      wl_client_connection_data, client);
	client->connection = wl_connection_create(fd,
						  wl_client_connection_update, 
						  client);
	wl_list_init(&client->object_list);

	wl_connection_write(client->connection,
			    &display->client_id_range,
			    sizeof display->client_id_range);
	display->client_id_range += 256;

	/* Write list of global objects to client. */
	count = wl_list_length(&display->global_list);
	wl_connection_write(client->connection, &count, sizeof count);
	
	ref = container_of(display->global_list.next,
			   struct wl_object_ref, link);
	while (&ref->link != &display->global_list) {
		advertise_object(client, ref->object);

		ref = container_of(ref->link.next,
				   struct wl_object_ref, link);
	}

	wl_list_insert(display->client_list.prev, &client->link);

	return client;
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
		wl_surface_destroy(client, (struct wl_surface *) ref->object);
		free(ref);
	}

	wl_event_source_remove(client->source);
	wl_connection_destroy(client->connection);
	free(client);
}

static int
wl_display_create_surface(struct wl_client *client,
			  struct wl_display *display, uint32_t id)
{
	struct wl_surface *surface;
	struct wl_object_ref *ref;

	surface = wl_surface_create(display, id);

	ref = malloc(sizeof *ref);
	if (ref == NULL) {
		wl_client_marshal(client, &display->base,
				  WL_DISPLAY_NO_MEMORY);
		return -1;
	}

	ref->object = &surface->base;
	wl_hash_insert(&display->objects, &surface->base);
	wl_list_insert(client->object_list.prev, &ref->link);

	return 0;
}

static int
wl_display_commit(struct wl_client *client,
		  struct wl_display *display, uint32_t key)
{
	const struct wl_compositor_interface *interface;
	uint32_t frame;

	client->pending_frame = 1;

	interface = display->compositor->interface;
	frame = interface->notify_commit(display->compositor);
	wl_client_marshal(client, &display->base,
			  WL_DISPLAY_ACKNOWLEDGE, key, frame);

	return 0;
}

static const struct wl_method display_methods[] = {
	{ "create_surface", wl_display_create_surface, "n" },
	{ "commit", wl_display_commit, "u" }
};

static const struct wl_event display_events[] = {
	{ "invalid_object", "u" },
	{ "invalid_method", "uu" },
	{ "no_memory", "" },
	{ "acknowledge", "uu" },
	{ "frame", "uu" }
};

static const struct wl_interface display_interface = {
	"display", 1,
	ARRAY_LENGTH(display_methods), display_methods,
	ARRAY_LENGTH(display_events), display_events,
};

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

	wl_list_init(&display->surface_list);
	wl_list_init(&display->client_list);
	wl_list_init(&display->global_list);

	display->pointer_x = 100;
	display->pointer_y = 100;

	display->client_id_range = 256; /* Gah, arbitrary... */

	display->id = 1;
	display->base.interface = &display_interface;
	wl_display_add_object(display, &display->base);
	if (wl_display_add_global(display, &display->base)) {
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
	wl_hash_insert(&display->objects, object);
}

WL_EXPORT int
wl_display_add_global(struct wl_display *display, struct wl_object *object)
{
	struct wl_object_ref *ref;

	ref = malloc(sizeof *ref);
	if (ref == NULL)
		return -1;

	ref->object = object;
	wl_list_insert(display->global_list.prev, &ref->link);

	return 0;	
}

static void
wl_display_send_event(struct wl_display *display, uint32_t *data, size_t size)
{
	struct wl_client *client;

	client = container_of(display->client_list.next,
			      struct wl_client, link);
	while (&client->link != &display->client_list) {
		wl_connection_write(client->connection, data, size);

		client = container_of(client->link.next,
				   struct wl_client, link);
	}
}

#define WL_INPUT_MOTION 0
#define WL_INPUT_BUTTON 1
#define WL_INPUT_KEY 2

WL_EXPORT void
wl_display_post_relative_event(struct wl_display *display,
			       struct wl_object *source, int dx, int dy)
{
	const struct wl_compositor_interface *interface;
	uint32_t p[4];

	display->pointer_x += dx;
	display->pointer_y += dy;

	interface = display->compositor->interface;
	interface->notify_pointer_motion(display->compositor, source,
					 display->pointer_x, display->pointer_y);

	p[0] = source->id;
	p[1] = (sizeof p << 16) | WL_INPUT_MOTION;
	p[2] = display->pointer_x;
	p[3] = display->pointer_y;

	wl_display_send_event(display, p, sizeof p);
}

WL_EXPORT void
wl_display_post_absolute_event(struct wl_display *display,
			       struct wl_object *source, int x, int y)
{
	const struct wl_compositor_interface *interface;
	uint32_t p[4];

	display->pointer_x = x;
	display->pointer_y = y;

	interface = display->compositor->interface;
	interface->notify_pointer_motion(display->compositor, source,
					 display->pointer_x, display->pointer_y);

	p[0] = source->id;
	p[1] = (sizeof p << 16) | WL_INPUT_MOTION;
	p[2] = display->pointer_x;
	p[3] = display->pointer_y;

	wl_display_send_event(display, p, sizeof p);
}

WL_EXPORT void
wl_display_post_button_event(struct wl_display *display,
			     struct wl_object *source, int button, int state)
{
	uint32_t p[4];

	p[0] = source->id;
	p[1] = (sizeof p << 16) | WL_INPUT_BUTTON;
	p[2] = button;
	p[3] = state;

	wl_display_send_event(display, p, sizeof p);
}

WL_EXPORT void
wl_display_post_key_event(struct wl_display *display,
			  struct wl_object *source, int key, int state)
{
	const struct wl_compositor_interface *interface;
	uint32_t p[4];

	interface = display->compositor->interface;
	interface->notify_key(display->compositor, source, key, state);

	p[0] = source->id;
	p[1] = (sizeof p << 16) | WL_INPUT_KEY;
	p[2] = key;
	p[3] = state;

	wl_display_send_event(display, p, sizeof p);
}

WL_EXPORT void
wl_display_post_frame(struct wl_display *display,
		      uint32_t frame, uint32_t msecs)
{
	struct wl_client *client;

	client = container_of(display->client_list.next,
			      struct wl_client, link);

	while (&client->link != &display->client_list) {
		if (client->pending_frame) {
			wl_client_marshal(client, &display->base,
					  WL_DISPLAY_FRAME, frame, msecs);
			client->pending_frame = 0;
		}
		client = container_of(client->link.next,
				      struct wl_client, link);
	}
}

WL_EXPORT void
wl_display_set_compositor(struct wl_display *display,
			  struct wl_compositor *compositor)
{
	display->compositor = compositor;
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
wl_display_add_socket(struct wl_display *display, const char *socket_name)
{
	struct sockaddr_un name;
	int sock;
	socklen_t size;

	sock = socket(PF_LOCAL, SOCK_STREAM, 0);
	if (sock < 0)
		return -1;

	name.sun_family = AF_LOCAL;
	memcpy(name.sun_path, socket_name, sizeof socket_name);

	size = offsetof (struct sockaddr_un, sun_path) + sizeof socket_name;
	if (bind(sock, (struct sockaddr *) &name, size) < 0)
		return -1;

	if (listen(sock, 1) < 0)
		return -1;

	wl_event_loop_add_fd(display->loop, sock,
			     WL_EVENT_READABLE,
			     socket_data, display);

	return 0;
}


struct wl_surface_iterator {
	struct wl_list *head;
	struct wl_surface *surface;
	uint32_t mask;
};

WL_EXPORT struct wl_surface_iterator *
wl_surface_iterator_create(struct wl_display *display, uint32_t mask)
{
	struct wl_surface_iterator *iterator;

	iterator = malloc(sizeof *iterator);
	if (iterator == NULL)
		return NULL;

	iterator->head = &display->surface_list;
	iterator->surface = container_of(display->surface_list.next,
					 struct wl_surface, link);
	iterator->mask = mask;

	return iterator;
}

WL_EXPORT int
wl_surface_iterator_next(struct wl_surface_iterator *iterator,
			 struct wl_surface **surface)
{
	if (&iterator->surface->link == iterator->head)
		return 0;

	*surface = iterator->surface;
	iterator->surface = container_of(iterator->surface->link.next,
					 struct wl_surface, link);

	return 1;
}

WL_EXPORT void
wl_surface_iterator_destroy(struct wl_surface_iterator *iterator)
{
	free(iterator);
}
