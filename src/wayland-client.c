/*
 * Copyright © 2008-2012 Kristian Høgsberg
 * Copyright © 2010-2012 Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial
 * portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT.  IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#define _GNU_SOURCE

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
#include <poll.h>
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
	wl_dispatcher_func_t dispatcher;
};

struct wl_global {
	uint32_t id;
	char *interface;
	uint32_t version;
	struct wl_list link;
};

struct wl_event_queue {
	struct wl_list event_list;
	struct wl_display *display;
};

struct wl_display {
	struct wl_proxy proxy;
	struct wl_connection *connection;

	/* errno of the last wl_display error */
	int last_error;

	/* When display gets an error event from some object, it stores
	 * information about it here, so that client can get this
	 * information afterwards */
	struct {
		/* Code of the error. It can be compared to
		 * the interface's errors enumeration. */
		uint32_t code;
		/* interface (protocol) in which the error occurred */
		const struct wl_interface *interface;
		/* id of the proxy that caused the error. There's no warranty
		 * that the proxy is still valid. It's up to client how it will
		 * use it */
		uint32_t id;
	} protocol_error;
	int fd;
	struct wl_map objects;
	struct wl_event_queue display_queue;
	struct wl_event_queue default_queue;
	pthread_mutex_t mutex;

	int reader_count;
	uint32_t read_serial;
	pthread_cond_t reader_cond;
};

/** \endcond */

static int debug_client = 0;

/**
 * This helper function wakes up all threads that are
 * waiting for display->reader_cond (i. e. when reading is done,
 * canceled, or an error occurred)
 *
 * NOTE: must be called with display->mutex locked
 */
static void
display_wakeup_threads(struct wl_display *display)
{
	/* Thread can get sleeping only in read_events(). If we're
	 * waking it up, it means that the read completed or was
	 * canceled, so we must increase the read_serial.
	 * This prevents from indefinite sleeping in read_events().
	 */
	++display->read_serial;

	pthread_cond_broadcast(&display->reader_cond);
}

/**
 * This function is called for local errors (no memory, server hung up)
 *
 * \param display
 * \param error    error value (EINVAL, EFAULT, ...)
 *
 * \note this function is called with display mutex locked
 */
static void
display_fatal_error(struct wl_display *display, int error)
{
	if (display->last_error)
		return;

	if (!error)
		error = EFAULT;

	display->last_error = error;

	display_wakeup_threads(display);
}

/**
 * This function is called for error events
 * and indicates that in some object an error occurred.
 * Difference between this function and display_fatal_error()
 * is that this one handles errors that will come by wire,
 * whereas display_fatal_error() is called for local errors.
 *
 * \param display
 * \param code    error code
 * \param id      id of the object that generated the error
 * \param intf    protocol interface
 */
static void
display_protocol_error(struct wl_display *display, uint32_t code,
		       uint32_t id, const struct wl_interface *intf)
{
	int err;

	if (display->last_error)
		return;

	/* set correct errno */
	if (wl_interface_equal(intf, &wl_display_interface)) {
		switch (code) {
		case WL_DISPLAY_ERROR_INVALID_OBJECT:
		case WL_DISPLAY_ERROR_INVALID_METHOD:
			err = EINVAL;
			break;
		case WL_DISPLAY_ERROR_NO_MEMORY:
			err = ENOMEM;
			break;
		default:
			err = EFAULT;
		}
	} else {
		err = EPROTO;
	}

	pthread_mutex_lock(&display->mutex);

	display->last_error = err;

	display->protocol_error.code = code;
	display->protocol_error.id = id;
	display->protocol_error.interface = intf;

	/*
	 * here it is not necessary to wake up threads like in
	 * display_fatal_error, because this function is called from
	 * an event handler and that means that read_events() is done
	 * and woke up all threads. Since wl_display_prepare_read()
	 * fails when there are events in the queue, no threads
	 * can sleep in read_events() during dispatching
	 * (and therefore during calling this function), so this is safe.
	 */

	pthread_mutex_unlock(&display->mutex);
}

