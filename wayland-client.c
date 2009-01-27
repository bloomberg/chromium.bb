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
	struct wl_proxy *proxy;
	struct wl_list link;
};

struct wl_global_listener {
	wl_display_global_func_t handler;
	void *data;
	struct wl_list link;
};

struct wl_listener {
	void (**implementation)(void);
	void *data;
	struct wl_list link;
};

struct wl_proxy {
	struct wl_object base;
	struct wl_display *display;
	struct wl_list listener_list;
};

struct wl_compositor {
	struct wl_proxy proxy;
};

struct wl_surface {
	struct wl_proxy proxy;
};

struct wl_visual {
	struct wl_proxy proxy;
};

struct wl_output {
	struct wl_proxy proxy;
	struct wl_listener listener;
	int32_t width, height;
};

struct wl_input_device {
	struct wl_proxy proxy;
};

struct wl_display {
	struct wl_proxy proxy;
	struct wl_connection *connection;
	int fd;
	uint32_t id, id_count, next_range;
	uint32_t mask;
	struct wl_hash *objects;
	struct wl_list global_list;
	struct wl_listener listener;
	struct wl_list global_listener_list;

	struct wl_visual *argb_visual;
	struct wl_visual *premultiplied_argb_visual;
	struct wl_visual *rgb_visual;

	wl_display_update_func_t update;
	void *update_data;

	wl_display_global_func_t global_handler;
	void *global_handler_data;

