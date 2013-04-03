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


/** \cond */

enum wl_proxy_flag {
	WL_PROXY_FLAG_ID_DELETED = (1 << 0),
	WL_PROXY_FLAG_DESTROYED = (1 << 1)
};

struct wl_proxy {
	struct wl_object object;
	struct wl_display *display;
	struct wl_event_queue *queue;
	uint32_t flags;
	int refcount;
	void *user_data;
};

struct wl_global {
	uint32_t id;
	char *interface;
	uint32_t version;
	struct wl_list link;
};

struct wl_event_queue {
	struct wl_list link;
	struct wl_list event_list;
	struct wl_display *display;
	pthread_cond_t cond;
};

struct wl_display {
	struct wl_proxy proxy;
	struct wl_connection *connection;
	int last_error;
	int fd;
	pthread_t display_thread;
	struct wl_map objects;
	struct wl_event_queue queue;
	struct wl_list event_queue_list;
	pthread_mutex_t mutex;
};

/** \endcond */

static int wl_debug = 0;

static void
display_fatal_error(struct wl_display *display, int error)
{
	struct wl_event_queue *iter;

	if (display->last_error)
		return;

	if (!error)
		error = 1;

	display->last_error = error;
	close(display->fd);
	display->fd = -1;

	wl_list_for_each(iter, &display->event_queue_list, link)
		pthread_cond_broadcast(&iter->cond);
}

static void
wl_display_fatal_error(struct wl_display *display, int error)
{
	pthread_mutex_lock(&display->mutex);
	display_fatal_error(display, error);
	pthread_mutex_unlock(&display->mutex);
}

static void
wl_event_queue_init(struct wl_event_queue *queue, struct wl_display *display)
{
	wl_list_init(&queue->event_list);
	pthread_cond_init(&queue->cond, NULL);
	queue->display = display;
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

/** Destroy an event queue
 *
 * \param queue The event queue to be destroyed
 *
 * Destroy the given event queue. Any pending event on that queue is
 * discarded.
 *
 * The \ref wl_display object used to create the queue should not be
 * destroyed until all event queues created with it are destroyed with
 * this function.
 *
 * \memberof wl_event_queue
 */
WL_EXPORT void
wl_event_queue_destroy(struct wl_event_queue *queue)
{
	struct wl_display *display = queue->display;

	pthread_mutex_lock(&display->mutex);
	wl_list_remove(&queue->link);
	wl_event_queue_release(queue);
	free(queue);
	pthread_mutex_unlock(&display->mutex);
}

/** Create a new event queue for this display
 *
 * \param display The display context object
 * \return A new event queue associated with this display or NULL on
 * failure.
 *
 * \memberof wl_display
 */
WL_EXPORT struct wl_event_queue *
wl_display_create_queue(struct wl_display *display)
{
	struct wl_event_queue *queue;

	queue = malloc(sizeof *queue);
	if (queue == NULL)
		return NULL;

	wl_event_queue_init(queue, display);

	pthread_mutex_lock(&display->mutex);
	wl_list_insert(&display->event_queue_list, &queue->link);
	pthread_mutex_unlock(&display->mutex);

	return queue;
}

/** Create a proxy object with a given interface
 *
 * \param factory Factory proxy object
 * \param interface Interface the proxy object should use
 * \return A newly allocated proxy object or NULL on failure
 *
 * This function creates a new proxy object with the supplied interface. The
 * proxy object will have an id assigned from the client id space. The id
 * should be created on the compositor side by sending an appropriate request
 * with \ref wl_proxy_marshal().
 *
 * The proxy will inherit the display and event queue of the factory object.
 *
 * \note This should not normally be used by non-generated code.
 *
 * \sa wl_display, wl_event_queue, wl_proxy_marshal()
 *
 * \memberof wl_proxy
 */
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
	proxy->flags = 0;
	proxy->refcount = 1;

	pthread_mutex_lock(&display->mutex);
	proxy->object.id = wl_map_insert_new(&display->objects,
					     WL_MAP_CLIENT_SIDE, proxy);
	pthread_mutex_unlock(&display->mutex);

	return proxy;
}

