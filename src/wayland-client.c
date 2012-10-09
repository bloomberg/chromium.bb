/*
 * Copyright © 2008-2012 Kristian Høgsberg
 * Copyright © 2010-2012 Intel Corporation
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
#include <pthread.h>

#include "wayland-util.h"
#include "wayland-os.h"
#include "wayland-client.h"
#include "wayland-private.h"

struct wl_proxy {
	struct wl_object object;
	struct wl_display *display;
	struct wl_event_queue *queue;
	int id_deleted;
	void *user_data;
};

struct wl_global {
	uint32_t id;
	char *interface;
	uint32_t version;
	struct wl_list link;
};

struct wl_event_queue {
	struct wl_list event_list;
	pthread_cond_t cond;
};

struct wl_display {
	struct wl_proxy proxy;
	struct wl_connection *connection;
	int fd;
	int close_fd;
	pthread_t display_thread;
	struct wl_map objects;
	struct wl_event_queue queue;
	pthread_mutex_t mutex;
};

static int wl_debug = 0;

static void
wl_event_queue_init(struct wl_event_queue *queue)
{
	wl_list_init(&queue->event_list);
	pthread_cond_init(&queue->cond, NULL);
}

static void
wl_event_queue_release(struct wl_event_queue *queue)
{
	struct wl_closure *closure;

	while (!wl_list_empty(&queue->event_list)) {
		closure = container_of(queue->event_list.next,
				       struct wl_closure, link);
		wl_list_remove(&closure->link);
		wl_closure_destroy(closure);
	}
	pthread_cond_destroy(&queue->cond);
}

WL_EXPORT void
wl_event_queue_destroy(struct wl_event_queue *queue)
{
	wl_event_queue_release(queue);
	free(queue);
}

WL_EXPORT struct wl_event_queue *
wl_display_create_queue(struct wl_display *display)
{
	struct wl_event_queue *queue;

	queue = malloc(sizeof *queue);
	if (queue == NULL)
		return NULL;

	wl_event_queue_init(queue);

	return queue;
}

WL_EXPORT struct wl_proxy *
wl_proxy_create(struct wl_proxy *factory, const struct wl_interface *interface)
{
	struct wl_proxy *proxy;
	struct wl_display *display = factory->display;

	proxy = malloc(sizeof *proxy);
	if (proxy == NULL)
		return NULL;

	proxy->object.interface = interface;
	proxy->object.implementation = NULL;
	proxy->display = display;
	proxy->queue = factory->queue;
	proxy->id_deleted = 0;

	pthread_mutex_lock(&display->mutex);
	proxy->object.id = wl_map_insert_new(&display->objects,
					     WL_MAP_CLIENT_SIDE, proxy);
	pthread_mutex_unlock(&display->mutex);

	return proxy;
}

WL_EXPORT struct wl_proxy *
wl_proxy_create_for_id(struct wl_proxy *factory,
		       uint32_t id, const struct wl_interface *interface)
{
	struct wl_proxy *proxy;
	struct wl_display *display = factory->display;

	proxy = malloc(sizeof *proxy);
	if (proxy == NULL)
		return NULL;

	proxy->object.interface = interface;
	proxy->object.implementation = NULL;
	proxy->object.id = id;
	proxy->display = display;
	proxy->queue = factory->queue;
	proxy->id_deleted = 0;

	pthread_mutex_lock(&display->mutex);
	wl_map_insert_at(&display->objects, id, proxy);
	pthread_mutex_unlock(&display->mutex);

	return proxy;
}

WL_EXPORT void
wl_proxy_destroy(struct wl_proxy *proxy)
{
	pthread_mutex_lock(&proxy->display->mutex);

	if (proxy->id_deleted)
		wl_map_remove(&proxy->display->objects, proxy->object.id);
	else if (proxy->object.id < WL_SERVER_ID_START)
		wl_map_insert_at(&proxy->display->objects,
				 proxy->object.id, WL_ZOMBIE_OBJECT);
	else
		wl_map_insert_at(&proxy->display->objects,
				 proxy->object.id, NULL);

	pthread_mutex_unlock(&proxy->display->mutex);

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

	pthread_mutex_lock(&proxy->display->mutex);

	va_start(ap, opcode);
	closure = wl_closure_vmarshal(&proxy->object, opcode, ap,
				      &proxy->object.interface->methods[opcode]);
	va_end(ap);

	if (closure == NULL) {
		fprintf(stderr, "Error marshalling request\n");
		abort();
	}

	if (wl_debug)
		wl_closure_print(closure, &proxy->object, true);

	if (wl_closure_send(closure, proxy->display->connection)) {
		fprintf(stderr, "Error sending request: %m\n");
		abort();
	}

	wl_closure_destroy(closure);

	pthread_mutex_unlock(&proxy->display->mutex);
}

static void
display_handle_error(void *data,
		     struct wl_display *display, struct wl_object *object,
		     uint32_t code, const char *message)
{
	fprintf(stderr, "%s@%u: error %d: %s\n",
		object->interface->name, object->id, code, message);
	abort();
}

static void
display_handle_delete_id(void *data, struct wl_display *display, uint32_t id)
{
	struct wl_proxy *proxy;

	pthread_mutex_lock(&display->mutex);

	proxy = wl_map_lookup(&display->objects, id);
	if (proxy != WL_ZOMBIE_OBJECT)
		proxy->id_deleted = 1;
	else
		wl_map_remove(&display->objects, id);

	pthread_mutex_unlock(&display->mutex);
}

static const struct wl_display_listener display_listener = {
	display_handle_error,
	display_handle_delete_id
};

static int
connect_to_socket(const char *name)
{
	struct sockaddr_un addr;
	socklen_t size;
	const char *runtime_dir;
	int name_size, fd;

	runtime_dir = getenv("XDG_RUNTIME_DIR");
	if (!runtime_dir) {
		fprintf(stderr,
			"error: XDG_RUNTIME_DIR not set in the environment.\n");

		/* to prevent programs reporting
		 * "failed to create display: Success" */
		errno = ENOENT;
		return -1;
	}

	if (name == NULL)
		name = getenv("WAYLAND_DISPLAY");
	if (name == NULL)
		name = "wayland-0";

	fd = wl_os_socket_cloexec(PF_LOCAL, SOCK_STREAM, 0);
	if (fd < 0)
		return -1;

	memset(&addr, 0, sizeof addr);
	addr.sun_family = AF_LOCAL;
	name_size =
		snprintf(addr.sun_path, sizeof addr.sun_path,
			 "%s/%s", runtime_dir, name) + 1;

	assert(name_size > 0);
	if (name_size > (int)sizeof addr.sun_path) {
		fprintf(stderr,
		       "error: socket path \"%s/%s\" plus null terminator"
		       " exceeds 108 bytes\n", runtime_dir, name);
		close(fd);
		/* to prevent programs reporting
		 * "failed to add socket: Success" */
		errno = ENAMETOOLONG;
		return -1;
	};

	size = offsetof (struct sockaddr_un, sun_path) + name_size;

	if (connect(fd, (struct sockaddr *) &addr, size) < 0) {
		close(fd);
		return -1;
	}

	return fd;
}

