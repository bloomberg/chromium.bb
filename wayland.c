#include <stdlib.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/uio.h>
#include <ffi.h>
#include "wayland.h"

#define ARRAY_LENGTH(a) (sizeof (a) / sizeof (a)[0])

struct wl_region {
};

struct wl_buffer {
	char data[4096];
	int head, tail;
};

struct wl_client {
	struct wl_buffer in, out;
	struct wl_event_source *source;
	struct wl_display *display;
};

struct wl_display {
	struct wl_object base;
	struct wl_event_loop *loop;
	struct wl_hash objects;

	struct wl_compositor *compositor;
	struct wl_compositor_interface *compositor_interface;
};

struct wl_surface {
	struct wl_object base;

	/* provided by client */
	int width, height;
	int buffer;
	int stride;
	
	struct wl_map map;

	/* how to convert buffer contents to pixels in screen format;
	 * yuv->rgb, indexed->rgb, svg->rgb, but mostly just rgb->rgb. */

	/* how to transform/render rectangular contents to polygons. */

	void *compositor_data;
};

				   
static void
wl_surface_destroy(struct wl_client *client,
		   struct wl_surface *surface)
{
	struct wl_compositor_interface *interface;

	interface = client->display->compositor->interface;
	interface->notify_surface_destroy(client->display->compositor, surface);
}

static void
wl_surface_attach(struct wl_client *client,
		  struct wl_surface *surface, uint32_t name, 
		  uint32_t width, uint32_t height, uint32_t stride)
{
	struct wl_compositor_interface *interface;

	interface = client->display->compositor->interface;
	interface->notify_surface_attach(client->display->compositor,
					 surface, name, width, height, stride);
}

static const struct wl_argument attach_arguments[] = {
	{ WL_ARGUMENT_UINT32 },
	{ WL_ARGUMENT_UINT32 },
	{ WL_ARGUMENT_UINT32 },
	{ WL_ARGUMENT_UINT32 },
};

void
wl_surface_map(struct wl_client *client, struct wl_surface *surface,
	       int32_t x, int32_t y, int32_t width, int32_t height)
{
	struct wl_compositor_interface *interface;

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

static const struct wl_argument map_arguments[] = {
	{ WL_ARGUMENT_UINT32 },
	{ WL_ARGUMENT_UINT32 },
	{ WL_ARGUMENT_UINT32 },
	{ WL_ARGUMENT_UINT32 },
};

static const struct wl_method surface_methods[] = {
	{ "destroy", wl_surface_destroy,
	  0, NULL },
	{ "attach", wl_surface_attach,
	  ARRAY_LENGTH(attach_arguments), attach_arguments },
	{ "map", wl_surface_map,
	  ARRAY_LENGTH(map_arguments), map_arguments }
};

static const struct wl_interface surface_interface = {
	"surface", 1,
	ARRAY_LENGTH(surface_methods),
	surface_methods,
};

struct wl_surface *
wl_surface_create(struct wl_display *display, uint32_t id)
{
	struct wl_surface *surface;
	struct wl_compositor_interface *interface;

	surface = malloc(sizeof *surface);
	if (surface == NULL)
		return NULL;

	surface->base.id = id;
	surface->base.interface = &surface_interface;

	interface = display->compositor->interface;
	interface->notify_surface_create(display->compositor, surface);

	return surface;
}

void
wl_surface_set_data(struct wl_surface *surface, void *data)
{
	surface->compositor_data = data;
}

void *
wl_surface_get_data(struct wl_surface *surface)
{
	return surface->compositor_data;
}

static void
wl_client_data(int fd, uint32_t mask, void *data);
static void
wl_client_write(struct wl_client *client, const void *data, size_t count);

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
	wl_client_write(client, p, sizeof p);
	wl_client_write(client, interface->name, length);
	wl_client_write(client, pad, -length & 3);
}

struct wl_client *
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
					      wl_client_data, client);

	advertise_object(client, &display->base);

	return client;
}

void
wl_client_destroy(struct wl_client *client)
{
	printf("disconnect from client %p\n", client);
	wl_event_loop_remove_source(client->display->loop, client->source);
	free(client);
}

static void
wl_client_copy(struct wl_client *client, void *data, size_t size)
{
	int tail, rest;

	tail = client->in.tail;
	if (tail + size <= ARRAY_LENGTH(client->in.data)) {
		memcpy(data, client->in.data + tail, size);
	} else { 
		rest = ARRAY_LENGTH(client->in.data) - tail;
		memcpy(data, client->in.data + tail, rest);
		memcpy(data + rest, client->in.data, size - rest);
	}
}

