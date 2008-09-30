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

struct wl_client {
	char protocol_buffer[4096];
	struct wl_event_source *source;
	struct wl_display *display;
};

struct wl_display {
	struct wl_object base;
	struct wl_event_loop *loop;
	struct wl_hash objects;
};

struct wl_surface {
	struct wl_object base;

	/* provided by client */
	int width, height;
	int buffer;
	int stride;
	
	/* these are used by the wayland server, not set from client */
	void (*screen_to_surface)(int sx, int sy, int *wx, int *wy);
	void (*surface_to_screen)(int sx, int sy, int *wx, int *wy);

	/* how to convert buffer contents to pixels in screen format;
	 * yuv->rgb, indexed->rgb, svg->rgb, but mostly just rgb->rgb. */

	/* how to transform/render rectangular contents to polygons. */
};

static void
wl_surface_get_interface(struct wl_client *client,
			 struct wl_surface *surface,
			 const char *interface, uint32_t id)
{
	/* client sends a new object id, and an interface identifier
	 * to name the object that implements the interface */
	printf("surface::get_interface\n");
}

static const struct wl_argument get_interface_arguments[] = {
	{ WL_ARGUMENT_STRING },
	{ WL_ARGUMENT_NEW_ID }
};

static void
wl_surface_post(struct wl_client *client,
		struct wl_surface *surface, uint32_t name, 
		uint32_t width, uint32_t height, uint32_t stride)
{
	printf("surface::post\n");
}

static const struct wl_argument post_arguments[] = {
	{ WL_ARGUMENT_UINT32 },
	{ WL_ARGUMENT_UINT32 },
	{ WL_ARGUMENT_UINT32 },
	{ WL_ARGUMENT_UINT32 },
};

void
wl_surface_update(struct wl_surface *surface,
		  uint32_t source_name, struct wl_region *region)
{
	/* FIXME: how to demarshal region? */
	/* copy region from buffer into current window contents. */
}

static const struct wl_argument update_arguments[] = {
	{ WL_ARGUMENT_UINT32 },
	{ WL_ARGUMENT_UINT32 },
};

static const struct wl_method surface_methods[] = {
	{ "get_interface", wl_surface_get_interface,
	  ARRAY_LENGTH(get_interface_arguments), get_interface_arguments },
	{ "post", wl_surface_post,
	  ARRAY_LENGTH(post_arguments), post_arguments }
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

	surface = malloc(sizeof *surface);
	if (surface == NULL)
		return NULL;

	surface->base.id = id;
	surface->base.interface = &surface_interface;
	/* doesn't create any pixel buffers, just the wl_surface
	 * object.  the client allocates and attaches pixel buffers
	 * itself and renders to them, then posts them using
	 * wl_window_post. */

	/* add to display list */

	return surface;
}

static void
wl_client_data(int fd, uint32_t mask, void *data);

struct wl_client *
wl_client_create(struct wl_display *display, int fd)
{
	struct wl_client *client;

	client = malloc(sizeof *client);
	if (client == NULL)
		return NULL;

	client->display = display;

	client->source = wl_event_loop_add_fd(display->loop, fd,
					      WL_EVENT_READABLE |
					      WL_EVENT_READABLE,
					      wl_client_data, client);

	printf("new client: %p\n", client);

	/* Send global objects to client in hand shake response, eg:
	 *
	 *   display: 0,
	 *   glyph_cache: 1
	 *   some other extension global object: 2
	 *
	 * etc */

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
	  const struct wl_method *method, uint32_t *data, int len)
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
			printf("got uint32 (%d)\n", *p);
			p++;
			break;
		case WL_ARGUMENT_STRING:
			printf("got string\n");
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
			printf("got object (%d)\n", *p);
			p++;
			break;
		case WL_ARGUMENT_NEW_ID:
			types[i + 2] = &ffi_type_uint32;
			values[i + 2].new_id = *p;
			object = wl_hash_lookup(&client->display->objects, *p);
			if (object != NULL)
				printf("object already exists (%d)\n", *p);
			printf("got new_id (%d)\n", *p);
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
wl_client_data(int fd, uint32_t mask, void *data)
{
	struct wl_client *client = data;
	struct wl_object *object;
	const struct wl_method *method;
	char buffer[256];
	uint32_t *p;
	int len;

	if (mask & WL_EVENT_READABLE) {
		len = read(fd, buffer, sizeof buffer);
		if (len == 0) {
			wl_client_destroy(client);
		} else {
			printf("got %d bytes from client %p\n",
			       len, client);
			if (len < 2 * sizeof *p)
				/* read more... */
				return;

			p = (uint32_t *) buffer;
			object = wl_hash_lookup(&client->display->objects,
						p[0]);
			if (object == NULL) {
				/* send error */
				printf("invalid object\n");
				return;
			}

			if (p[1] >= object->interface->method_count) {
				/* send error */
				printf("invalid method\n");
				return;
			}

			method = &object->interface->methods[p[1]];
			printf("calling method %s on interface %s\n",
			       method->name, object->interface->name);
			demarshal(client, object, method, p + 2, len - 8);
		}
	}

	if (mask & WL_EVENT_WRITEABLE) {
	}
}

static void
wl_display_get_interface(struct wl_client *client,
			 struct wl_display *display,
			 const char *interface, uint32_t id)
{
	/* client sends a new object id, and an interface identifier
	 * to name the object that implements the interface */
	printf("display::get_interface\n");
}

static int
wl_display_create_surface(struct wl_client *client,
			  struct wl_display *display, uint32_t id)
{
	struct wl_surface *surface;

	printf("display::create_surface, client %p, display %p, new_id=%d\n",
	       client, display, id);

	surface = wl_surface_create(display, id);
	wl_hash_insert(&display->objects, &surface->base);

	return 0;
}

static const struct wl_argument create_surface_arguments[] = {
	{ WL_ARGUMENT_NEW_ID }
};

static const struct wl_method display_methods[] = {
	{ "get_interface", wl_display_get_interface,
	  ARRAY_LENGTH(get_interface_arguments), get_interface_arguments },
	{ "create_surface", wl_display_create_surface,
	  ARRAY_LENGTH(create_surface_arguments), create_surface_arguments },
};

static const struct wl_interface display_interface = {
	"display", 1, ARRAY_LENGTH(display_methods), display_methods,
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

	wl_event_loop_add_fd(display->loop, sock, WL_EVENT_READABLE,
			     socket_data, display);

	return 0;
}


int main(int argc, char *argv[])
{
	struct wl_display *display;

	display = wl_display_create();

	if (wl_display_add_socket(display)) {
		fprintf(stderr, "failed to add socket: %m\n");
		exit(EXIT_FAILURE);
	}

	printf("wayland online, display is %p\n", display);

	wl_display_run(display);

	return 0;
}