WL_EXPORT struct wl_display *
wl_display_connect_to_fd(int fd)
{
	struct wl_display *display;
	const char *debug;

	debug = getenv("WAYLAND_DEBUG");
	if (debug)
		wl_debug = 1;

	display = malloc(sizeof *display);
	if (display == NULL)
		return NULL;

	memset(display, 0, sizeof *display);

	display->fd = fd;
	wl_map_init(&display->objects);
	wl_event_queue_init(&display->queue);

	wl_map_insert_new(&display->objects, WL_MAP_CLIENT_SIDE, NULL);

	display->proxy.object.interface = &wl_display_interface;
	display->proxy.object.id =
		wl_map_insert_new(&display->objects,
				  WL_MAP_CLIENT_SIDE, display);
	display->proxy.display = display;
	display->proxy.object.implementation = (void(**)(void)) &display_listener;
	display->proxy.user_data = display;
	display->proxy.queue = &display->queue;

	display->connection = wl_connection_create(display->fd);
	if (display->connection == NULL) {
		wl_map_release(&display->objects);
		close(display->fd);
		free(display);
		return NULL;
	}

	return display;
}

WL_EXPORT struct wl_display *
wl_display_connect(const char *name)
{
	struct wl_display *display;
	char *connection, *end;
	int flags, fd;

	connection = getenv("WAYLAND_SOCKET");
	if (connection) {
		fd = strtol(connection, &end, 0);
		if (*end != '\0')
			return NULL;

		flags = fcntl(fd, F_GETFD);
		if (flags != -1)
			fcntl(fd, F_SETFD, flags | FD_CLOEXEC);
		unsetenv("WAYLAND_SOCKET");
	} else {
		fd = connect_to_socket(name);
		if (fd < 0)
			return NULL;
	}

	display = wl_display_connect_to_fd(fd);
	if (display)
		display->close_fd = 1;

	return display;
}

WL_EXPORT void
wl_display_disconnect(struct wl_display *display)
{
	wl_connection_destroy(display->connection);
	wl_map_release(&display->objects);
	wl_event_queue_release(&display->queue);

	if (display->close_fd)
		close(display->fd);

	free(display);
}

WL_EXPORT int
wl_display_get_fd(struct wl_display *display)
{
	return display->fd;
}

static void
sync_callback(void *data, struct wl_callback *callback, uint32_t serial)
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
	while (!done)
		wl_display_dispatch(display);
}

