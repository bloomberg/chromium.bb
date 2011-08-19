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
#include <stdbool.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <ctype.h>
#include <assert.h>
#include <fcntl.h>
#include <sys/poll.h>

#include "connection.h"
#include "wayland-util.h"
#include "wayland-client.h"

struct wl_global_listener {
	wl_display_global_func_t handler;
	void *data;
	struct wl_list link;
};

struct wl_proxy {
	struct wl_object object;
	struct wl_display *display;
	void *user_data;
};

struct wl_global {
	uint32_t id;
	char *interface;
	uint32_t version;
	struct wl_list link;
};

struct wl_display {
	struct wl_proxy proxy;
	struct wl_connection *connection;
	int fd;
	uint32_t id, id_count, next_range;
	uint32_t mask;
	struct wl_hash_table *objects;
	struct wl_list global_listener_list;
	struct wl_list global_list;

	wl_display_update_func_t update;
	void *update_data;

	wl_display_global_func_t global_handler;
	void *global_handler_data;
};

static int wl_debug = 0;

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
	struct wl_global *global;

	listener = malloc(sizeof *listener);
	if (listener == NULL)
		return NULL;

	listener->handler = handler;
	listener->data = data;
	wl_list_insert(display->global_listener_list.prev, &listener->link);

	wl_list_for_each(global, &display->global_list, link)
		(*listener->handler)(display, global->id, global->interface,
				     global->version, listener->data);

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

	proxy->object.interface = interface;
	proxy->object.implementation = NULL;
	proxy->object.id = id;
	proxy->display = display;
	wl_hash_table_insert(display->objects, proxy->object.id, proxy);

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
	wl_hash_table_remove(proxy->display->objects, proxy->object.id);
	free(proxy);
}

WL_EXPORT int
wl_proxy_add_listener(struct wl_proxy *proxy,
		      void (**implementation)(void), void *data)
{
	if (proxy->object.implementation) {
		fprintf(stderr, "proxy already has listener\n");
		return -1;
	}

	proxy->object.implementation = implementation;
	proxy->user_data = data;

	return 0;
}

WL_EXPORT void
wl_proxy_marshal(struct wl_proxy *proxy, uint32_t opcode, ...)
{
	struct wl_closure *closure;
	va_list ap;

	va_start(ap, opcode);
	closure = wl_connection_vmarshal(proxy->display->connection,
					 &proxy->object, opcode, ap,
					 &proxy->object.interface->methods[opcode]);
	va_end(ap);

	wl_closure_send(closure, proxy->display->connection);

	if (wl_debug)
		wl_closure_print(closure, &proxy->object, true);

	wl_closure_destroy(closure);
}

/* Can't do this, there may be more than one instance of an
 * interface... */
WL_EXPORT uint32_t
wl_display_get_global(struct wl_display *display,
		      const char *interface, uint32_t version)
{
	struct wl_global *global;

	wl_list_for_each(global, &display->global_list, link)
		if (strcmp(interface, global->interface) == 0 &&
		    version <= global->version)
			return global->id;

	return 0;
}

static void
display_handle_error(void *data,
		     struct wl_display *display, struct wl_object *object,
		     uint32_t code, const char *message)
{
	fprintf(stderr, "%s@%d: error %d: %s\n",
		object->interface->name, object->id, code, message);
	abort();
}

static void
display_handle_global(void *data,
		      struct wl_display *display,
		      uint32_t id, const char *interface, uint32_t version)
{
	struct wl_global_listener *listener;
	struct wl_global *global;

	if (strcmp(interface, "wl_display") == 0)
		wl_hash_table_insert(display->objects,
				     id, &display->proxy.object);

	global = malloc(sizeof *global);
	global->id = id;
	global->interface = strdup(interface);
	global->version = version;
	wl_list_insert(display->global_list.prev, &global->link);

	wl_list_for_each(listener, &display->global_listener_list, link)
		(*listener->handler)(display,
				     id, interface, version, listener->data);
}

static void
display_handle_global_remove(void *data,
                             struct wl_display *display, uint32_t id)
{
	struct wl_global *global;

	wl_list_for_each(global, &display->global_list, link)
		if (global->id == id) {
			wl_list_remove(&global->link);
			free(global);
			break;
		}
}

static void
display_handle_range(void *data,
		     struct wl_display *display, uint32_t range)
{
	display->next_range = range;
}

static const struct wl_display_listener display_listener = {
	display_handle_error,
	display_handle_global,
	display_handle_global_remove,
	display_handle_range,
};

static int
connect_to_socket(struct wl_display *display, const char *name)
{
	struct sockaddr_un addr;
	socklen_t size;
	const char *runtime_dir;
	size_t name_size;

	display->fd = socket(PF_LOCAL, SOCK_STREAM | SOCK_CLOEXEC, 0);
	if (display->fd < 0)
		return -1;

	runtime_dir = getenv("XDG_RUNTIME_DIR");
	if (runtime_dir == NULL) {
		runtime_dir = ".";
		fprintf(stderr,
			"XDG_RUNTIME_DIR not set, falling back to %s\n",
			runtime_dir);
	}

	if (name == NULL)
		name = getenv("WAYLAND_DISPLAY");
	if (name == NULL)
		name = "wayland-0";

	memset(&addr, 0, sizeof addr);
	addr.sun_family = AF_LOCAL;
	name_size =
		snprintf(addr.sun_path, sizeof addr.sun_path,
			 "%s/%s", runtime_dir, name) + 1;

	size = offsetof (struct sockaddr_un, sun_path) + name_size;

	if (connect(display->fd, (struct sockaddr *) &addr, size) < 0) {
		close(display->fd);
		return -1;
	}

	return 0;
}