	struct wl_compositor *compositor;
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

WL_EXPORT int
wl_object_implements(struct wl_object *object,
		     const char *interface, int version)
{
	return strcmp(object->interface->name, interface) == 0 &&
		object->interface->version >= version;
}

WL_EXPORT struct wl_global_listener *
wl_display_add_global_listener(struct wl_display *display,
			       wl_display_global_func_t handler, void *data)
{
	struct wl_global_listener *listener;
	struct wl_global *global;

	listener = malloc(sizeof *listener);
	if (listener == NULL)
		return NULL;

	listener->handler = handler;
	listener->data = data;
	wl_list_insert(display->global_listener_list.prev, &listener->link);

	/* FIXME: Need a destructor for void *data? */

	global = container_of(display->global_list.next,
			      struct wl_global, link);
	while (&global->link != &display->global_list) {
		if (global->proxy != NULL)
			(*handler)(display, &global->proxy->base, data);

		global = container_of(global->link.next,
				      struct wl_global, link);
	}

	return listener;
}

WL_EXPORT void
wl_display_remove_global_listener(struct wl_display *display,
				  struct wl_global_listener *listener)
{
	wl_list_remove(&listener->link);
	free(listener);
}

static struct wl_proxy *
wl_proxy_create_for_global(struct wl_display *display,
			   struct wl_global *global,
			   const struct wl_interface *interface)
{
	struct wl_proxy *proxy;
	struct wl_global_listener *listener;

	proxy = malloc(sizeof *proxy);
	if (proxy == NULL)
		return NULL;

	proxy->base.interface = interface;
	proxy->base.id = global->id;
	proxy->display = display;
	global->proxy = proxy;
	wl_list_init(&proxy->listener_list);
	wl_hash_insert(display->objects, &proxy->base);

	listener = container_of(display->global_listener_list.next,
				struct wl_global_listener, link);
	while (&listener->link != &display->global_listener_list) {
		(*listener->handler)(display, &proxy->base, listener->data);
		listener = container_of(listener->link.next,
					struct wl_global_listener, link);
	}

	return proxy;
}

static int
wl_proxy_add_listener(struct wl_proxy *proxy, void (**implementation)(void), void *data)
{
	struct wl_listener *listener;

	listener = malloc(sizeof *listener);
	if (listener == NULL)
		return -1;

	listener->implementation = (void (**)(void)) implementation;
	listener->data = data;
	wl_list_insert(proxy->listener_list.prev, &listener->link);

	return 0;
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

WL_EXPORT int
wl_output_add_listener(struct wl_output *output,
		       const struct wl_output_listener *listener,
		       void *data)
{
	return wl_proxy_add_listener(&output->proxy,
				     (void (**)(void)) listener, data);
}

static void
add_visual(struct wl_display *display, struct wl_global *global)
{
	struct wl_visual *visual;

	visual = (struct wl_visual *)
		wl_proxy_create_for_global(display, global,
					   &wl_visual_interface);
	if (display->argb_visual == NULL)
		display->argb_visual = visual;
	else if (display->premultiplied_argb_visual == NULL)
		display->premultiplied_argb_visual = visual;
	else
		display->rgb_visual = visual;
}

WL_EXPORT struct wl_visual *
wl_display_get_argb_visual(struct wl_display *display)
{
	return display->argb_visual;
}

WL_EXPORT struct wl_visual *
wl_display_get_premultiplied_argb_visual(struct wl_display *display)
{
	return display->premultiplied_argb_visual;
}

WL_EXPORT struct wl_visual *
wl_display_get_rgb_visual(struct wl_display *display)
{
	return display->rgb_visual;
}

WL_EXPORT int
wl_input_device_add_listener(struct wl_input_device *input_device,
			     const struct wl_input_device_listener *listener,
			     void *data)
{
	return wl_proxy_add_listener(&input_device->proxy,
				     (void (**)(void)) listener, data);
}

static void
display_handle_invalid_object(void *data,
			      struct wl_display *display, uint32_t id)
{
	fprintf(stderr, "sent request to invalid object\n");
}
			      
static void
display_handle_invalid_method(void *data, 
			      struct wl_display *display,
			      uint32_t id, uint32_t opcode)
{
	fprintf(stderr, "sent invalid request opcode\n");
}

static void
display_handle_no_memory(void *data,
			 struct wl_display *display)
{
	fprintf(stderr, "server out of memory\n");
}

static void
display_handle_global(void *data,
		      struct wl_display *display,
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
	else if (strcmp(global->interface, "compositor") == 0)
		display->compositor = (struct wl_compositor *) 
			wl_proxy_create_for_global(display, global,
						   &wl_compositor_interface);
	else if (strcmp(global->interface, "visual") == 0)
		add_visual(display, global);
	else if (strcmp(global->interface, "output") == 0)
		wl_proxy_create_for_global(display, global,
					   &wl_output_interface);
	else if (strcmp(global->interface, "input_device") == 0)
		wl_proxy_create_for_global(display, global,
					   &wl_input_device_interface);
}

static void
display_handle_range(void *data,
		     struct wl_display *display, uint32_t range)
{
	display->next_range = range;
}

struct wl_display_listener {
	void (*invalid_object)(void *data,
			       struct wl_display *display, uint32_t id);
	void (*invalid_method)(void *data, struct wl_display *display,
			       uint32_t id, uint32_t opcode);
	void (*no_memory)(void *data,
			  struct wl_display *display);
	void (*global)(void *data, struct wl_display *display,
		       uint32_t id, const char *interface, uint32_t version);
	void (*range)(void *data,
		      struct wl_display *display, uint32_t range);
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
	wl_list_init(&display->global_listener_list);

	display->proxy.base.interface = &wl_display_interface;
	display->proxy.base.id = 1;
	display->proxy.display = display;
	wl_list_init(&display->proxy.listener_list);

	display->listener.implementation = (void(**)(void)) &display_listener;
	wl_list_insert(display->proxy.listener_list.prev, &display->listener.link);

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
	struct wl_listener *listener;
	struct wl_proxy *proxy;

	wl_connection_copy(display->connection, p, size);
	if (id == 1)
		proxy = &display->proxy;
	else
		proxy = (struct wl_proxy *) wl_hash_lookup(display->objects, id);

	if (proxy != NULL) {
		if (wl_list_empty(&proxy->listener_list)) {
			printf("proxy found for object %d, opcode %d, but no listeners\n",
			       id, opcode);
		}

		listener = container_of(proxy->listener_list.next,
					struct wl_listener, link);
		while (&listener->link != &proxy->listener_list) {

			wl_connection_demarshal(display->connection,
						size,
						display->objects,
						listener->implementation[opcode],
						listener->data,
						&proxy->base, 
						&proxy->base.interface->events[opcode]);

			listener = container_of(listener->link.next,
						struct wl_listener, link);
		}
	}

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
	return display->compositor;
}

WL_EXPORT int
wl_compositor_add_listener(struct wl_compositor *compositor,
			   const struct wl_compositor_listener *listener,
			   void *data)
{
	return wl_proxy_add_listener(&compositor->proxy,
				     (void (**)(void)) listener, data);
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