/* The caller should hold the display lock */
static struct wl_proxy *
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
	proxy->flags = 0;
	proxy->refcount = 1;

	wl_map_insert_at(&display->objects, id, proxy);

	return proxy;
}

/** Destroy a proxy object
 *
 * \param proxy The proxy to be destroyed
 *
 * \memberof wl_proxy
 */
WL_EXPORT void
wl_proxy_destroy(struct wl_proxy *proxy)
{
	struct wl_display *display = proxy->display;

	pthread_mutex_lock(&display->mutex);

	if (proxy->flags & WL_PROXY_FLAG_ID_DELETED)
		wl_map_remove(&proxy->display->objects, proxy->object.id);
	else if (proxy->object.id < WL_SERVER_ID_START)
		wl_map_insert_at(&proxy->display->objects,
				 proxy->object.id, WL_ZOMBIE_OBJECT);
	else
		wl_map_insert_at(&proxy->display->objects,
				 proxy->object.id, NULL);


	proxy->flags |= WL_PROXY_FLAG_DESTROYED;

	proxy->refcount--;
	if (!proxy->refcount)
		free(proxy);

	pthread_mutex_unlock(&display->mutex);
}

/** Set a proxy's listener
 *
 * \param proxy The proxy object
 * \param implementation The listener to be added to proxy
 * \param data User data to be associated with the proxy
 * \return 0 on success or -1 on failure
 *
 * Set proxy's listener to \c implementation and its user data to
 * \c data. If a listener has already been set, this function
 * fails and nothing is changed.
 *
 * \c implementation is a vector of function pointers. For an opcode
 * \c n, \c implementation[n] should point to the handler of \c n for
 * the given object.
 *
 * \memberof wl_proxy
 */
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

/** Prepare a request to be sent to the compositor
 *
 * \param proxy The proxy object
 * \param opcode Opcode of the request to be sent
 * \param ... Extra arguments for the given request
 *
 * Translates the request given by opcode and the extra arguments into the
 * wire format and write it to the connection buffer.
 *
 * The example below creates a proxy object with the wl_surface_interface
 * using a wl_compositor factory interface and sends the
 * \c compositor.create_surface request using \ref wl_proxy_marshal(). Note
 * the \c id is the extra argument to the request as specified by the
 * protocol.
 *
 * \code
 * id = wl_proxy_create((struct wl_proxy *) wl_compositor,
 *                      &wl_surface_interface);
 * wl_proxy_marshal((struct wl_proxy *) wl_compositor,
 *                  WL_COMPOSITOR_CREATE_SURFACE, id);
 * \endcode
 *
 * \note This should not normally be used by non-generated code.
 *
 * \sa wl_proxy_create()
 *
 * \memberof wl_proxy
 */
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
	int err;

	wl_log("%s@%u: error %d: %s\n",
	       object->interface->name, object->id, code, message);

	switch (code) {
	case WL_DISPLAY_ERROR_INVALID_OBJECT:
	case WL_DISPLAY_ERROR_INVALID_METHOD:
		err = -EINVAL;
		break;
	case WL_DISPLAY_ERROR_NO_MEMORY:
		err = -ENOMEM;
		break;
	default:
		err = -EFAULT;
		break;
	}

	wl_display_fatal_error(display, err);
}