static void
wl_event_queue_init(struct wl_event_queue *queue, struct wl_display *display)
{
	wl_list_init(&queue->event_list);
	queue->display = display;
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

void
proxy_destroy(struct wl_proxy *proxy);

static void
wl_event_queue_release(struct wl_event_queue *queue)
{
	struct wl_closure *closure;
	struct wl_proxy *proxy;
	bool proxy_destroyed;

	while (!wl_list_empty(&queue->event_list)) {
		closure = container_of(queue->event_list.next,
				       struct wl_closure, link);
		wl_list_remove(&closure->link);

		decrease_closure_args_refcount(closure);

		proxy = closure->proxy;
		proxy_destroyed = !!(proxy->flags & WL_PROXY_FLAG_DESTROYED);

		proxy->refcount--;
		if (proxy_destroyed && !proxy->refcount)
			free(proxy);

		wl_closure_destroy(closure);
	}
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
 * \memberof wl_event_queue
 */
WL_EXPORT struct wl_event_queue *
wl_display_create_queue(struct wl_display *display)
{
	struct wl_event_queue *queue;

	queue = malloc(sizeof *queue);
	if (queue == NULL)
		return NULL;

	wl_event_queue_init(queue, display);

	return queue;
}

static struct wl_proxy *
proxy_create(struct wl_proxy *factory, const struct wl_interface *interface)
{
	struct wl_proxy *proxy;
	struct wl_display *display = factory->display;

	proxy = malloc(sizeof *proxy);
	if (proxy == NULL)
		return NULL;

	memset(proxy, 0, sizeof *proxy);

	proxy->object.interface = interface;
	proxy->display = display;
	proxy->queue = factory->queue;
	proxy->refcount = 1;

	proxy->object.id = wl_map_insert_new(&display->objects, 0, proxy);

	return proxy;
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
	struct wl_display *display = factory->display;
	struct wl_proxy *proxy;

	pthread_mutex_lock(&display->mutex);
	proxy = proxy_create(factory, interface);
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

	memset(proxy, 0, sizeof *proxy);

	proxy->object.interface = interface;
	proxy->object.id = id;
	proxy->display = display;
	proxy->queue = factory->queue;
	proxy->refcount = 1;

	wl_map_insert_at(&display->objects, 0, id, proxy);

	return proxy;
}

void
proxy_destroy(struct wl_proxy *proxy)
{
	if (proxy->flags & WL_PROXY_FLAG_ID_DELETED)
		wl_map_remove(&proxy->display->objects, proxy->object.id);
	else if (proxy->object.id < WL_SERVER_ID_START)
		wl_map_insert_at(&proxy->display->objects, 0,
				 proxy->object.id, WL_ZOMBIE_OBJECT);
	else
		wl_map_insert_at(&proxy->display->objects, 0,
				 proxy->object.id, NULL);


	proxy->flags |= WL_PROXY_FLAG_DESTROYED;

	proxy->refcount--;
	if (!proxy->refcount)
		free(proxy);
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
	proxy_destroy(proxy);
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
	if (proxy->object.implementation || proxy->dispatcher) {
		wl_log("proxy %p already has listener\n", proxy);
		return -1;
	}

	proxy->object.implementation = implementation;
	proxy->user_data = data;

	return 0;
}

/** Get a proxy's listener
 *
 * \param proxy The proxy object
 * \return The address of the proxy's listener or NULL if no listener is set
 *
 * Gets the address to the proxy's listener; which is the listener set with
 * \ref wl_proxy_add_listener.
 *
 * This function is useful in clients with multiple listeners on the same
 * interface to allow the identification of which code to execute.
 *
 * \memberof wl_proxy
 */
WL_EXPORT const void *
wl_proxy_get_listener(struct wl_proxy *proxy)
{
	return proxy->object.implementation;
}

/** Set a proxy's listener (with dispatcher)
 *
 * \param proxy The proxy object
 * \param dispatcher The dispatcher to be used for this proxy
 * \param implementation The dispatcher-specific listener implementation
 * \param data User data to be associated with the proxy
 * \return 0 on success or -1 on failure
 *
 * Set proxy's listener to use \c dispatcher_func as its dispatcher and \c
 * dispatcher_data as its dispatcher-specific implementation and its user data
 * to \c data. If a listener has already been set, this function
 * fails and nothing is changed.
 *
 * The exact details of dispatcher_data depend on the dispatcher used.  This
 * function is intended to be used by language bindings, not user code.
 *
 * \memberof wl_proxy
 */
WL_EXPORT int
wl_proxy_add_dispatcher(struct wl_proxy *proxy,
			wl_dispatcher_func_t dispatcher,
			const void *implementation, void *data)
{
	if (proxy->object.implementation || proxy->dispatcher) {
		wl_log("proxy %p already has listener\n");
		return -1;
	}

	proxy->object.implementation = implementation;
	proxy->dispatcher = dispatcher;
	proxy->user_data = data;

	return 0;
}

static struct wl_proxy *
create_outgoing_proxy(struct wl_proxy *proxy, const struct wl_message *message,
		      union wl_argument *args,
		      const struct wl_interface *interface)
{
	int i, count;
	const char *signature;
	struct argument_details arg;
	struct wl_proxy *new_proxy = NULL;

	signature = message->signature;
	count = arg_count_for_signature(signature);
	for (i = 0; i < count; i++) {
		signature = get_next_argument(signature, &arg);

		switch (arg.type) {
		case 'n':
			new_proxy = proxy_create(proxy, interface);
			if (new_proxy == NULL)
				return NULL;

			args[i].o = &new_proxy->object;
			break;
		}
	}

	return new_proxy;
}

/** Prepare a request to be sent to the compositor
 *
 * \param proxy The proxy object
 * \param opcode Opcode of the request to be sent
 * \param args Extra arguments for the given request
 * \param interface The interface to use for the new proxy
 *
 * Translates the request given by opcode and the extra arguments into the
 * wire format and write it to the connection buffer.  This version takes an
 * array of the union type wl_argument.
 *
 * For new-id arguments, this function will allocate a new wl_proxy
 * and send the ID to the server.  The new wl_proxy will be returned
 * on success or NULL on errror with errno set accordingly.
 *
 * \note This is intended to be used by language bindings and not in
 * non-generated code.
 *
 * \sa wl_proxy_marshal()
 *
 * \memberof wl_proxy
 */
WL_EXPORT struct wl_proxy *
wl_proxy_marshal_array_constructor(struct wl_proxy *proxy,
				   uint32_t opcode, union wl_argument *args,
				   const struct wl_interface *interface)
{
	struct wl_closure *closure;
	struct wl_proxy *new_proxy = NULL;
	const struct wl_message *message;

	pthread_mutex_lock(&proxy->display->mutex);

	message = &proxy->object.interface->methods[opcode];
	if (interface) {
		new_proxy = create_outgoing_proxy(proxy, message,
						  args, interface);
		if (new_proxy == NULL)
			goto err_unlock;
	}

	closure = wl_closure_marshal(&proxy->object, opcode, args, message);
	if (closure == NULL) {
		wl_log("Error marshalling request: %m\n");
		abort();
	}

	if (debug_client)
		wl_closure_print(closure, &proxy->object, true);

	if (wl_closure_send(closure, proxy->display->connection)) {
		wl_log("Error sending request: %m\n");
		abort();
	}

	wl_closure_destroy(closure);

 err_unlock:
	pthread_mutex_unlock(&proxy->display->mutex);

	return new_proxy;
}


/** Prepare a request to be sent to the compositor
 *
 * \param proxy The proxy object
 * \param opcode Opcode of the request to be sent
 * \param ... Extra arguments for the given request
 *
 * This function is similar to wl_proxy_marshal_constructor(), except
 * it doesn't create proxies for new-id arguments.
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
	union wl_argument args[WL_CLOSURE_MAX_ARGS];
	va_list ap;

	va_start(ap, opcode);
	wl_argument_from_va_list(proxy->object.interface->methods[opcode].signature,
				 args, WL_CLOSURE_MAX_ARGS, ap);
	va_end(ap);

	wl_proxy_marshal_array_constructor(proxy, opcode, args, NULL);
}

/** Prepare a request to be sent to the compositor
 *
 * \param proxy The proxy object
 * \param opcode Opcode of the request to be sent
 * \param interface The interface to use for the new proxy
 * \param ... Extra arguments for the given request
 * \return A new wl_proxy for the new_id argument or NULL on error
 *
 * Translates the request given by opcode and the extra arguments into the
 * wire format and write it to the connection buffer.
 *
 * For new-id arguments, this function will allocate a new wl_proxy
 * and send the ID to the server.  The new wl_proxy will be returned
 * on success or NULL on errror with errno set accordingly.
 *
 * \note This should not normally be used by non-generated code.
 *
 * \memberof wl_proxy
 */
WL_EXPORT struct wl_proxy *
wl_proxy_marshal_constructor(struct wl_proxy *proxy, uint32_t opcode,
			     const struct wl_interface *interface, ...)
{
	union wl_argument args[WL_CLOSURE_MAX_ARGS];
	va_list ap;

	va_start(ap, interface);
	wl_argument_from_va_list(proxy->object.interface->methods[opcode].signature,
				 args, WL_CLOSURE_MAX_ARGS, ap);
	va_end(ap);

	return wl_proxy_marshal_array_constructor(proxy, opcode,
						  args, interface);
}

/** Prepare a request to be sent to the compositor
 *
 * \param proxy The proxy object
 * \param opcode Opcode of the request to be sent
 * \param args Extra arguments for the given request
 *
 * This function is similar to wl_proxy_marshal_array_constructor(), except
 * it doesn't create proxies for new-id arguments.
 *
 * \note This is intended to be used by language bindings and not in
 * non-generated code.
 *
 * \sa wl_proxy_marshal()
 *
 * \memberof wl_proxy
 */
WL_EXPORT void
wl_proxy_marshal_array(struct wl_proxy *proxy, uint32_t opcode,
		       union wl_argument *args)
{
	wl_proxy_marshal_array_constructor(proxy, opcode, args, NULL);
}

static void
display_handle_error(void *data,
		     struct wl_display *display, void *object,
		     uint32_t code, const char *message)
{
	struct wl_proxy *proxy = object;

	wl_log("%s@%u: error %d: %s\n",
	       proxy->object.interface->name, proxy->object.id, code, message);

	display_protocol_error(display, code, proxy->object.id,
			       proxy->object.interface);
}

static void
display_handle_delete_id(void *data, struct wl_display *display, uint32_t id)
{
	struct wl_proxy *proxy;

	pthread_mutex_lock(&display->mutex);

	proxy = wl_map_lookup(&display->objects, id);

	if (!proxy)
		wl_log("error: received delete_id for unknown id (%u)\n", id);

	if (proxy && proxy != WL_ZOMBIE_OBJECT)
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
		wl_log("error: XDG_RUNTIME_DIR not set in the environment.\n");
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
		wl_log("error: socket path \"%s/%s\" plus null terminator"
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
		debug_client = 1;

	display = malloc(sizeof *display);
	if (display == NULL) {
		close(fd);
		return NULL;
	}

	memset(display, 0, sizeof *display);

	display->fd = fd;
	wl_map_init(&display->objects, WL_MAP_CLIENT_SIDE);
	wl_event_queue_init(&display->default_queue, display);
	wl_event_queue_init(&display->display_queue, display);
	pthread_mutex_init(&display->mutex, NULL);
	pthread_cond_init(&display->reader_cond, NULL);
	display->reader_count = 0;

	wl_map_insert_new(&display->objects, 0, NULL);

	display->proxy.object.interface = &wl_display_interface;
	display->proxy.object.id =
		wl_map_insert_new(&display->objects, 0, display);
	display->proxy.display = display;
	display->proxy.object.implementation = (void(**)(void)) &display_listener;
	display->proxy.user_data = display;
	display->proxy.queue = &display->default_queue;
	display->proxy.flags = 0;
	display->proxy.refcount = 1;

	display->connection = wl_connection_create(display->fd);
	if (display->connection == NULL)
		goto err_connection;

	return display;

 err_connection:
	pthread_mutex_destroy(&display->mutex);
	pthread_cond_destroy(&display->reader_cond);
	wl_map_release(&display->objects);
	close(display->fd);
	free(display);

	return NULL;
}

/** Connect to a Wayland display
 *
 * \param name Name of the Wayland display to connect to
 * \return A \ref wl_display object or \c NULL on failure
 *
 * Connect to the Wayland display named \c name. If \c name is \c NULL,
 * its value will be replaced with the WAYLAND_DISPLAY environment
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
		int prev_errno = errno;
		errno = 0;
		fd = strtol(connection, &end, 0);
		if (errno != 0 || connection == end || *end != '\0')
			return NULL;
		errno = prev_errno;

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
	wl_event_queue_release(&display->default_queue);
	wl_event_queue_release(&display->display_queue);
	pthread_mutex_destroy(&display->mutex);
	pthread_cond_destroy(&display->reader_cond);
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
 * \param queue The queue on which to run the roundtrip
 * \return The number of dispatched events on success or -1 on failure
 *
 * Blocks until the server processes all currently issued requests and
 * sends out pending events on the event queue.
 *
 * \note This function uses wl_display_dispatch_queue() internally. If you
 * are using wl_display_read_events() from more threads, don't use this function
 * (or make sure that calling wl_display_roundtrip_queue() doesn't interfere
 * with calling wl_display_prepare_read() and wl_display_read_events())
 *
 * \sa wl_display_roundtrip()
 * \memberof wl_event_queue
 */
WL_EXPORT int
wl_display_roundtrip_queue(struct wl_display *display, struct wl_event_queue *queue)
{
	struct wl_callback *callback;
	int done, ret = 0;

	done = 0;
	callback = wl_display_sync(display);
	if (callback == NULL)
		return -1;
	wl_proxy_set_queue((struct wl_proxy *) callback, queue);
	wl_callback_add_listener(callback, &sync_listener, &done);
	while (!done && ret >= 0)
		ret = wl_display_dispatch_queue(display, queue);

	if (ret == -1 && !done)
		wl_callback_destroy(callback);

	return ret;
}

/** Block until all pending request are processed by the server
 *
 * \param display The display context object
 * \return The number of dispatched events on success or -1 on failure
 *
 * Blocks until the server process all currently issued requests and
 * sends out pending events on the default event queue.
 *
 * \note This function uses wl_display_dispatch_queue() internally. If you
 * are using wl_display_read_events() from more threads, don't use this function
 * (or make sure that calling wl_display_roundtrip() doesn't interfere
 * with calling wl_display_prepare_read() and wl_display_read_events())
 *
 * \memberof wl_display
 */
WL_EXPORT int
wl_display_roundtrip(struct wl_display *display)
{
	return wl_display_roundtrip_queue(display, &display->default_queue);
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
	struct wl_event_queue *queue;

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

	if (proxy == &display->proxy)
		queue = &display->display_queue;
	else
		queue = proxy->queue;

	wl_list_insert(queue->event_list.prev, &closure->link);

	return size;
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

	if (proxy->dispatcher) {
		if (debug_client)
			wl_closure_print(closure, &proxy->object, false);

		wl_closure_dispatch(closure, proxy->dispatcher,
				    &proxy->object, opcode);
	} else if (proxy->object.implementation) {
		if (debug_client)
			wl_closure_print(closure, &proxy->object, false);

		wl_closure_invoke(closure, WL_CLOSURE_INVOKE_CLIENT,
				  &proxy->object, opcode, proxy->user_data);
	}

	wl_closure_destroy(closure);

	pthread_mutex_lock(&display->mutex);
}

static int
read_events(struct wl_display *display)
{
	int total, rem, size;
	uint32_t serial;

	display->reader_count--;
	if (display->reader_count == 0) {
		total = wl_connection_read(display->connection);
		if (total == -1) {
			if (errno == EAGAIN) {
				/* we must wake up threads whenever
				 * the reader_count dropped to 0 */
				display_wakeup_threads(display);

				return 0;
			}

			display_fatal_error(display, errno);
			return -1;
		} else if (total == 0) {
			/* The compositor has closed the socket. This
			 * should be considered an error so we'll fake
			 * an errno */
			errno = EPIPE;
			display_fatal_error(display, errno);
			return -1;
		}

		for (rem = total; rem >= 8; rem -= size) {
			size = queue_event(display, rem);
			if (size == -1) {
				display_fatal_error(display, errno);
				return -1;
			} else if (size == 0) {
				break;
			}
		}

		display_wakeup_threads(display);
	} else {
		serial = display->read_serial;
		while (display->read_serial == serial)
			pthread_cond_wait(&display->reader_cond,
					  &display->mutex);

		if (display->last_error) {
			errno = display->last_error;
			return -1;
		}
	}

	return 0;
}

static void
cancel_read(struct wl_display *display)
{
	display->reader_count--;
	if (display->reader_count == 0)
		display_wakeup_threads(display);
}

/** Read events from display file descriptor
 *
 * \param display The display context object
 * \return 0 on success or -1 on error.  In case of error errno will
 * be set accordingly
 *
 * This will read events from the file descriptor for the display.
 * This function does not dispatch events, it only reads and queues
 * events into their corresponding event queues. If no data is
 * available on the file descriptor, wl_display_read_events() returns
 * immediately. To dispatch events that may have been queued, call
 * wl_display_dispatch_pending() or wl_display_dispatch_queue_pending().
 *
 * Before calling this function, wl_display_prepare_read() must be
 * called first. When running in more threads (which is the usual
 * case, since we'd use wl_display_dispatch() otherwise), every thread
 * must call wl_display_prepare_read() before calling this function.
 *
 * After calling wl_display_prepare_read() there can be some extra code
 * before calling wl_display_read_events(), for example poll() or alike.
 * Example of code in a thread:
 *
 * \code
 *
 *   while (wl_display_prepare_read(display) < 0)
 *           wl_display_dispatch_pending(display);
 *   wl_display_flush(display);
 *
 *   ... some code ...
 *
 *   fds[0].fd = wl_display_get_fd(display);
 *   fds[0].events = POLLIN;
 *   poll(fds, 1, -1);
 *
 *   if (!everything_ok()) {
 *	wl_display_cancel_read(display);
 *	handle_error();
 *   }
 *
 *   if (wl_display_read_events(display) < 0)
 *	handle_error();
 *
 *   ...
 * \endcode
 *
 * After wl_display_prepare_read() succeeds, other threads that enter
 * wl_display_read_events() will sleep until the very last thread enters
 * it too or cancels. Therefore when the display fd becomes (or already
 * is) readable, wl_display_read_events() should be called as soon as
 * possible to unblock all threads. If wl_display_read_events() will not
 * be called, then wl_display_cancel_read() must be called instead to let
 * the other threads continue.
 *
 * This function must not be called simultaneously with wl_display_dispatch().
 * It may lead to deadlock. If programmer wants, for some reason, use
 * wl_display_dispatch() in one thread and wl_display_prepare_read() with
 * wl_display_read_events() in another, extra care must be taken to serialize
 * these calls, i. e. use mutexes or similar (on whole prepare + read sequence)
 *
 * \sa wl_display_prepare_read(), wl_display_cancel_read(),
 * wl_display_dispatch_pending(), wl_display_dispatch()
 *
 * \memberof wl_display
 */
WL_EXPORT int
wl_display_read_events(struct wl_display *display)
{
	int ret;

	pthread_mutex_lock(&display->mutex);

	if (display->last_error) {
		cancel_read(display);
		pthread_mutex_unlock(&display->mutex);

		errno = display->last_error;
		return -1;
	}

	ret = read_events(display);

	pthread_mutex_unlock(&display->mutex);

	return ret;
}

static int
dispatch_queue(struct wl_display *display, struct wl_event_queue *queue)
{
	int count;

	if (display->last_error)
		goto err;

	count = 0;
	while (!wl_list_empty(&display->display_queue.event_list)) {
		dispatch_event(display, &display->display_queue);
		if (display->last_error)
			goto err;
		count++;
	}

	while (!wl_list_empty(&queue->event_list)) {
		dispatch_event(display, queue);
		if (display->last_error)
			goto err;
		count++;
	}

	return count;

err:
	errno = display->last_error;

	return -1;
}

/** Prepare to read events from the display to this queue
 *
 * \param display The display context object
 * \param queue The event queue to use
 * \return 0 on success or -1 if event queue was not empty
 *
 * Atomically makes sure the queue is empty and stops any other thread
 * from placing events into this (or any) queue.  Caller must
 * eventually call either wl_display_cancel_read() or
 * wl_display_read_events(), usually after waiting for the
 * display fd to become ready for reading, to release the lock.
 *
 * \sa wl_display_prepare_read
 * \memberof wl_event_queue
 */
WL_EXPORT int
wl_display_prepare_read_queue(struct wl_display *display,
			      struct wl_event_queue *queue)
{
	int ret;

	pthread_mutex_lock(&display->mutex);

	if (!wl_list_empty(&queue->event_list)) {
		errno = EAGAIN;
		ret = -1;
	} else {
		display->reader_count++;
		ret = 0;
	}

	pthread_mutex_unlock(&display->mutex);

	return ret;
}

/** Prepare to read events from the display's file descriptor
 *
 * \param display The display context object
 * \return 0 on success or -1 if event queue was not empty
 *
 * This function must be called before reading from the file
 * descriptor using wl_display_read_events(). Calling
 * wl_display_prepare_read() announces the calling thread's intention
 * to read and ensures that until the thread is ready to read and
 * calls wl_display_read_events(), no other thread will read from the
 * file descriptor. This only succeeds if the event queue is empty
 * though, and if there are undispatched events in the queue, -1 is
 * returned and errno set to EAGAIN.
 *
 * If a thread successfully calls wl_display_prepare_read(), it must
 * either call wl_display_read_events() when it's ready or cancel the
 * read intention by calling wl_display_cancel_read().
 *
 * Use this function before polling on the display fd or to integrate
 * the fd into a toolkit event loop in a race-free way.
 * A correct usage would be (we left out most of error checking):
 *
 * \code
 * while (wl_display_prepare_read(display) != 0)
 *         wl_display_dispatch_pending(display);
 * wl_display_flush(display);
 *
 * ret = poll(fds, nfds, -1);
 * if (has_error(ret))
 *         wl_display_cancel_read(display);
 * else
 *         wl_display_read_events(display);
 *
 * wl_display_dispatch_pending(display);
 * \endcode
 *
 * Here we call wl_display_prepare_read(), which ensures that between
 * returning from that call and eventually calling
 * wl_display_read_events(), no other thread will read from the fd and
 * queue events in our queue. If the call to wl_display_prepare_read() fails,
 * we dispatch the pending events and try again until we're successful.
 *
 * When using wl_display_dispatch() we'd have something like:
 *
 * \code
 * wl_display_dispatch_pending(display);
 * wl_display_flush(display);
 * poll(fds, nfds, -1);
 * wl_display_dispatch(display);
 * \endcode
 *
 * This sequence in not thread-safe. The race is immediately after poll(),
 * where one thread could preempt and read events before the other thread calls
 * wl_display_dispatch(). This call now blocks and starves the other
 * fds in the event loop.
 *
 * Another race would be when using more event queues.
 * When one thread calls wl_display_dispatch(_queue)(), then it
 * reads all events from display's fd and queues them in appropriate
 * queues. Then it dispatches only its own queue and the other events
 * are sitting in their queues, waiting for dispatching. If that happens
 * before the other thread managed to call poll(), it will
 * block with events queued.
 *
 * wl_display_prepare_read() function doesn't acquire exclusive access
 * to the display's fd. It only registers that the thread calling this function
 * has intention to read from fd.
 * When all registered readers call wl_display_read_events(),
 * only one (at random) eventually reads and queues the events and the
 * others are sleeping meanwhile. This way we avoid races and still
 * can read from more threads.
 *
 * If the relevant queue is not the default queue, then
 * wl_display_prepare_read_queue() and wl_display_dispatch_queue_pending()
 * need to be used instead.
 *
 * \sa wl_display_cancel_read(), wl_display_read_events()
 *
 * \memberof wl_display
 */
WL_EXPORT int
wl_display_prepare_read(struct wl_display *display)
{
	return wl_display_prepare_read_queue(display, &display->default_queue);
}

/** Cancel read intention on display's fd
 *
 * \param display The display context object
 *
 * After a thread successfully called wl_display_prepare_read() it must
 * either call wl_display_read_events() or wl_display_cancel_read().
 * If the threads do not follow this rule it will lead to deadlock.
 *
 * \sa wl_display_prepare_read(), wl_display_read_events()
 *
 * \memberof wl_display
 */
WL_EXPORT void
wl_display_cancel_read(struct wl_display *display)
{
	pthread_mutex_lock(&display->mutex);

	cancel_read(display);

	pthread_mutex_unlock(&display->mutex);
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
 * The behaviour of this function is exactly the same as the behaviour of
 * wl_display_dispatch(), but it dispatches events on given queue,
 * not on the default queue.
 *
 * This function blocks if there are no events to dispatch (if there are,
 * it only dispatches these events and returns immediately).
 * When this function returns after blocking, it means that it read events
 * from display's fd and queued them to appropriate queues.
 * If among the incoming events were some events assigned to the given queue,
 * they are dispatched by this moment.
 *
 * \note Since Wayland 1.5 the display has an extra queue
 * for its own events (i. e. delete_id). This queue is dispatched always,
 * no matter what queue we passed as an argument to this function.
 * That means that this function can return non-0 value even when it
 * haven't dispatched any event for the given queue.
 *
 * This function has the same constrains for using in multi-threaded apps
 * as \ref wl_display_dispatch().
 *
 * \sa wl_display_dispatch(), wl_display_dispatch_pending(),
 * wl_display_dispatch_queue_pending()
 *
 * \memberof wl_event_queue
 */
WL_EXPORT int
wl_display_dispatch_queue(struct wl_display *display,
			  struct wl_event_queue *queue)
{
	struct pollfd pfd[2];
	int ret;

	pthread_mutex_lock(&display->mutex);

	ret = dispatch_queue(display, queue);
	if (ret == -1)
		goto err_unlock;
	if (ret > 0) {
		pthread_mutex_unlock(&display->mutex);
		return ret;
	}

	/* We ignore EPIPE here, so that we try to read events before
	 * returning an error.  When the compositor sends an error it
	 * will close the socket, and if we bail out here we don't get
	 * a chance to process the error. */
	ret = wl_connection_flush(display->connection);
	if (ret < 0 && errno != EAGAIN && errno != EPIPE) {
		display_fatal_error(display, errno);
		goto err_unlock;
	}

	display->reader_count++;

	pthread_mutex_unlock(&display->mutex);

	pfd[0].fd = display->fd;
	pfd[0].events = POLLIN;
	do {
		ret = poll(pfd, 1, -1);
	} while (ret == -1 && errno == EINTR);

	if (ret == -1) {
		wl_display_cancel_read(display);
		return -1;
	}

	pthread_mutex_lock(&display->mutex);

	if (read_events(display) == -1)
		goto err_unlock;

	ret = dispatch_queue(display, queue);
	if (ret == -1)
		goto err_unlock;

	pthread_mutex_unlock(&display->mutex);

	return ret;

 err_unlock:
	pthread_mutex_unlock(&display->mutex);
	return -1;
}

/** Dispatch pending events in an event queue
 *
 * \param display The display context object
 * \param queue The event queue to dispatch
 * \return The number of dispatched events on success or -1 on failure
 *
 * Dispatch all incoming events for objects assigned to the given
 * event queue. On failure -1 is returned and errno set appropriately.
 * If there are no events queued, this function returns immediately.
 *
 * \memberof wl_event_queue
 * \since 1.0.2
 */
WL_EXPORT int
wl_display_dispatch_queue_pending(struct wl_display *display,
				  struct wl_event_queue *queue)
{
	int ret;

	pthread_mutex_lock(&display->mutex);

	ret = dispatch_queue(display, queue);

	pthread_mutex_unlock(&display->mutex);

	return ret;
}

/** Process incoming events
 *
 * \param display The display context object
 * \return The number of dispatched events on success or -1 on failure
 *
 * Dispatch the display's default event queue.
 *
 * If the default event queue is empty, this function blocks until there are
 * events to be read from the display fd. Events are read and queued on
 * the appropriate event queues. Finally, events on the default event queue
 * are dispatched.
 *
 * In multi-threaded environment, programmer may want to use
 * wl_display_read_events(). However, use of wl_display_read_events()
 * must not be mixed with wl_display_dispatch(). See wl_display_read_events()
 * and wl_display_prepare_read() for more details.
 *
 * \note It is not possible to check if there are events on the queue
 * or not. For dispatching default queue events without blocking, see \ref
 * wl_display_dispatch_pending().
 *
 * \sa wl_display_dispatch_pending(), wl_display_dispatch_queue(),
 * wl_display_read_events()
 *
 * \memberof wl_display
 */
WL_EXPORT int
wl_display_dispatch(struct wl_display *display)
{
	return wl_display_dispatch_queue(display, &display->default_queue);
}

/** Dispatch default queue events without reading from the display fd
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
 * events in other queues while reading all data from the display fd.
 * When the main loop returns from the handler, the display fd
 * no longer has data, causing a call to \em poll(2) (or similar
 * functions) to block indefinitely, even though there are events ready
 * to dispatch.
 *
 * To proper integrate the wayland display fd into a main loop, the
 * client should always call wl_display_dispatch_pending() and then
 * \ref wl_display_flush() prior to going back to sleep. At that point,
 * the fd typically doesn't have data so attempting I/O could block, but
 * events queued up on the default queue should be dispatched.
 *
 * A real-world example is a main loop that wakes up on a timerfd (or a
 * sound card fd becoming writable, for example in a video player), which
 * then triggers GL rendering and eventually eglSwapBuffers().
 * eglSwapBuffers() may call wl_display_dispatch_queue() if it didn't
 * receive the frame event for the previous frame, and as such queue
 * events in the default queue.
 *
 * \sa wl_display_dispatch(), wl_display_dispatch_queue(),
 * wl_display_flush()
 *
 * \memberof wl_display
 */
WL_EXPORT int
wl_display_dispatch_pending(struct wl_display *display)
{
	return wl_display_dispatch_queue_pending(display,
						 &display->default_queue);
}

/** Retrieve the last error that occurred on a display
 *
 * \param display The display context object
 * \return The last error that occurred on \c display or 0 if no error occurred
 *
 * Return the last error that occurred on the display. This may be an error sent
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

/** Retrieves the information about a protocol error:
 *
 * \param display    The Wayland display
 * \param interface  if not NULL, stores the interface where the error occurred
 * \param id         if not NULL, stores the object id that generated
 *                   the error. There's no guarantee the object is
 *                   still valid; the client must know if it deleted the object.
 * \return           The error code as defined in the interface specification.
 *
 * \code
 * int err = wl_display_get_error(display);
 *
 * if (err == EPROTO) {
 *        code = wl_display_get_protocol_error(display, &interface, &id);
 *        handle_error(code, interface, id);
 * }
 *
 * ...
 * \endcode
 * \memberof wl_display
 */
WL_EXPORT uint32_t
wl_display_get_protocol_error(struct wl_display *display,
			      const struct wl_interface **interface,
			      uint32_t *id)
{
	uint32_t ret;

	pthread_mutex_lock(&display->mutex);

	ret = display->protocol_error.code;

	if (interface)
		*interface = display->protocol_error.interface;
	if (id)
		*id = display->protocol_error.id;

	pthread_mutex_unlock(&display->mutex);

	return ret;
}


/** Send all buffered requests on the display to the server
 *
 * \param display The display context object
 * \return The number of bytes sent on success or -1 on failure
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
 * \param queue The event queue that will handle this proxy or NULL
 *
 * Assign proxy to event queue. Events coming from \c proxy will be
 * queued in \c queue from now. If queue is NULL, then the display's
 * default queue is set to the proxy.
 *
 * \note By default, the queue set in proxy is the one inherited from parent.
 *
 * \sa wl_display_dispatch_queue()
 *
 * \memberof wl_proxy
 */
WL_EXPORT void
wl_proxy_set_queue(struct wl_proxy *proxy, struct wl_event_queue *queue)
{
	if (queue)
		proxy->queue = queue;
	else
		proxy->queue = &proxy->display->default_queue;
}

WL_EXPORT void
wl_log_set_handler_client(wl_log_func_t handler)
{
	wl_log_handler = handler;
}
