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

#include "wayland-client-protocol.h"
#include "connection.h"
#include "wayland-util.h"
#include "wayland-client.h"

static const char socket_name[] = "\0wayland";

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
	void *user_data;
};

struct wl_sync_handler {
	wl_display_sync_func_t func;
	uint32_t key;
	void *data;
	struct wl_list link;
};

struct wl_frame_handler {
	wl_display_frame_func_t func;
	uint32_t key;
	void *data;
	struct wl_list link;
};

struct wl_display {
	struct wl_proxy proxy;
	struct wl_connection *connection;
	int fd;
	uint32_t id, id_count, next_range;
	uint32_t mask;
	struct wl_hash_table *objects;
	struct wl_listener listener;
	struct wl_list global_listener_list;

	struct wl_visual *argb_visual;
	struct wl_visual *premultiplied_argb_visual;
	struct wl_visual *rgb_visual;

	wl_display_update_func_t update;
	void *update_data;

	wl_display_global_func_t global_handler;
	void *global_handler_data;

	struct wl_list sync_list, frame_list;
	uint32_t key;
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

WL_EXPORT struct wl_global_listener *
wl_display_add_global_listener(struct wl_display *display,
			       wl_display_global_func_t handler, void *data)
{
	struct wl_global_listener *listener;

	listener = malloc(sizeof *listener);
	if (listener == NULL)
		return NULL;

	listener->handler = handler;
	listener->data = data;
	wl_list_insert(display->global_listener_list.prev, &listener->link);

	return listener;
}

WL_EXPORT void
wl_display_remove_global_listener(struct wl_display *display,
				  struct wl_global_listener *listener)
{
	wl_list_remove(&listener->link);
	free(listener);
}

WL_EXPORT struct wl_proxy *
wl_proxy_create_for_id(struct wl_display *display,
		       const struct wl_interface *interface, uint32_t id)
{
	struct wl_proxy *proxy;

	proxy = malloc(sizeof *proxy);
	if (proxy == NULL)
		return NULL;

	proxy->base.interface = interface;
	proxy->base.id = id;
	proxy->display = display;
	wl_list_init(&proxy->listener_list);
	wl_hash_table_insert(display->objects, proxy->base.id, proxy);

	return proxy;
}

WL_EXPORT struct wl_proxy *
wl_proxy_create(struct wl_proxy *factory,
		const struct wl_interface *interface)
{
	return wl_proxy_create_for_id(factory->display, interface,
				      wl_display_allocate_id(factory->display));
}

WL_EXPORT void
wl_proxy_destroy(struct wl_proxy *proxy)
{
	struct wl_listener *listener, *next;

	wl_list_for_each_safe(listener, next, &proxy->listener_list, link)
		free(listener);

	wl_hash_table_remove(proxy->display->objects, proxy->base.id);
	free(proxy);
}

WL_EXPORT int
wl_proxy_add_listener(struct wl_proxy *proxy,
		      void (**implementation)(void), void *data)
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

WL_EXPORT void
wl_proxy_marshal(struct wl_proxy *proxy, uint32_t opcode, ...)
{
	struct wl_closure *closure;
	va_list ap;

	va_start(ap, opcode);
	closure = wl_connection_vmarshal(proxy->display->connection,
					 &proxy->base, opcode, ap,
					 &proxy->base.interface->methods[opcode]);
	va_end(ap);

	wl_closure_send(closure, proxy->display->connection);
	wl_closure_destroy(closure);
}

static void
add_visual(struct wl_display *display, uint32_t id)
{
	struct wl_visual *visual;

	visual = (struct wl_visual *)
		wl_proxy_create_for_id(display, &wl_visual_interface, id);
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

static void
display_handle_invalid_object(void *data,
			      struct wl_display *display, uint32_t id)
{
	fprintf(stderr, "sent request to invalid object\n");
	abort();
}
			      
static void
display_handle_invalid_method(void *data, 
			      struct wl_display *display,
			      uint32_t id, uint32_t opcode)
{
	fprintf(stderr, "sent invalid request opcode\n");
	abort();
}

static void
display_handle_no_memory(void *data,
			 struct wl_display *display)
{
	fprintf(stderr, "server out of memory\n");
	abort();
}

static void
display_handle_global(void *data,
		      struct wl_display *display,
		      uint32_t id, const char *interface, uint32_t version)
{
	struct wl_global_listener *listener;

	if (strcmp(interface, "display") == 0)
		wl_hash_table_insert(display->objects,
				     id, &display->proxy.base);
	else if (strcmp(interface, "visual") == 0)
		add_visual(display, id);

	wl_list_for_each(listener, &display->global_listener_list, link)
		(*listener->handler)(display,
				     id, interface, version, listener->data);
}

static void
display_handle_range(void *data,
		     struct wl_display *display, uint32_t range)
{
	display->next_range = range;
}

static void
display_handle_key(void *data,
		   struct wl_display *display, uint32_t key, uint32_t time)
{
	struct wl_sync_handler *sync_handler;
	struct wl_frame_handler *frame_handler;

	sync_handler = container_of(display->sync_list.next,
				    struct wl_sync_handler, link);
	if (!wl_list_empty(&display->sync_list) && sync_handler->key == key) {
		wl_list_remove(&sync_handler->link);
		sync_handler->func(sync_handler->data);
		free(sync_handler);
		return;
	}

	frame_handler = container_of(display->frame_list. next,
				     struct wl_frame_handler, link);
	if (!wl_list_empty(&display->frame_list) &&
	    frame_handler->key == key) {
		wl_list_remove(&frame_handler->link);
		frame_handler->func(frame_handler->data, time);
		free(frame_handler);
		return;
	}

	fprintf(stderr, "unsolicited sync event, client gone?\n");
}

static const struct wl_display_listener display_listener = {
	display_handle_invalid_object,
	display_handle_invalid_method,
	display_handle_no_memory,
	display_handle_global,
	display_handle_range,
	display_handle_key
};

WL_EXPORT struct wl_display *
wl_display_connect(const char *name, size_t name_size)
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

	display->objects = wl_hash_table_create();
	wl_list_init(&display->global_listener_list);

	display->proxy.base.interface = &wl_display_interface;
	display->proxy.base.id = 1;
	display->proxy.display = display;
	wl_list_init(&display->proxy.listener_list);

	wl_list_init(&display->sync_list);
	wl_list_init(&display->frame_list);

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

WL_EXPORT int
wl_display_get_fd(struct wl_display *display,
		  wl_display_update_func_t update, void *data)
{
	display->update = update;
	display->update_data = data;

	display->update(display->mask, display->update_data);

	return display->fd;
}

WL_EXPORT int
wl_display_sync_callback(struct wl_display *display,
			 wl_display_sync_func_t func, void *data)
{
	struct wl_sync_handler *handler;

	handler = malloc(sizeof *handler);
	if (handler == NULL)
		return -1;

	handler->func = func;
	handler->key = display->key++;
	handler->data = data;

	wl_list_insert(display->sync_list.prev, &handler->link);
	wl_display_sync(display, handler->key);

	return 0;
}

WL_EXPORT int
wl_display_frame_callback(struct wl_display *display,
			  wl_display_frame_func_t func, void *data)
{
	struct wl_frame_handler *handler;

	handler = malloc(sizeof *handler);
	if (handler == NULL)
		return -1;

	handler->func = func;
	handler->key = display->key++;
	handler->data = data;

	wl_list_insert(display->frame_list.prev, &handler->link);
	wl_display_frame(display, handler->key);

	return 0;
}

static void
handle_event(struct wl_display *display,
	     uint32_t id, uint32_t opcode, uint32_t size)
{
	uint32_t p[32];
	struct wl_listener *listener;
	struct wl_proxy *proxy;
	struct wl_closure *closure;
	const struct wl_message *message;

	wl_connection_copy(display->connection, p, size);
	if (id == 1)
		proxy = &display->proxy;
	else
		proxy = wl_hash_table_lookup(display->objects, id);

	if (proxy == NULL) {
		wl_connection_consume(display->connection, size);
		return;
	}

	message = &proxy->base.interface->events[opcode];
	closure = wl_connection_demarshal(display->connection,
					  size, display->objects, message);

	wl_list_for_each(listener, &proxy->listener_list, link)
		wl_closure_invoke(closure, &proxy->base,
				  listener->implementation[opcode],
				  listener->data);

	wl_closure_destroy(closure);
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
wl_proxy_set_user_data(struct wl_proxy *proxy, void *user_data)
{
	proxy->user_data = user_data;
}

WL_EXPORT void *
wl_proxy_get_user_data(struct wl_proxy *proxy)
{
	return proxy->user_data;
}