static void
display_handle_delete_id(void *data, struct wl_display *display, uint32_t id)
{
	struct wl_proxy *proxy;

	pthread_mutex_lock(&display->mutex);

	proxy = wl_map_lookup(&display->objects, id);
	if (proxy != WL_ZOMBIE_OBJECT)
		proxy->flags |= WL_PROXY_FLAG_ID_DELETED;
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

/** Connect to Wayland display on an already open fd
 *
 * \param fd The fd to use for the connection
 * \return A \ref wl_display object or \c NULL on failure
 *
 * The wl_display takes ownership of the fd and will close it when the
 * display is destroyed.  The fd will also be closed in case of
 * failure.
 *
 * \memberof wl_display
 */
WL_EXPORT struct wl_display *
wl_display_connect_to_fd(int fd)
{
	struct wl_display *display;
	const char *debug;

	debug = getenv("WAYLAND_DEBUG");
	if (debug && (strstr(debug, "client") || strstr(debug, "1")))
		wl_debug = 1;

	display = malloc(sizeof *display);
	if (display == NULL) {
		close(fd);
		return NULL;
	}

	memset(display, 0, sizeof *display);

	display->fd = fd;
	wl_map_init(&display->objects);
	wl_event_queue_init(&display->queue, display);
	wl_list_init(&display->event_queue_list);
	pthread_mutex_init(&display->mutex, NULL);

	wl_map_insert_new(&display->objects, WL_MAP_CLIENT_SIDE, NULL);

	display->proxy.object.interface = &wl_display_interface;
	display->proxy.object.id =
		wl_map_insert_new(&display->objects,
				  WL_MAP_CLIENT_SIDE, display);
	display->proxy.display = display;
	display->proxy.object.implementation = (void(**)(void)) &display_listener;
	display->proxy.user_data = display;
	display->proxy.queue = &display->queue;
	display->proxy.flags = 0;
	display->proxy.refcount = 1;

	display->connection = wl_connection_create(display->fd);
	if (display->connection == NULL) {
		wl_map_release(&display->objects);
		close(display->fd);
		free(display);
		return NULL;
	}

	return display;
}

/** Connect to a Wayland display
 *
 * \param name Name of the Wayland display to connect to
 * \return A \ref wl_display object or \c NULL on failure
 *
 * Connect to the Wayland display named \c name. If \c name is \c NULL,
 * its value will bee replaced with the WAYLAND_DISPLAY environment
 * variable if it is set, otherwise display "wayland-0" will be used.
 *
 * \memberof wl_display
 */
WL_EXPORT struct wl_display *
wl_display_connect(const char *name)
{
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

	return wl_display_connect_to_fd(fd);
}

/** Close a connection to a Wayland display
 *
 * \param display The display context object
 *
 * Close the connection to \c display and free all resources associated
 * with it.
 *
 * \memberof wl_display
 */
WL_EXPORT void
wl_display_disconnect(struct wl_display *display)
{
	wl_connection_destroy(display->connection);
	wl_map_release(&display->objects);
	wl_event_queue_release(&display->queue);
	pthread_mutex_destroy(&display->mutex);
	if (display->fd > 0)
		close(display->fd);

	free(display);
}

/** Get a display context's file descriptor
 *
 * \param display The display context object
 * \return Display object file descriptor
 *
 * Return the file descriptor associated with a display so it can be
 * integrated into the client's main loop.
 *
 * \memberof wl_display
 */
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

/** Block until all pending request are processed by the server
 *
 * \param display The display context object
 * \return The number of dispatched events on success or -1 on failure
 *
 * Blocks until the server process all currently issued requests and
 * sends out pending events on all event queues.
 *
 * \memberof wl_display
 */
WL_EXPORT int
wl_display_roundtrip(struct wl_display *display)
{
	struct wl_callback *callback;
	int done, ret = 0;

	done = 0;
	callback = wl_display_sync(display);
	wl_callback_add_listener(callback, &sync_listener, &done);
	while (!done && ret >= 0)
		ret = wl_display_dispatch(display);

	if (ret == -1 && !done)
		wl_callback_destroy(callback);

	return ret;
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
	count = arg_count_for_signature(signature);
	for (i = 0; i < count; i++) {
		signature = get_next_argument(signature, &arg);
		switch (arg.type) {
		case 'n':
			id = closure->args[i].n;
			if (id == 0) {
				closure->args[i].o = NULL;
				break;
			}
			proxy = wl_proxy_create_for_id(sender, id,
						       closure->message->types[i]);
			if (proxy == NULL)
				return -1;
			closure->args[i].o = (struct wl_object *)proxy;
			break;
		default:
			break;
		}
	}

	return 0;
}

static void
increase_closure_args_refcount(struct wl_closure *closure)
{
	const char *signature;
	struct argument_details arg;
	int i, count;
	struct wl_proxy *proxy;

	signature = closure->message->signature;
	count = arg_count_for_signature(signature);
	for (i = 0; i < count; i++) {
		signature = get_next_argument(signature, &arg);
		switch (arg.type) {
		case 'n':
		case 'o':
			proxy = (struct wl_proxy *) closure->args[i].o;
			if (proxy)
				proxy->refcount++;
			break;
		default:
			break;
		}
	}
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
	} else if (proxy == NULL) {
		wl_connection_consume(display->connection, size);
		return size;
	}

	message = &proxy->object.interface->events[opcode];
	closure = wl_connection_demarshal(display->connection, size,
					  &display->objects, message);
	if (!closure)
		return -1;

	if (create_proxies(proxy, closure) < 0) {
		wl_closure_destroy(closure);
		return -1;
	}

	if (wl_closure_lookup_objects(closure, &display->objects) != 0) {
		wl_closure_destroy(closure);
		return -1;
	}

	increase_closure_args_refcount(closure);
	proxy->refcount++;
	closure->proxy = proxy;

	if (wl_list_empty(&proxy->queue->event_list))
		pthread_cond_signal(&proxy->queue->cond);
	wl_list_insert(proxy->queue->event_list.prev, &closure->link);

	return size;
}