WL_EXPORT struct wl_display *
wl_display_connect(const char *name)
{
	struct wl_display *display;
	const char *debug;
	char *connection, *end;
	int flags;

	debug = getenv("WAYLAND_DEBUG");
	if (debug)
		wl_debug = 1;

	display = malloc(sizeof *display);
	if (display == NULL)
		return NULL;

	memset(display, 0, sizeof *display);
	connection = getenv("WAYLAND_SOCKET");
	if (connection) {
		display->fd = strtol(connection, &end, 0);
		if (*end != '\0') {
			free(display);
			return NULL;
		}
		flags = fcntl(display->fd, F_GETFD);
		if (flags != -1)
			fcntl(display->fd, F_SETFD, flags | FD_CLOEXEC);
	} else if (connect_to_socket(display, name) < 0) {
		free(display);
		return NULL;
	}

	display->objects = wl_hash_table_create();
	if (display->objects == NULL) {
		close(display->fd);
		free(display);
		return NULL;
	}
	wl_list_init(&display->global_listener_list);
	wl_list_init(&display->global_list);

	display->proxy.object.interface = &wl_display_interface;
	display->proxy.object.id = 1;
	display->proxy.display = display;

	display->proxy.object.implementation =
		(void(**)(void)) &display_listener;
	display->proxy.user_data = display;

	display->connection = wl_connection_create(display->fd,
						   connection_update,
						   display);
	if (display->connection == NULL) {
		wl_hash_table_destroy(display->objects);
		close(display->fd);
		free(display);
		return NULL;
	}

	wl_display_bind(display, 1, "wl_display", 1);

	return display;
}

WL_EXPORT void
wl_display_destroy(struct wl_display *display)
{
	struct wl_global *global, *gnext;
	struct wl_global_listener *listener, *lnext;

	wl_connection_destroy(display->connection);
	wl_hash_table_destroy(display->objects);
	wl_list_for_each_safe(global, gnext,
			      &display->global_list, link)
		free(global);
	wl_list_for_each_safe(listener, lnext,
			      &display->global_listener_list, link)
		free(listener);

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

static void
sync_callback(void *data, struct wl_callback *callback, uint32_t time)
{
   int *done = data;

   *done = 1;
   wl_callback_destroy(callback);
}

static const struct wl_callback_listener sync_listener = {
	sync_callback
};

WL_EXPORT void
wl_display_roundtrip(struct wl_display *display)
{
	struct wl_callback *callback;
	int done;

	done = 0;
	callback = wl_display_sync(display);
	wl_callback_add_listener(callback, &sync_listener, &done);
	wl_display_flush(display);
	while (!done)
		wl_display_iterate(display, WL_DISPLAY_READABLE);
}

static void
handle_event(struct wl_display *display,
	     uint32_t id, uint32_t opcode, uint32_t size)
{
	uint32_t p[32];
	struct wl_proxy *proxy;
	struct wl_closure *closure;
	const struct wl_message *message;

	wl_connection_copy(display->connection, p, size);
	if (id == 1)
		proxy = &display->proxy;
	else
		proxy = wl_hash_table_lookup(display->objects, id);

	if (proxy == NULL || proxy->object.implementation == NULL) {
		wl_connection_consume(display->connection, size);
		return;
	}

	message = &proxy->object.interface->events[opcode];
	closure = wl_connection_demarshal(display->connection,
					  size, display->objects, message);

	if (closure == NULL) {
		fprintf(stderr, "Error demarshalling event: %m\n");
		abort();
	}

	if (wl_debug)
		wl_closure_print(closure, &proxy->object, false);

	wl_closure_invoke(closure, &proxy->object,
			  proxy->object.implementation[opcode],
			  proxy->user_data);

	wl_closure_destroy(closure);
}

WL_EXPORT void
wl_display_iterate(struct wl_display *display, uint32_t mask)
{
	uint32_t p[2], object, opcode, size;
	int len;

	mask &= display->mask;
	if (mask == 0) {
		fprintf(stderr,
			"wl_display_iterate called with unsolicited flags");
		return;
	}

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
wl_display_flush(struct wl_display *display)
{
	while (display->mask & WL_DISPLAY_WRITABLE)
		wl_display_iterate (display, WL_DISPLAY_WRITABLE);
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
wl_display_bind(struct wl_display *display,
		uint32_t id, const char *interface, uint32_t version)
{
	wl_proxy_marshal(&display->proxy,
			 WL_DISPLAY_BIND, id, interface, version);
}

WL_EXPORT struct wl_callback *
wl_display_sync(struct wl_display *display)
{
	struct wl_proxy *proxy;

	proxy = wl_proxy_create(&display->proxy, &wl_callback_interface);

	if (!proxy)
		return NULL;

	wl_proxy_marshal(&display->proxy, WL_DISPLAY_SYNC, proxy);

	return (struct wl_callback *) proxy;
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