static void
wl_client_consume(struct wl_client *client, size_t size)
{
	int tail, rest;

	tail = client->in.tail;
	if (tail + size <= ARRAY_LENGTH(client->in.data)) {
		client->in.tail += size;
	} else { 
		rest = ARRAY_LENGTH(client->in.data) - tail;
		client->in.tail = size - rest;
	}
}

static void
wl_client_demarshal(struct wl_client *client, struct wl_object *target,
		    const struct wl_method *method, size_t size)
{
	ffi_type *types[10];
	ffi_cif cif;
	uint32_t *p, result;
	int i;
	union {
		uint32_t uint32;
		const char *string;
		void *object;
		uint32_t new_id;
	} values[10];
	void *args[10];
	struct wl_object *object;
	uint32_t data[64];

	if (method->argument_count > ARRAY_LENGTH(types)) {
		printf("too many args (%d)\n", method->argument_count);
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

	wl_client_copy(client, data, size);
	p = &data[2];
	for (i = 0; i < method->argument_count; i++) {
		switch (method->arguments[i].type) {
		case WL_ARGUMENT_UINT32:
			types[i + 2] = &ffi_type_uint32;
			values[i + 2].uint32 = *p;
			p++;
			break;
		case WL_ARGUMENT_STRING:
			types[i + 2] = &ffi_type_pointer;
			/* FIXME */
			values[i + 2].uint32 = *p++;
			break;
		case WL_ARGUMENT_OBJECT:
			types[i + 2] = &ffi_type_pointer;
			object = wl_hash_lookup(&client->display->objects, *p);
			if (object == NULL)
				printf("unknown object (%d)\n", *p);
			if (object->interface != method->arguments[i].data)
				printf("wrong object type\n");
			values[i + 2].object = object;
			p++;
			break;
		case WL_ARGUMENT_NEW_ID:
			types[i + 2] = &ffi_type_uint32;
			values[i + 2].new_id = *p;
			object = wl_hash_lookup(&client->display->objects, *p);
			if (object != NULL)
				printf("object already exists (%d)\n", *p);
			p++;
			break;
		default:
			printf("unknown type\n");
			break;
		}
		args[i + 2] = &values[i + 2];
	}

	ffi_prep_cif(&cif, FFI_DEFAULT_ABI, method->argument_count + 2,
		     &ffi_type_uint32, types);
	ffi_call(&cif, FFI_FN(method->func), &result, args);
}

static void
wl_client_write(struct wl_client *client, const void *data, size_t count)
{
	size_t size;
	int head;

	head = client->out.head;
	if (head + count <= ARRAY_LENGTH(client->out.data)) {
		memcpy(client->out.data + head, data, count);
		client->out.head += count;
	} else {
		size = ARRAY_LENGTH(client->out.data) - head;
		memcpy(client->out.data + head, data, size);
		memcpy(client->out.data, data + size, count - size);
		client->out.head = count - size;
	}

	if (client->out.tail == head)
		wl_event_loop_update_source(client->display->loop,
					    client->source,
					    WL_EVENT_READABLE |
					    WL_EVENT_WRITEABLE);
}


static void
wl_client_event(struct wl_client *client, struct wl_object *object, uint32_t event)
{
	const struct wl_interface *interface;
	uint32_t p[2];

	interface = object->interface;

	p[0] = object->id;
	p[1] = event | (8 << 16);
	wl_client_write(client, p, sizeof p);
}

#define WL_DISPLAY_INVALID_OBJECT 0
#define WL_DISPLAY_INVALID_METHOD 1

static void
wl_client_process_input(struct wl_client *client)
{
	const struct wl_method *method;
	struct wl_object *object;
	uint32_t p[2], opcode, size, available;

	/* We know we have data in the buffer at this point, so if
	 * head equals tail, it means the buffer is full. */

	available = client->in.head - client->in.tail;
	if (available <= 0)
		available = sizeof client->in.data;

	while (available > sizeof p) {
		wl_client_copy(client, p, sizeof p);
		opcode = p[1] & 0xffff;
		size = p[1] >> 16;
		if (available < size)
			break;

		object = wl_hash_lookup(&client->display->objects,
					p[0]);
		if (object == NULL) {
			wl_client_event(client, &client->display->base,
					WL_DISPLAY_INVALID_OBJECT);
			wl_client_consume(client, size);
			available -= size;
			continue;
		}
				
		if (opcode >= object->interface->method_count) {
			wl_client_event(client, &client->display->base,
					WL_DISPLAY_INVALID_METHOD);
			wl_client_consume(client, size);
			available -= size;
			continue;
		}
				
		method = &object->interface->methods[opcode];
		wl_client_demarshal(client, object, method, size);
		wl_client_consume(client, size);
		available -= size;
	}
}

static void
wl_client_data(int fd, uint32_t mask, void *data)
{
	struct wl_client *client = data;
	struct iovec iov[2];
	int len, head, tail, count, size;

	if (mask & WL_EVENT_READABLE) {
		head = client->in.head;
		if (head < client->in.tail) {
			iov[0].iov_base = client->in.data + head;
			iov[0].iov_len = client->in.tail - head;
			count = 1;
		} else {
			size = ARRAY_LENGTH(client->out.data) - head;
			iov[0].iov_base = client->in.data + head;
			iov[0].iov_len = size;
			iov[1].iov_base = client->in.data;
			iov[1].iov_len = client->in.tail;
			count = 2;
		}
		len = readv(fd, iov, count);
		if (len < 0) {
			fprintf(stderr,
				"read error from client %p: %m (%d)\n",
				client, errno);
			wl_client_destroy(client);
		} else if (len == 0) {
			wl_client_destroy(client);
		} else if (head + len <= ARRAY_LENGTH(client->in.data)) {
			client->in.head += len;
		} else {
			client->in.head =
				head + len - ARRAY_LENGTH(client->in.data);
		}

		if (client->in.head != client->in.tail)
			wl_client_process_input(client);
	}

	if (mask & WL_EVENT_WRITEABLE) {
		tail = client->out.tail;
		if (tail < client->out.head) {
			iov[0].iov_base = client->out.data + tail;
			iov[0].iov_len = client->out.head - tail;
			count = 1;
		} else {
			size = ARRAY_LENGTH(client->out.data) - tail;
			iov[0].iov_base = client->out.data + tail;
			iov[0].iov_len = size;
			iov[1].iov_base = client->out.data;
			iov[1].iov_len = client->out.head;
			count = 2;
		}
		len = writev(fd, iov, count);
		if (len < 0) {
			fprintf(stderr, "write error for client %p: %m\n", client);
			wl_client_destroy(client);
		} else if (tail + len <= ARRAY_LENGTH(client->out.data)) {
			client->out.tail += len;
		} else {
			client->out.tail =
				tail + len - ARRAY_LENGTH(client->out.data);
		}

		/* We just took data out of the buffer, so at this
		 * point if head equals tail, the buffer is empty. */

		if (client->out.tail == client->out.head)
			wl_event_loop_update_source(client->display->loop,
						    client->source,
						    WL_EVENT_READABLE);
	}
}

static int
wl_display_create_surface(struct wl_client *client,
			  struct wl_display *display, uint32_t id)
{
	struct wl_surface *surface;

	surface = wl_surface_create(display, id);
	wl_hash_insert(&display->objects, &surface->base);

	/* FIXME: garbage collect client resources when client exits. */

	return 0;
}

static const struct wl_argument create_surface_arguments[] = {
	{ WL_ARGUMENT_NEW_ID }
};

static const struct wl_method display_methods[] = {
	{ "create_surface", wl_display_create_surface,
	  ARRAY_LENGTH(create_surface_arguments), create_surface_arguments },
};

static const struct wl_event display_events[] = {
	{ "invalid_object" },
	{ "invalid_method" },
};

static const struct wl_interface display_interface = {
	"display", 1,
	ARRAY_LENGTH(display_methods), display_methods,
	ARRAY_LENGTH(display_events), display_events,
};

struct wl_display *
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

	display->base.id = 0;
	display->base.interface = &display_interface;
	wl_hash_insert(&display->objects, &display->base);

	return display;		
}

void
wl_display_set_compositor(struct wl_display *display,
			  struct wl_compositor *compositor)
{
	display->compositor = compositor;
}

void
wl_display_run(struct wl_display *display)
{
	while (1)
		wl_event_loop_wait(display->loop);
}

/* The plan here is to generate a random anonymous socket name and
 * advertise that through a service on the session dbus.
 */
static const char socket_name[] = "\0wayland";

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

int
wl_display_add_socket(struct wl_display *display)
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


int main(int argc, char *argv[])
{
	struct wl_display *display;
	struct wl_compositor *compositor;

	display = wl_display_create();

	if (wl_display_add_socket(display)) {
		fprintf(stderr, "failed to add socket: %m\n");
		exit(EXIT_FAILURE);
	}

	compositor = wl_compositor_create();
	wl_display_set_compositor(display, compositor);

	printf("wayland online, display is %p\n", display);

	wl_display_run(display);

	return 0;
}