static void
decrease_closure_args_refcount(struct wl_closure *closure)
{
	const char *signature;
	struct argument_details arg;
	int i, count;
	struct wl_proxy *proxy;

	signature = closure->message->signature;
	count = arg_count_for_signature(signature);
	for (i = 0; i < count; i++) {
		signature = get_next_argument(signature, &arg);
		switch (arg.type) {
		case 'n':
		case 'o':
			proxy = (struct wl_proxy *) closure->args[i].o;
			if (proxy) {
				if (proxy->flags & WL_PROXY_FLAG_DESTROYED)
					closure->args[i].o = NULL;

				proxy->refcount--;
				if (!proxy->refcount)
					free(proxy);
			}
			break;
		default:
			break;
		}
	}
}

static void
dispatch_event(struct wl_display *display, struct wl_event_queue *queue)
{
	struct wl_closure *closure;
	struct wl_proxy *proxy;
	int opcode;
	bool proxy_destroyed;

	closure = container_of(queue->event_list.next,
			       struct wl_closure, link);
	wl_list_remove(&closure->link);
	opcode = closure->opcode;

	/* Verify that the receiving object is still valid by checking if has
	 * been destroyed by the application. */

	decrease_closure_args_refcount(closure);
	proxy = closure->proxy;
	proxy_destroyed = !!(proxy->flags & WL_PROXY_FLAG_DESTROYED);

	proxy->refcount--;
	if (proxy_destroyed) {
		if (!proxy->refcount)
			free(proxy);

		wl_closure_destroy(closure);
		return;
	}

	pthread_mutex_unlock(&display->mutex);

	if (proxy->object.implementation) {
		if (wl_debug)
			wl_closure_print(closure, &proxy->object, false);

		wl_closure_invoke(closure, WL_CLOSURE_INVOKE_CLIENT,
				  &proxy->object, opcode,
				  proxy->user_data);
	}

	wl_closure_destroy(closure);

	pthread_mutex_lock(&display->mutex);
}

static int
dispatch_queue(struct wl_display *display,
	       struct wl_event_queue *queue, int block)
{
	int len, size, count, ret;

	pthread_mutex_lock(&display->mutex);

	if (display->last_error)
		goto err_unlock;

	ret = wl_connection_flush(display->connection);
	if (ret < 0 && errno != EAGAIN) {
		display_fatal_error(display, errno);
		goto err_unlock;
	}

	if (block && wl_list_empty(&queue->event_list) &&
	    pthread_equal(display->display_thread, pthread_self())) {
		len = wl_connection_read(display->connection);
		if (len == -1) {
			display_fatal_error(display, errno);
			goto err_unlock;
		} else if (len == 0) {
			display_fatal_error(display, EPIPE);
			goto err_unlock;
		}
		while (len >= 8) {
			size = queue_event(display, len);
			if (size == -1) {
				display_fatal_error(display, errno);
				goto err_unlock;
			} else if (size == 0) {
				break;
			}
			len -= size;
		}
	} else if (block && wl_list_empty(&queue->event_list)) {
		pthread_cond_wait(&queue->cond, &display->mutex);
		if (display->last_error)
			goto err_unlock;
	}

