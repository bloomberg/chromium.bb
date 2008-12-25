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
#include <assert.h>
#include <sys/poll.h>

#include "wayland-protocol.h"
#include "connection.h"
#include "wayland-util.h"
#include "wayland-client.h"

static const char socket_name[] = "\0wayland";

struct wl_global {
	uint32_t id;
	char *interface;
	uint32_t version;
	struct wl_list link;
};

struct wl_proxy {
	struct wl_object base;
	struct wl_display *display;
};

struct wl_display {
	struct wl_proxy proxy;
	struct wl_connection *connection;
	int fd;
	uint32_t id, id_count, next_range;
	uint32_t mask;
	struct wl_hash *objects;
	struct wl_list global_list;
	struct wl_list visual_list;

	wl_display_update_func_t update;
	void *update_data;

	wl_display_event_func_t event_handler;
	void *event_handler_data;

	struct wl_output *output;
};

struct wl_compositor {
	struct wl_proxy proxy;
};

struct wl_surface {
	struct wl_proxy proxy;
};

struct wl_visual {
	struct wl_proxy proxy;
	struct wl_list link;
};

struct wl_output {
	struct wl_proxy proxy;
	int32_t width, height;
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

static void
output_handle_geometry(struct wl_display *display,
		       struct wl_output *output, int32_t width, int32_t height)
{
	output->width = width;
	output->height = height;
}

struct wl_output_listener {
	void (*geometry)(struct wl_display *display,
			 struct wl_output *output,
			 int32_t width, int32_t height);
};

static const struct wl_output_listener output_listener = {
	output_handle_geometry
};

static void
add_output(struct wl_display *display, struct wl_global *global)
{
	struct wl_output *output;

	output = malloc(sizeof *output);
	if (output == NULL)
		return;

	output->proxy.base.interface = &wl_output_interface;
	output->proxy.base.implementation = (void(**)(void)) &output_listener;
	output->proxy.base.id = global->id;
	output->proxy.display = display;
	display->output = output;
	wl_hash_insert(display->objects, &output->proxy.base);
}

WL_EXPORT void
wl_display_get_geometry(struct wl_display *display, int32_t *width, int32_t *height)
{
	*width = display->output->width;
	*height = display->output->height;
}

static void
add_visual(struct wl_display *display, struct wl_global *global)
{
	struct wl_visual *visual;

	visual = malloc(sizeof *visual);
	if (visual == NULL)
		return;

	visual->proxy.base.interface = &wl_visual_interface;
	visual->proxy.base.id = global->id;
	visual->proxy.base.implementation = NULL;
	visual->proxy.display = display;
	wl_list_insert(display->visual_list.prev, &visual->link);
	wl_hash_insert(display->objects, &visual->proxy.base);
}

WL_EXPORT struct wl_visual *
wl_display_get_argb_visual(struct wl_display *display)
{
	return container_of(display->visual_list.next,
			    struct wl_visual, link);
}

WL_EXPORT struct wl_visual *
wl_display_get_premultiplied_argb_visual(struct wl_display *display)
{
	return container_of(display->visual_list.next->next,
			    struct wl_visual, link);
}

WL_EXPORT struct wl_visual *
wl_display_get_rgb_visual(struct wl_display *display)
{
	/* FIXME: Where's cddar when you need it... */

	return container_of(display->visual_list.next->next->next,
			    struct wl_visual, link);
}

static void
display_handle_invalid_object(struct wl_display *display,
			      struct wl_object *object, uint32_t id)
{
	fprintf(stderr, "sent request to invalid object\n");
}
			      
static void
display_handle_invalid_method(struct wl_display *display,
			      struct wl_object *object,
			      uint32_t id, uint32_t opcode)
{
	fprintf(stderr, "sent invalid request opcode\n");
}

static void
display_handle_no_memory(struct wl_display *display,
			 struct wl_object *object)
{
	fprintf(stderr, "server out of memory\n");
}

static void
display_handle_global(struct wl_display *display,
		      struct wl_object *object,
		      uint32_t id, const char *interface, uint32_t version)
{
	struct wl_global *global;

	global = malloc(sizeof *global);
	if (global == NULL)
		return;

	global->id = id;
	global->interface = strdup(interface);
	global->version = version;
	wl_list_insert(display->global_list.prev, &global->link);
	if (strcmp(global->interface, "display") == 0)
		wl_hash_insert(display->objects, &display->proxy.base);
	if (strcmp(global->interface, "visual") == 0)
		add_visual(display, global);
	else if (strcmp(global->interface, "output") == 0)
		add_output(display, global);
}

static void
display_handle_range(struct wl_display *display,
		     struct wl_object *object, uint32_t range)
{
	display->next_range = range;
}

struct wl_display_listener {
	void (*invalid_object)(struct wl_display *display,
			       struct wl_object *object, uint32_t id);
	void (*invalid_method)(struct wl_display *display,
			       struct wl_object *object,
			       uint32_t id, uint32_t opcode);
	void (*no_memory)(struct wl_display *display, struct wl_object *object);
	void (*global)(struct wl_display *display, struct wl_object *object,
		       uint32_t id, const char *interface, uint32_t version);
	void (*range)(struct wl_display *display,
		      struct wl_object *object, uint32_t range);
};

static const struct wl_display_listener display_listener = {
	display_handle_invalid_object,
	display_handle_invalid_method,
	display_handle_no_memory,
	display_handle_global,
	display_handle_range
};

WL_EXPORT struct wl_display *
wl_display_create(const char *name, size_t name_size)
{
	struct wl_display *display;
	struct sockaddr_un addr;
	socklen_t size;

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

	display->objects = wl_hash_create();
	wl_list_init(&display->global_list);
	wl_list_init(&display->visual_list);

	display->proxy.base.interface = &wl_display_interface;
	display->proxy.base.implementation = (void(**)(void)) &display_listener;
	display->proxy.base.id = 1;
	display->proxy.display = display;

	display->connection = wl_connection_create(display->fd,
						   connection_update,
						   display);

	/* Process connection events. */
	wl_display_iterate(display, WL_CONNECTION_READABLE);

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
wl_display_get_object_id(struct wl_display *display,
			 const char *interface, uint32_t version)
{
	struct wl_global *global;

	global = container_of(display->global_list.next,
			      struct wl_global, link);
	while (&global->link != &display->global_list) {
		if (strcmp(global->interface, interface) == 0 &&
		    global->version >= version)
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
	     uint32_t id, uint32_t opcode, uint32_t size)
{
	uint32_t p[32];
	struct wl_object *object;

	wl_connection_copy(display->connection, p, size);
	if (id == 1)
		object = &display->proxy.base;
	else
		object = wl_hash_lookup(display->objects, id);

	if (object != NULL)
		wl_connection_demarshal(display->connection,
					size,
					display->objects,
					object->implementation[opcode],
					display,
					object, 
					&object->interface->events[opcode]);
	else if (display->event_handler != NULL)
		display->event_handler(display, id,
				       opcode, size, p + 2,
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
	if (display->id_count == 0) {
		display->id_count = 256;
		display->id = display->next_range;
	}

	display->id_count--;

	return display->id++;
}

WL_EXPORT void
wl_display_write(struct wl_display *display, const void *data, size_t count)
{
	wl_connection_write(display->connection, data, count);
}

WL_EXPORT struct wl_compositor *
wl_display_get_compositor(struct wl_display *display)
{
	struct wl_compositor *compositor;
	uint32_t id;

	id = wl_display_get_object_id(display, "compositor", 1);
	if (id == 0)
		return NULL;

	compositor = malloc(sizeof *compositor);
	compositor->proxy.base.interface = &wl_compositor_interface;
	compositor->proxy.base.id = id;
	compositor->proxy.display = display;

	return compositor;
}

static void
wl_proxy_marshal(struct wl_proxy *proxy, uint32_t opcode, ...)
{
	va_list ap;

	va_start(ap, opcode);
	wl_connection_vmarshal(proxy->display->connection,
			       &proxy->base, opcode, ap,
			       &proxy->base.interface->methods[opcode]);
	va_end(ap);

}

WL_EXPORT struct wl_surface *
wl_compositor_create_surface(struct wl_compositor *compositor)
{
	struct wl_surface *surface;

	surface = malloc(sizeof *surface);
	if (surface == NULL)
		return NULL;

	surface->proxy.base.interface = &wl_surface_interface;
	surface->proxy.base.id = wl_display_allocate_id(compositor->proxy.display);
	surface->proxy.display = compositor->proxy.display;
	wl_proxy_marshal(&compositor->proxy,
			  WL_COMPOSITOR_CREATE_SURFACE, surface);

	return surface;
}

WL_EXPORT void
wl_compositor_commit(struct wl_compositor *compositor, uint32_t key)
{
	wl_proxy_marshal(&compositor->proxy, WL_COMPOSITOR_COMMIT, key);
}

WL_EXPORT void
wl_surface_destroy(struct wl_surface *surface)
{
	wl_proxy_marshal(&surface->proxy, WL_SURFACE_DESTROY);
}

WL_EXPORT void
wl_surface_attach(struct wl_surface *surface, uint32_t name,
		  int32_t width, int32_t height, uint32_t stride,
		  struct wl_visual *visual)
{
	wl_proxy_marshal(&surface->proxy, WL_SURFACE_ATTACH,
			 name, width, height, stride, visual);
}

WL_EXPORT void
wl_surface_map(struct wl_surface *surface,
	       int32_t x, int32_t y, int32_t width, int32_t height)
{
	wl_proxy_marshal(&surface->proxy,
			 WL_SURFACE_MAP, x, y, width, height);
}

WL_EXPORT void
wl_surface_copy(struct wl_surface *surface, int32_t dst_x, int32_t dst_y,
		uint32_t name, uint32_t stride,
		int32_t x, int32_t y, int32_t width, int32_t height)
{
	wl_proxy_marshal(&surface->proxy, WL_SURFACE_COPY,
			 dst_x, dst_y, name, stride, x, y, width, height);
}

WL_EXPORT void
wl_surface_damage(struct wl_surface *surface,
		  int32_t x, int32_t y, int32_t width, int32_t height)
{
	wl_proxy_marshal(&surface->proxy,
			 WL_SURFACE_DAMAGE, x, y, width, height);
}
