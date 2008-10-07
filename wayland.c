#include <stdlib.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
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
	struct wl_buffer input, output;
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

static int
advertise_object(struct wl_client *client, struct wl_object *object)
{
	const struct wl_interface *interface;
	uint32_t length, *p;

	interface = object->interface;
	p = (uint32_t *) (client->output.data + client->output.head);
	length = strlen(interface->name);
	*p++ = object->id;
	*p++ = length;
	memcpy(p, interface->name, length);
	memset((char *) p + length, 0, -length & 3);
	client->output.head += 8 + ((length + 3) & ~3);

	wl_event_loop_update_source(client->display->loop, client->source,
				    WL_EVENT_READABLE | WL_EVENT_WRITEABLE);


	return 0;
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
demarshal(struct wl_client *client, struct wl_object *target,
	  const struct wl_method *method, uint32_t *data)
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

	if (method->argument_count > ARRAY_LENGTH(types)) {
		printf("too many args (%d)\n", method->argument_count);
		return;
	}

	types[0] = &ffi_type_pointer;
	values[0].object = client;
	args[0] =  &values[0];

	types[1] = &ffi_type_pointer;
	values[1].object = target;
	args[1] =  &values[1];

	p = data;
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
wl_client_event(struct wl_client *client, struct wl_object *object, uint32_t event)
{
	const struct wl_interface *interface;
	uint32_t *p;

	interface = object->interface;

	p = (void *) client->output.data + client->output.head;
	p[0] = object->id;
	p[1] = event | (8 << 16);
	client->output.head += 8;
	wl_event_loop_update_source(client->display->loop, client->source,
				    WL_EVENT_READABLE | WL_EVENT_WRITEABLE);
}

#define WL_DISPLAY_INVALID_OBJECT 0
#define WL_DISPLAY_INVALID_METHOD 1

static void
wl_client_process_input(struct wl_client *client)
{
	const struct wl_method *method;
	struct wl_object *object;
	uint32_t *p, opcode, size;

	while (1) {
		if (client->input.head - client->input.tail < 2 * sizeof *p)
			break;

		p = (uint32_t *) (client->input.data + client->input.tail);
		opcode = p[1] & 0xffff;
		size = p[1] >> 16;
		if (client->input.head - client->input.tail < size)
			break;

		object = wl_hash_lookup(&client->display->objects,
					p[0]);
		if (object == NULL) {
			wl_client_event(client, &client->display->base,
					WL_DISPLAY_INVALID_OBJECT);
			client->input.tail += size;
			continue;
		}
				
		if (opcode >= object->interface->method_count) {
			wl_client_event(client, &client->display->base,
					WL_DISPLAY_INVALID_METHOD);
			client->input.tail += size;
			continue;
		}
				
		method = &object->interface->methods[opcode];
		demarshal(client, object, method, p + 2);
		client->input.tail += size;
	}
}

static void
wl_client_data(int fd, uint32_t mask, void *data)
{
	struct wl_client *client = data;
	int len;

	if (mask & WL_EVENT_READABLE) {
		len = read(fd, client->input.data, sizeof client->input.data);
		if (len > 0) {
			client->input.head += len;
			wl_client_process_input(client);
		} else if (len == 0 && errno == ECONNRESET) {
			fprintf(stderr,
				"read from client %p: %m (%d)\n",
				client, errno);
		} else {
				wl_client_destroy(client);
		}
	}

	if (mask & WL_EVENT_WRITEABLE) {
		len = write(fd, client->output.data + client->output.tail,
			    client->output.head - client->output.tail);
		client->output.tail += len;

		if (client->output.tail == client->output.head)
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