	for (count = 0; !wl_list_empty(&queue->event_list); count++) {
		dispatch_event(display, queue);
		if (display->last_error)
			goto err_unlock;
	}

	pthread_mutex_unlock(&display->mutex);

	return count;

err_unlock:
	errno = display->last_error;
	pthread_mutex_unlock(&display->mutex);

	return -1;
}

/** Dispatch events in an event queue
 *
 * \param display The display context object
 * \param queue The event queue to dispatch
 * \return The number of dispatched events on success or -1 on failure
 *
 * Dispatch all incoming events for objects assigned to the given
 * event queue. On failure -1 is returned and errno set appropriately.
 *
 * This function blocks if there are no events to dispatch. If calling from
 * the main thread, it will block reading data from the display fd. For other
 * threads this will block until the main thread queues events on the queue
 * passed as argument.
 *
 * \memberof wl_display
 */
WL_EXPORT int
wl_display_dispatch_queue(struct wl_display *display,
			  struct wl_event_queue *queue)
{
	return dispatch_queue(display, queue, 1);
}

/** Dispatch pending events in an event queue
 *
 * \param display The display context object
 * \param queue The event queue to dispatch
 * \return The number of dispatched events on success or -1 on failure
 *
 * Dispatch all incoming events for objects assigned to the given
 * event queue. On failure -1 is returned and errno set appropriately.
 * If there are no events queued, this functions return immediately.
 *
 * \memberof wl_display
 * \since 1.0.2
 */
WL_EXPORT int
wl_display_dispatch_queue_pending(struct wl_display *display,
				  struct wl_event_queue *queue)
{
	return dispatch_queue(display, queue, 0);
}

/** Process incoming events
 *
 * \param display The display context object
 * \return The number of dispatched events on success or -1 on failure
 *
 * Dispatch the display's main event queue.
 *
 * If the main event queue is empty, this function blocks until there are
 * events to be read from the display fd. Events are read and queued on
 * the appropriate event queues. Finally, events on the main event queue
 * are dispatched.
 *
 * \note It is not possible to check if there are events on the main queue
 * or not. For dispatching main queue events without blocking, see \ref
 * wl_display_dispatch_pending().
 *
 * \note Calling this makes the current thread the main one.
 *
 * \sa wl_display_dispatch_pending(), wl_display_dispatch_queue()
 *
 * \memberof wl_display
 */
WL_EXPORT int
wl_display_dispatch(struct wl_display *display)
{
	display->display_thread = pthread_self();

	return dispatch_queue(display, &display->queue, 1);
}

/** Dispatch main queue events without reading from the display fd
 *
 * \param display The display context object
 * \return The number of dispatched events or -1 on failure
 *
 * This function dispatches events on the main event queue. It does not
 * attempt to read the display fd and simply returns zero if the main
 * queue is empty, i.e., it doesn't block.
 *
 * This is necessary when a client's main loop wakes up on some fd other
 * than the display fd (network socket, timer fd, etc) and calls \ref
 * wl_display_dispatch_queue() from that callback. This may queue up
 * events in the main queue while reading all data from the display fd.
 * When the main thread returns to the main loop to block, the display fd
 * no longer has data, causing a call to \em poll(2) (or similar
 * functions) to block indefinitely, even though there are events ready
 * to dispatch.
 *
 * To proper integrate the wayland display fd into a main loop, the
 * client should always call \ref wl_display_dispatch_pending() and then
 * \ref wl_display_flush() prior to going back to sleep. At that point,
 * the fd typically doesn't have data so attempting I/O could block, but
 * events queued up on the main queue should be dispatched.
 *
 * A real-world example is a main loop that wakes up on a timerfd (or a
 * sound card fd becoming writable, for example in a video player), which
 * then triggers GL rendering and eventually eglSwapBuffers().
 * eglSwapBuffers() may call wl_display_dispatch_queue() if it didn't
 * receive the frame event for the previous frame, and as such queue
 * events in the main queue.
 *
 * \note Calling this makes the current thread the main one.
 *
 * \sa wl_display_dispatch(), wl_display_dispatch_queue(),
 * wl_display_flush()
 *
 * \memberof wl_display
 */
