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
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <ctype.h>
#include <sys/poll.h>

#include "connection.h"
#include "wayland-util.h"
#include "wayland-client.h"

static const char socket_name[] = "\0wayland";

struct wl_global {
	uint32_t id;
	char *interface;
	struct wl_list link;
};

struct wl_proxy {
	struct wl_display *display;
	uint32_t id;
};

struct wl_display {
	struct wl_proxy proxy;
	struct wl_connection *connection;
	int fd;
	uint32_t id;
	uint32_t mask;
	struct wl_list global_list;

	wl_display_update_func_t update;
	void *update_data;

	wl_display_event_func_t event_handler;
	void *event_handler_data;
};

struct wl_surface {
	struct wl_proxy proxy;
};

static int
connection_update(struct wl_connection *connection,
		  uint32_t mask, void *data)
{
	struct wl_display *display = data;

	display->mask = mask;
	if (display->update)
		return display->update(display->mask,
				       display->update_data);

	return 0;
}

WL_EXPORT struct wl_display *
wl_display_create(const char *name, size_t name_size)
{
	struct wl_display *display;
	struct wl_global *global;
	struct sockaddr_un addr;
	socklen_t size;
	char buffer[256];
	uint32_t id, length, count, i;

	display = malloc(sizeof *display);
	if (display == NULL)
		return NULL;

	memset(display, 0, sizeof *display);
	display->fd = socket(PF_LOCAL, SOCK_STREAM, 0);
	if (display->fd < 0) {
		free(display);
		return NULL;
	}

	addr.sun_family = AF_LOCAL;
	memcpy(addr.sun_path, name, name_size);

	size = offsetof (struct sockaddr_un, sun_path) + name_size;

	if (connect(display->fd, (struct sockaddr *) &addr, size) < 0) {
		close(display->fd);
		free(display);
		return NULL;
	}

	/* FIXME: We'll need a protocol for getting a new range, I
	 * guess... */
	read(display->fd, &display->id, sizeof display->id);

	read(display->fd, &count, sizeof count);

	wl_list_init(&display->global_list);
	for (i = 0; i < count; i++) {
		/* FIXME: actually discover advertised objects here. */
		read(display->fd, &id, sizeof id);
		read(display->fd, &length, sizeof length);
		read(display->fd, buffer, (length + 3) & ~3);

		global = malloc(sizeof *global);
		if (global == NULL)
			return NULL;

		global->id = id;
		global->interface = malloc(length + 1);
		memcpy(global->interface, buffer, length);
		global->interface[length] = '\0';
		wl_list_insert(display->global_list.prev, &global->link);
	}

	display->proxy.display = display;
	display->proxy.id = wl_display_get_object_id(display, "display");

	display->connection = wl_connection_create(display->fd,
						   connection_update,
						   display);

	return display;
}

WL_EXPORT void
wl_display_destroy(struct wl_display *display)
{
	wl_connection_destroy(display->connection);
	close(display->fd);
	free(display);
}

WL_EXPORT uint32_t
wl_display_get_object_id(struct wl_display *display, const char *interface)
{
	struct wl_global *global;

	global = container_of(display->global_list.next,
			      struct wl_global, link);
	while (&global->link != &display->global_list) {
		if (strcmp(global->interface, interface) == 0)
			return global->id;

		global = container_of(global->link.next,
				      struct wl_global, link);
	}

	return 0;
}

WL_EXPORT int
wl_display_get_fd(struct wl_display *display,
		  wl_display_update_func_t update, void *data)
{
	display->update = update;
	display->update_data = data;

	display->update(display->mask, display->update_data);

	return display->fd;
}

static void
handle_event(struct wl_display *display,
	     uint32_t object, uint32_t opcode, uint32_t size)
{
	uint32_t p[10];

	wl_connection_copy(display->connection, p, size);
	if (display->event_handler != NULL)
		display->event_handler(display, object, opcode, size, p + 2,
				       display->event_handler_data);
	wl_connection_consume(display->connection, size);
}

WL_EXPORT void
wl_display_iterate(struct wl_display *display, uint32_t mask)
{
	uint32_t p[2], object, opcode, size;
	int len;

	len = wl_connection_data(display->connection, mask);
	while (len > 0) {
		if (len < sizeof p)
			break;
		
		wl_connection_copy(display->connection, p, sizeof p);
		object = p[0];
		opcode = p[1] & 0xffff;
		size = p[1] >> 16;
		if (len < size)
			break;

		handle_event(display, object, opcode, size);
		len -= size;
	}

	if (len < 0) {
		fprintf(stderr, "read error: %m\n");
		exit(EXIT_FAILURE);
	}
}