static int
create_proxies(struct wl_proxy *sender, struct wl_closure *closure)
{
	struct wl_proxy *proxy;
	const char *signature;
	struct argument_details arg;
	uint32_t id;
	int i;
	int count;

	signature = closure->message->signature;
	count = arg_count_for_signature(signature) + 2;
	for (i = 2; i < count; i++) {
		signature = get_next_argument(signature, &arg);
		switch (arg.type) {
		case 'n':
			id = **(uint32_t **) closure->args[i];
			if (id == 0) {
				*(void **) closure->args[i] = NULL;
				break;
			}
			proxy = wl_proxy_create_for_id(sender, id,
						       closure->message->types[i - 2]);
			if (proxy == NULL)
				return -1;
			*(void **) closure->args[i] = proxy;
			break;
		default:
			break;
		}
	}

	return 0;
}

static int
queue_event(struct wl_display *display, int len)
{
	uint32_t p[2], id;
	int opcode, size;
	struct wl_proxy *proxy;
	struct wl_closure *closure;
	const struct wl_message *message;

	wl_connection_copy(display->connection, p, sizeof p);
	id = p[0];
	opcode = p[1] & 0xffff;
	size = p[1] >> 16;
	if (len < size)
		return 0;

	proxy = wl_map_lookup(&display->objects, id);
	if (proxy == WL_ZOMBIE_OBJECT) {
		wl_connection_consume(display->connection, size);
		return size;
	} else if (proxy == NULL || proxy->object.implementation == NULL) {
		wl_connection_consume(display->connection, size);
		return size;
	}

	message = &proxy->object.interface->events[opcode];
	closure = wl_connection_demarshal(display->connection, size,
					  &display->objects, message);

	if (closure == NULL || create_proxies(proxy, closure) < 0) {
		fprintf(stderr, "Error demarshalling event\n");
		abort();
	}

	if (wl_list_empty(&proxy->queue->event_list))
		pthread_cond_signal(&proxy->queue->cond);
	wl_list_insert(proxy->queue->event_list.prev, &closure->link);

	return size;
}

static void
dispatch_event(struct wl_display *display, struct wl_event_queue *queue)
{
	struct wl_closure *closure;
	struct wl_proxy *proxy;
	uint32_t id;
	int opcode, ret;

	closure = container_of(queue->event_list.next,
			       struct wl_closure, link);
	wl_list_remove(&closure->link);
	id = closure->buffer[0];
	opcode = closure->buffer[1] & 0xffff;

	/* Verify that the receiving object is still valid and look up
	 * proxies for any arguments.  We have to do this just before
	 * calling the handler, since preceeding events may have
	 * destroyed either the proxy or the proxy args since the
	 * event was queued. */
	proxy = wl_map_lookup(&display->objects, id);
	ret = wl_closure_lookup_objects(closure, &display->objects);

	pthread_mutex_unlock(&display->mutex);

	if (proxy != WL_ZOMBIE_OBJECT && ret == 0) {
		if (wl_debug)
			wl_closure_print(closure, &proxy->object, false);

		wl_closure_invoke(closure, &proxy->object,
				  proxy->object.implementation[opcode],
				  proxy->user_data);
	}

	wl_closure_destroy(closure);

	pthread_mutex_lock(&display->mutex);
}


WL_EXPORT int
wl_display_dispatch_queue(struct wl_display *display,
			  struct wl_event_queue *queue)
{
	int len, size;

	pthread_mutex_lock(&display->mutex);

	/* FIXME: Handle flush errors, EAGAIN... */
	wl_connection_flush(display->connection);

	if (wl_list_empty(&queue->event_list) &&
	    pthread_equal(display->display_thread, pthread_self())) {
		len = wl_connection_read(display->connection);
		if (len == -1) {
			pthread_mutex_unlock(&display->mutex);
			return -1;
		}
		while (len >= 8) {
			size = queue_event(display, len);
			if (size == 0)
				break;
			len -= size;
		}
	} else if (wl_list_empty(&queue->event_list)) {
		pthread_cond_wait(&queue->cond, &display->mutex);
	}

	while (!wl_list_empty(&queue->event_list))
		dispatch_event(display, queue);

	pthread_mutex_unlock(&display->mutex);

	return 0;
}

WL_EXPORT int
wl_display_dispatch(struct wl_display *display)
{
	display->display_thread = pthread_self();

	return wl_display_dispatch_queue(display, &display->queue);
}

WL_EXPORT int
wl_display_flush(struct wl_display *display)
{
	int ret;

	pthread_mutex_lock(&display->mutex);

	ret = wl_connection_flush(display->connection);

	pthread_mutex_unlock(&display->mutex);

	return ret;
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

WL_EXPORT uint32_t
wl_proxy_get_id(struct wl_proxy *proxy)
{
	return proxy->object.id;
}


WL_EXPORT void
wl_proxy_set_queue(struct wl_proxy *proxy, struct wl_event_queue *queue)
{
	proxy->queue = queue;
}

WL_EXPORT void
wl_log_set_handler_client(wl_log_func_t handler)
{
	wl_log_handler = handler;
}
