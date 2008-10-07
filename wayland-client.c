#include <stdlib.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <ctype.h>
#include <sys/poll.h>

static const char socket_name[] = "\0wayland";

struct wl_buffer {
	char data[4096];
	int head, tail;
};

struct wl_connection {
	int fd;
	struct wl_buffer in, out;
	struct wl_display *display;
	uint32_t id;
};

struct wl_proxy {
	struct wl_connection *connection;
	uint32_t id;
};

struct wl_display {
	struct wl_proxy proxy;
};

struct wl_surface {
	struct wl_proxy proxy;
};

struct wl_connection *
wl_connection_create(const char *address)
{
	struct wl_connection *connection;
	struct wl_display *display;
	struct sockaddr_un name;
	socklen_t size;
	char buffer[256];
	uint32_t id, length;

	connection = malloc(sizeof *connection);
	if (connection == NULL)
		return NULL;

	memset(connection, 0, sizeof *connection);
	connection->id = 256; /* Need to get our id-range. */
	connection->fd = socket(PF_LOCAL, SOCK_STREAM, 0);
	if (connection->fd < 0) {
		free(connection);
		return NULL;
	}

	name.sun_family = AF_LOCAL;
	memcpy(name.sun_path, address, strlen(address + 1) + 2);

	size = offsetof (struct sockaddr_un, sun_path) + sizeof socket_name;

	if (connect (connection->fd, (struct sockaddr *) &name, size) < 0) {
		close(connection->fd);
		free(connection);
		return NULL;
	}

	/* FIXME: actually discover advertised objects here. */
	read(connection->fd, &id, sizeof id);
	read(connection->fd, &length, sizeof length);
	read(connection->fd, buffer, (length + 3) & ~3);

	display = malloc(sizeof *display);
	display->proxy.connection = connection;
	display->proxy.id = id;
	connection->display = display;

	return connection;
}

void
wl_connection_destroy(struct wl_connection *connection)
{
	close(connection->fd);
	free(connection->display);
	free(connection);
}

int
wl_connection_get_fd(struct wl_connection *connection)
{
	return connection->fd;
}

static void
handle_event(struct wl_connection *connection)
{
	struct wl_buffer *b;
	uint32_t *p, opcode, size;

	b = &connection->in;
	p = (uint32_t *) (b->data + b->tail);
	opcode = p[1] & 0xffff;
	size = p[1] >> 16;
	printf("signal from object %d, opcode %d, size %d\n",
	       p[0], opcode, size);
	b->tail += size;
}

void
wl_connection_iterate(struct wl_connection *connection)
{
	struct wl_buffer *b;
	uint32_t *p, opcode, size;
	int len;

	b = &connection->in;
	len = read(connection->fd, b->data + b->head, sizeof b->data);
	b->head += len;
	while (len > 0) {

		if (b->head - b->tail < 8)
			break;
		
		p = (uint32_t *) (b->data + b->tail);
		opcode = p[1] & 0xffff;
		size = p[1] >> 16;
		if (b->head - b->tail < size)
			break;

		handle_event(connection);
	}

	if (len < 0) {
		fprintf(stderr, "read error: %m\n");
		exit(EXIT_FAILURE);
	}
}

int
wl_connection_flush(struct wl_connection *connection)
{
	struct wl_buffer *b;
	int len;

	b = &connection->out;
	if (b->head == b->tail)
		return 0;

	len = write(connection->fd, b->data + b->tail, b->head - b->tail);
	b->tail += len;

	return len;
};

struct wl_display *
wl_connection_get_display(struct wl_connection *connection)
{
	return connection->display;
}

#define WL_DISPLAY_CREATE_SURFACE 0

struct wl_surface *
wl_display_create_surface(struct wl_display *display)
{
	struct wl_surface *surface;
	struct wl_connection *connection;
	uint32_t request[3];

	surface = malloc(sizeof *surface);
	if (surface == NULL)
		return NULL;

	connection = display->proxy.connection;
	surface->proxy.id = connection->id++;
	surface->proxy.connection = connection;

	request[0] = display->proxy.id;
	request[1] = WL_DISPLAY_CREATE_SURFACE | ((sizeof request) << 16);
	request[2] = surface->proxy.id;

	memcpy(connection->out.data + connection->out.head,
	       request, sizeof request);
	connection->out.head += sizeof request;

	return surface;
}

#define WL_SURFACE_ATTACH 0

void wl_surface_attach(struct wl_surface *surface,
		       uint32_t name, int width, int height, int stride)
{
	uint32_t request[6];
	struct wl_connection *connection;

	request[0] = surface->proxy.id;
	request[1] = WL_SURFACE_ATTACH | ((sizeof request) << 16);
	request[2] = name;
	request[3] = width;
	request[4] = height;
	request[5] = stride;

	connection = surface->proxy.connection;
	memcpy(connection->out.data + connection->out.head,
	       request, sizeof request);
	connection->out.head += sizeof request;
}