WL_EXPORT void
wl_display_set_event_handler(struct wl_display *display,
			     wl_display_event_func_t handler,
			     void *data)
{
	/* FIXME: This needs something more generic... */
	display->event_handler = handler;
	display->event_handler_data = data;
}

WL_EXPORT uint32_t
wl_display_allocate_id(struct wl_display *display)
{
	return display->id++;
}

WL_EXPORT void
wl_display_write(struct wl_display *display, const void *data, size_t count)
{
	wl_connection_write(display->connection, data, count);
}

#define WL_DISPLAY_CREATE_SURFACE	0
#define WL_DISPLAY_COMMIT		1

WL_EXPORT struct wl_surface *
wl_display_create_surface(struct wl_display *display)
{
	struct wl_surface *surface;
	uint32_t request[3];

	surface = malloc(sizeof *surface);
	if (surface == NULL)
		return NULL;

	surface->proxy.id = wl_display_allocate_id(display);
	surface->proxy.display = display;

	request[0] = display->proxy.id;
	request[1] = WL_DISPLAY_CREATE_SURFACE | ((sizeof request) << 16);
	request[2] = surface->proxy.id;
	wl_connection_write(display->connection, request, sizeof request);

	return surface;
}

WL_EXPORT void
wl_display_commit(struct wl_display *display, uint32_t key)
{
	uint32_t request[3];

	request[0] = display->proxy.id;
	request[1] = WL_DISPLAY_COMMIT | ((sizeof request) << 16);
	request[2] = key;
	wl_connection_write(display->connection, request, sizeof request);
}

#define WL_SURFACE_DESTROY	0
#define WL_SURFACE_ATTACH	1
#define WL_SURFACE_MAP		2
#define WL_SURFACE_COPY		3
#define WL_SURFACE_DAMAGE	4

WL_EXPORT void
wl_surface_destroy(struct wl_surface *surface)
{
	uint32_t request[2];

	request[0] = surface->proxy.id;
	request[1] = WL_SURFACE_DESTROY | ((sizeof request) << 16);

	wl_connection_write(surface->proxy.display->connection,
			    request, sizeof request);
}

WL_EXPORT void
wl_surface_attach(struct wl_surface *surface, uint32_t name,
		  int32_t width, int32_t height, uint32_t stride)
{
	uint32_t request[6];

	request[0] = surface->proxy.id;
	request[1] = WL_SURFACE_ATTACH | ((sizeof request) << 16);
	request[2] = name;
	request[3] = width;
	request[4] = height;
	request[5] = stride;

	wl_connection_write(surface->proxy.display->connection,
			    request, sizeof request);
}

WL_EXPORT void
wl_surface_map(struct wl_surface *surface,
	       int32_t x, int32_t y, int32_t width, int32_t height)
{
	uint32_t request[6];

	request[0] = surface->proxy.id;
	request[1] = WL_SURFACE_MAP | ((sizeof request) << 16);
	request[2] = x;
	request[3] = y;
	request[4] = width;
	request[5] = height;

	wl_connection_write(surface->proxy.display->connection,
			    request, sizeof request);
}

WL_EXPORT void
wl_surface_copy(struct wl_surface *surface, int32_t dst_x, int32_t dst_y,
		uint32_t name, uint32_t stride,
		int32_t x, int32_t y, int32_t width, int32_t height)
{
	uint32_t request[10];

	request[0] = surface->proxy.id;
	request[1] = WL_SURFACE_COPY | ((sizeof request) << 16);
	request[2] = dst_x;
	request[3] = dst_y;
	request[4] = name;
	request[5] = stride;
	request[6] = x;
	request[7] = y;
	request[8] = width;
	request[9] = height;

	wl_connection_write(surface->proxy.display->connection,
			    request, sizeof request);
}

WL_EXPORT void
wl_surface_damage(struct wl_surface *surface,
		  int32_t x, int32_t y, int32_t width, int32_t height)
{
	uint32_t request[6];

	request[0] = surface->proxy.id;
	request[1] = WL_SURFACE_DAMAGE | ((sizeof request) << 16);
	request[2] = x;
	request[3] = y;
	request[4] = width;
	request[5] = height;

	wl_connection_write(surface->proxy.display->connection,
			    request, sizeof request);
}