WL_EXPORT int
wl_display_dispatch_pending(struct wl_display *display)
{
	display->display_thread = pthread_self();

	return dispatch_queue(display, &display->queue, 0);
}

/** Retrieve the last error occurred on a display
 *
 * \param display The display context object
 * \return The last error occurred on \c display or 0 if no error occurred
 *
 * Return the last error occurred on the display. This may be an error sent
 * by the server or caused by the local client.
 *
 * \note Errors are \b fatal. If this function returns non-zero the display
 * can no longer be used.
 *
 * \memberof wl_display
 */
WL_EXPORT int
wl_display_get_error(struct wl_display *display)
{
	int ret;

	pthread_mutex_lock(&display->mutex);

	ret = display->last_error;

	pthread_mutex_unlock(&display->mutex);

	return ret;
}

/** Send all buffered requests on the display to the server
 *
 * \param display The display context object
 * \return The number of bytes send on success or -1 on failure
 *
 * Send all buffered data on the client side to the server. Clients
 * should call this function before blocking. On success, the number
 * of bytes sent to the server is returned. On failure, this
 * function returns -1 and errno is set appropriately.
 *
 * wl_display_flush() never blocks.  It will write as much data as
 * possible, but if all data could not be written, errno will be set
 * to EAGAIN and -1 returned.  In that case, use poll on the display
 * file descriptor to wait for it to become writable again.
 *
 * \memberof wl_display
 */
WL_EXPORT int
wl_display_flush(struct wl_display *display)
{
	int ret;

	pthread_mutex_lock(&display->mutex);

	if (display->last_error) {
		errno = display->last_error;
		ret = -1;
	} else {
		ret = wl_connection_flush(display->connection);
		if (ret < 0 && errno != EAGAIN)
			display_fatal_error(display, errno);
	}

	pthread_mutex_unlock(&display->mutex);

	return ret;
}

/** Set the user data associated with a proxy
 *
 * \param proxy The proxy object
 * \param user_data The data to be associated with proxy
 *
 * Set the user data associated with \c proxy. When events for this
 * proxy are received, \c user_data will be supplied to its listener.
 *
 * \memberof wl_proxy
 */
WL_EXPORT void
wl_proxy_set_user_data(struct wl_proxy *proxy, void *user_data)
{
	proxy->user_data = user_data;
}

/** Get the user data associated with a proxy
 *
 * \param proxy The proxy object
 * \return The user data associated with proxy
 *
 * \memberof wl_proxy
 */
WL_EXPORT void *
wl_proxy_get_user_data(struct wl_proxy *proxy)
{
	return proxy->user_data;
}

/** Get the id of a proxy object
 *
 * \param proxy The proxy object
 * \return The id the object associated with the proxy
 *
 * \memberof wl_proxy
 */
WL_EXPORT uint32_t
wl_proxy_get_id(struct wl_proxy *proxy)
{
	return proxy->object.id;
}

/** Get the interface name (class) of a proxy object
 *
 * \param proxy The proxy object
 * \return The interface name of the object associated with the proxy
 *
 * \memberof wl_proxy
 */
WL_EXPORT const char *
wl_proxy_get_class(struct wl_proxy *proxy)
{
	return proxy->object.interface->name;
}

/** Assign a proxy to an event queue
 *
 * \param proxy The proxy object
 * \param queue The event queue that will handle this proxy
 *
 * Assign proxy to event queue. Events coming from \c proxy will be
 * queued in \c queue instead of the display's main queue.
 *
 * \sa wl_display_dispatch_queue()
 *
 * \memberof wl_proxy
 */
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
