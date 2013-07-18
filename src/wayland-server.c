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

#define _GNU_SOURCE

#include <stdlib.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdbool.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <dlfcn.h>
#include <assert.h>
#include <sys/time.h>
#include <fcntl.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <ffi.h>

#include "wayland-private.h"
#include "wayland-server.h"
#include "wayland-server-protocol.h"
#include "wayland-os.h"

/* This is the size of the char array in struct sock_addr_un.
   No Wayland socket can be created with a path longer than this,
   including the null terminator. */
#ifndef UNIX_PATH_MAX
#define UNIX_PATH_MAX	108
#endif

#define LOCK_SUFFIX	".lock"
#define LOCK_SUFFIXLEN	5

struct wl_socket {
	int fd;
	int fd_lock;
	struct sockaddr_un addr;
	char lock_addr[UNIX_PATH_MAX + LOCK_SUFFIXLEN];
	struct wl_list link;
	struct wl_event_source *source;
};

struct wl_client {
	struct wl_connection *connection;
	struct wl_event_source *source;
	struct wl_display *display;
	struct wl_resource *display_resource;
	uint32_t id_count;
	uint32_t mask;
	struct wl_list link;
	struct wl_map objects;
	struct wl_signal destroy_signal;
	struct ucred ucred;
	int error;
};

struct wl_display {
	struct wl_event_loop *loop;
	int run;

	uint32_t id;
	uint32_t serial;

	struct wl_list registry_resource_list;
	struct wl_list global_list;
	struct wl_list socket_list;
	struct wl_list client_list;

	struct wl_signal destroy_signal;

	struct wl_array additional_shm_formats;
};

struct wl_global {
	struct wl_display *display;
	const struct wl_interface *interface;
	uint32_t name;
	uint32_t version;
	void *data;
	wl_global_bind_func_t bind;
	struct wl_list link;
};

struct wl_resource {
	struct wl_object object;
	wl_resource_destroy_func_t destroy;
	struct wl_list link;
	struct wl_signal destroy_signal;
	struct wl_client *client;
	void *data;
	int version;
	wl_dispatcher_func_t dispatcher;
};

static int wl_debug = 0;

WL_EXPORT void
wl_resource_post_event_array(struct wl_resource *resource, uint32_t opcode,
			     union wl_argument *args)
{
	struct wl_closure *closure;
	struct wl_object *object = &resource->object;

	closure = wl_closure_marshal(object, opcode, args,
				     &object->interface->events[opcode]);

	if (closure == NULL) {
		resource->client->error = 1;
		return;
	}

	if (wl_closure_send(closure, resource->client->connection))
		resource->client->error = 1;

	if (wl_debug)
		wl_closure_print(closure, object, true);

	wl_closure_destroy(closure);
}

WL_EXPORT void
wl_resource_post_event(struct wl_resource *resource, uint32_t opcode, ...)
{
	union wl_argument args[WL_CLOSURE_MAX_ARGS];
	struct wl_object *object = &resource->object;
	va_list ap;

	va_start(ap, opcode);
	wl_argument_from_va_list(object->interface->events[opcode].signature,
				 args, WL_CLOSURE_MAX_ARGS, ap);
	va_end(ap);

	wl_resource_post_event_array(resource, opcode, args);
}


WL_EXPORT void
wl_resource_queue_event_array(struct wl_resource *resource, uint32_t opcode,
			      union wl_argument *args)
{
	struct wl_closure *closure;
	struct wl_object *object = &resource->object;

	closure = wl_closure_marshal(object, opcode, args,
				     &object->interface->events[opcode]);

	if (closure == NULL) {
		resource->client->error = 1;
		return;
	}

	if (wl_closure_queue(closure, resource->client->connection))
		resource->client->error = 1;

	if (wl_debug)
		wl_closure_print(closure, object, true);

	wl_closure_destroy(closure);
}

WL_EXPORT void
wl_resource_queue_event(struct wl_resource *resource, uint32_t opcode, ...)
{
	union wl_argument args[WL_CLOSURE_MAX_ARGS];
	struct wl_object *object = &resource->object;
	va_list ap;

	va_start(ap, opcode);
	wl_argument_from_va_list(object->interface->events[opcode].signature,
				 args, WL_CLOSURE_MAX_ARGS, ap);
	va_end(ap);

	wl_resource_queue_event_array(resource, opcode, args);
}

WL_EXPORT void
wl_resource_post_error(struct wl_resource *resource,
		       uint32_t code, const char *msg, ...)
{
	struct wl_client *client = resource->client;
	char buffer[128];
	va_list ap;

	va_start(ap, msg);
	vsnprintf(buffer, sizeof buffer, msg, ap);
	va_end(ap);

	client->error = 1;

	/*
	 * When a client aborts, its resources are destroyed in id order,
	 * which means the display resource is destroyed first. If destruction
	 * of any later resources results in a protocol error, we end up here
	 * with a NULL display_resource. Do not try to send errors to an
	 * already dead client.
	 */
	if (!client->display_resource)
		return;

	wl_resource_post_event(client->display_resource,
			       WL_DISPLAY_ERROR, resource, code, buffer);
}

static int
wl_client_connection_data(int fd, uint32_t mask, void *data)
{
	struct wl_client *client = data;
	struct wl_connection *connection = client->connection;
	struct wl_resource *resource;
	struct wl_object *object;
	struct wl_closure *closure;
	const struct wl_message *message;
	uint32_t p[2];
	uint32_t resource_flags;
	int opcode, size;
	int len;

	if (mask & (WL_EVENT_ERROR | WL_EVENT_HANGUP)) {
		wl_client_destroy(client);
		return 1;
	}

	if (mask & WL_EVENT_WRITABLE) {
		len = wl_connection_flush(connection);
		if (len < 0 && errno != EAGAIN) {
			wl_client_destroy(client);
			return 1;
		} else if (len >= 0) {
			wl_event_source_fd_update(client->source,
						  WL_EVENT_READABLE);
		}
	}

	len = 0;
	if (mask & WL_EVENT_READABLE) {
		len = wl_connection_read(connection);
		if (len < 0 && errno != EAGAIN) {
			wl_client_destroy(client);
			return 1;
		}
	}

	while ((size_t) len >= sizeof p) {
		wl_connection_copy(connection, p, sizeof p);
		opcode = p[1] & 0xffff;
		size = p[1] >> 16;
		if (len < size)
			break;

		resource = wl_map_lookup(&client->objects, p[0]);
		resource_flags = wl_map_lookup_flags(&client->objects, p[0]);
		if (resource == NULL) {
			wl_resource_post_error(client->display_resource,
					       WL_DISPLAY_ERROR_INVALID_OBJECT,
					       "invalid object %u", p[0]);
			break;
		}

		object = &resource->object;
		if (opcode >= object->interface->method_count) {
			wl_resource_post_error(client->display_resource,
					       WL_DISPLAY_ERROR_INVALID_METHOD,
					       "invalid method %d, object %s@%u",
					       opcode,
					       object->interface->name,
					       object->id);
			break;
		}

		message = &object->interface->methods[opcode];
		if (!(resource_flags & WL_MAP_ENTRY_LEGACY) &&
		    resource->version > 0 &&
		    resource->version < wl_message_get_since(message)) {
			wl_resource_post_error(client->display_resource,
					       WL_DISPLAY_ERROR_INVALID_METHOD,
					       "invalid method %d, object %s@%u",
					       opcode,
					       object->interface->name,
					       object->id);
			break;
		}


		closure = wl_connection_demarshal(client->connection, size,
						  &client->objects, message);
		len -= size;

		if (closure == NULL && errno == ENOMEM) {
			wl_resource_post_no_memory(resource);
			break;
		} else if ((closure == NULL && errno == EINVAL) ||
			   wl_closure_lookup_objects(closure, &client->objects) < 0) {
			wl_resource_post_error(client->display_resource,
					       WL_DISPLAY_ERROR_INVALID_METHOD,
					       "invalid arguments for %s@%u.%s",
					       object->interface->name,
					       object->id,
					       message->name);
			break;
		}

		if (wl_debug)
			wl_closure_print(closure, object, false);

		if ((resource_flags & WL_MAP_ENTRY_LEGACY) ||
		    resource->dispatcher == NULL) {
			wl_closure_invoke(closure, WL_CLOSURE_INVOKE_SERVER,
					  object, opcode, client);
		} else {
			wl_closure_dispatch(closure, resource->dispatcher,
					    object, opcode);
		}

		wl_closure_destroy(closure);

		if (client->error)
			break;
	}

	if (client->error)
		wl_client_destroy(client);

	return 1;
}

WL_EXPORT void
wl_client_flush(struct wl_client *client)
{
	wl_connection_flush(client->connection);
}

WL_EXPORT struct wl_display *
wl_client_get_display(struct wl_client *client)
{
	return client->display;
}

static void
bind_display(struct wl_client *client,
	     void *data, uint32_t version, uint32_t id);

WL_EXPORT struct wl_client *
wl_client_create(struct wl_display *display, int fd)
{
	struct wl_client *client;
	socklen_t len;

	client = malloc(sizeof *client);
	if (client == NULL)
		return NULL;

	memset(client, 0, sizeof *client);
	client->display = display;
	client->source = wl_event_loop_add_fd(display->loop, fd,
					      WL_EVENT_READABLE,
					      wl_client_connection_data, client);

	if (!client->source)
		goto err_client;

	len = sizeof client->ucred;
	if (getsockopt(fd, SOL_SOCKET, SO_PEERCRED,
		       &client->ucred, &len) < 0)
		goto err_source;

	client->connection = wl_connection_create(fd);
	if (client->connection == NULL)
		goto err_source;

	wl_map_init(&client->objects, WL_MAP_SERVER_SIDE);

	if (wl_map_insert_at(&client->objects, 0, 0, NULL) < 0)
		goto err_map;

	wl_signal_init(&client->destroy_signal);
	bind_display(client, display, 1, 1);

	if (!client->display_resource)
		goto err_map;

	wl_list_insert(display->client_list.prev, &client->link);

	return client;

err_map:
	wl_map_release(&client->objects);
	wl_connection_destroy(client->connection);
err_source:
	wl_event_source_remove(client->source);
err_client:
	free(client);
	return NULL;
}

WL_EXPORT void
wl_client_get_credentials(struct wl_client *client,
			  pid_t *pid, uid_t *uid, gid_t *gid)
{
	if (pid)
		*pid = client->ucred.pid;
	if (uid)
		*uid = client->ucred.uid;
	if (gid)
		*gid = client->ucred.gid;
}

WL_EXPORT struct wl_resource *
wl_client_get_object(struct wl_client *client, uint32_t id)
{
	return wl_map_lookup(&client->objects, id);
}

WL_EXPORT void
wl_client_post_no_memory(struct wl_client *client)
{
	wl_resource_post_error(client->display_resource,
			       WL_DISPLAY_ERROR_NO_MEMORY, "no memory");
}

WL_EXPORT void
wl_resource_post_no_memory(struct wl_resource *resource)
{
	wl_resource_post_error(resource->client->display_resource,
			       WL_DISPLAY_ERROR_NO_MEMORY, "no memory");
}

static void
destroy_resource(void *element, void *data)
{
	struct wl_resource *resource = element;
	struct wl_client *client = resource->client;
	uint32_t flags;

	wl_signal_emit(&resource->destroy_signal, resource);

	flags = wl_map_lookup_flags(&client->objects, resource->object.id);
	if (resource->destroy)
		resource->destroy(resource);

	if (!(flags & WL_MAP_ENTRY_LEGACY))
		free(resource);
}

WL_EXPORT void
wl_resource_destroy(struct wl_resource *resource)
{
	struct wl_client *client = resource->client;
	uint32_t id;

	id = resource->object.id;
	destroy_resource(resource, NULL);

	if (id < WL_SERVER_ID_START) {
		if (client->display_resource) {
			wl_resource_queue_event(client->display_resource,
						WL_DISPLAY_DELETE_ID, id);
		}
		wl_map_insert_at(&client->objects, 0, id, NULL);
	} else {
		wl_map_remove(&client->objects, id);
	}
}

WL_EXPORT uint32_t
wl_resource_get_id(struct wl_resource *resource)
{
	return resource->object.id;
}

WL_EXPORT struct wl_list *
wl_resource_get_link(struct wl_resource *resource)
{
	return &resource->link;
}

WL_EXPORT struct wl_resource *
wl_resource_from_link(struct wl_list *link)
{
	struct wl_resource *resource;

	return wl_container_of(link, resource, link);
}

WL_EXPORT struct wl_resource *
wl_resource_find_for_client(struct wl_list *list, struct wl_client *client)
{
	struct wl_resource *resource;

	if (client == NULL)
		return NULL;

        wl_list_for_each(resource, list, link) {
                if (resource->client == client)
                        return resource;
        }

        return NULL;
}

WL_EXPORT struct wl_client *
wl_resource_get_client(struct wl_resource *resource)
{
	return resource->client;
}

WL_EXPORT void
wl_resource_set_user_data(struct wl_resource *resource, void *data)
{
	resource->data = data;
}

WL_EXPORT void *
wl_resource_get_user_data(struct wl_resource *resource)
{
	return resource->data;
}

WL_EXPORT int
wl_resource_get_version(struct wl_resource *resource)
{
	return resource->version;
}

WL_EXPORT void
wl_resource_set_destructor(struct wl_resource *resource,
			   wl_resource_destroy_func_t destroy)
{
	resource->destroy = destroy;
}

WL_EXPORT int
wl_resource_instance_of(struct wl_resource *resource,
			const struct wl_interface *interface,
			const void *implementation)
{
	return wl_interface_equal(resource->object.interface, interface) &&
		resource->object.implementation == implementation;
}

WL_EXPORT void
wl_resource_add_destroy_listener(struct wl_resource *resource,
				 struct wl_listener * listener)
{
	wl_signal_add(&resource->destroy_signal, listener);
}

WL_EXPORT struct wl_listener *
wl_resource_get_destroy_listener(struct wl_resource *resource,
				 wl_notify_func_t notify)
{
	return wl_signal_get(&resource->destroy_signal, notify);
}

WL_EXPORT void
wl_client_add_destroy_listener(struct wl_client *client,
			       struct wl_listener *listener)
{
	wl_signal_add(&client->destroy_signal, listener);
}

WL_EXPORT struct wl_listener *
wl_client_get_destroy_listener(struct wl_client *client,
			       wl_notify_func_t notify)
{
	return wl_signal_get(&client->destroy_signal, notify);
}

WL_EXPORT void
wl_client_destroy(struct wl_client *client)
{
	uint32_t serial = 0;
	
	wl_log("disconnect from client %p\n", client);

	wl_signal_emit(&client->destroy_signal, client);

	wl_client_flush(client);
	wl_map_for_each(&client->objects, destroy_resource, &serial);
	wl_map_release(&client->objects);
	wl_event_source_remove(client->source);
	wl_connection_destroy(client->connection);
	wl_list_remove(&client->link);
	free(client);
}

static void
registry_bind(struct wl_client *client,
	      struct wl_resource *resource, uint32_t name,
	      const char *interface, uint32_t version, uint32_t id)
{
	struct wl_global *global;
	struct wl_display *display = resource->data;

	wl_list_for_each(global, &display->global_list, link)
		if (global->name == name)
			break;

	if (&global->link == &display->global_list ||
	    global->version < version)
		wl_resource_post_error(resource,
				       WL_DISPLAY_ERROR_INVALID_OBJECT,
				       "invalid global %d", name);
	else
		global->bind(client, global->data, version, id);
}

static const struct wl_registry_interface registry_interface = {
	registry_bind
};

static void
display_sync(struct wl_client *client,
	     struct wl_resource *resource, uint32_t id)
{
	struct wl_resource *callback;
	uint32_t serial;

	callback = wl_resource_create(client, &wl_callback_interface, 1, id);
	if (callback == NULL) {
		wl_client_post_no_memory(client);
		return;
	}

	serial = wl_display_get_serial(client->display);
	wl_callback_send_done(callback, serial);
	wl_resource_destroy(callback);
}

static void
unbind_resource(struct wl_resource *resource)
{
	wl_list_remove(&resource->link);
}

static void
display_get_registry(struct wl_client *client,
		     struct wl_resource *resource, uint32_t id)
{
	struct wl_display *display = resource->data;
	struct wl_resource *registry_resource;
	struct wl_global *global;

	registry_resource =
		wl_resource_create(client, &wl_registry_interface, 1, id);
	if (registry_resource == NULL) {
		wl_client_post_no_memory(client);
		return;
	}

	wl_resource_set_implementation(registry_resource,
				       &registry_interface,
				       display, unbind_resource);

	wl_list_insert(&display->registry_resource_list,
		       &registry_resource->link);

	wl_list_for_each(global, &display->global_list, link)
		wl_resource_post_event(registry_resource,
				       WL_REGISTRY_GLOBAL,
				       global->name,
				       global->interface->name,
				       global->version);
}

static const struct wl_display_interface display_interface = {
	display_sync,
	display_get_registry
};

static void
destroy_client_display_resource(struct wl_resource *resource)
{
	resource->client->display_resource = NULL;
}

static void
bind_display(struct wl_client *client,
	     void *data, uint32_t version, uint32_t id)
{
	struct wl_display *display = data;

	client->display_resource =
		wl_resource_create(client, &wl_display_interface, 1, id);
	if (client->display_resource == NULL) {
		wl_client_post_no_memory(client);
		return;
	}

	wl_resource_set_implementation(client->display_resource,
				       &display_interface, display,
				       destroy_client_display_resource);
}

WL_EXPORT struct wl_display *
wl_display_create(void)
{
	struct wl_display *display;
	const char *debug;

	debug = getenv("WAYLAND_DEBUG");
	if (debug && (strstr(debug, "server") || strstr(debug, "1")))
		wl_debug = 1;

	display = malloc(sizeof *display);
	if (display == NULL)
		return NULL;

	display->loop = wl_event_loop_create();
	if (display->loop == NULL) {
		free(display);
		return NULL;
	}

	wl_list_init(&display->global_list);
	wl_list_init(&display->socket_list);
	wl_list_init(&display->client_list);
	wl_list_init(&display->registry_resource_list);

	wl_signal_init(&display->destroy_signal);

	display->id = 1;
	display->serial = 0;

	wl_array_init(&display->additional_shm_formats);

	if (!wl_global_create(display, &wl_display_interface, 1,
			      display, bind_display)) {
		wl_event_loop_destroy(display->loop);
		free(display);
		return NULL;
	}

	return display;
}

WL_EXPORT void
wl_display_destroy(struct wl_display *display)
{
	struct wl_socket *s, *next;
	struct wl_global *global, *gnext;

	wl_signal_emit(&display->destroy_signal, display);

	wl_list_for_each_safe(s, next, &display->socket_list, link) {
		wl_event_source_remove(s->source);
		unlink(s->addr.sun_path);
		close(s->fd);
		unlink(s->lock_addr);
		close(s->fd_lock);
		free(s);
	}
	wl_event_loop_destroy(display->loop);

	wl_list_for_each_safe(global, gnext, &display->global_list, link)
		free(global);

	wl_array_release(&display->additional_shm_formats);

	free(display);
}

WL_EXPORT struct wl_global *
wl_global_create(struct wl_display *display,
		 const struct wl_interface *interface, int version,
		 void *data, wl_global_bind_func_t bind)
{
	struct wl_global *global;
	struct wl_resource *resource;

	if (interface->version < version) {
		wl_log("wl_global_create: implemented version higher "
		       "than interface version%m\n");
		return NULL;
	}

	global = malloc(sizeof *global);
	if (global == NULL)
		return NULL;

	global->display = display;
	global->name = display->id++;
	global->interface = interface;
	global->version = version;
	global->data = data;
	global->bind = bind;
	wl_list_insert(display->global_list.prev, &global->link);

	wl_list_for_each(resource, &display->registry_resource_list, link)
		wl_resource_post_event(resource,
				       WL_REGISTRY_GLOBAL,
				       global->name,
				       global->interface->name,
				       global->version);

	return global;
}

WL_EXPORT void
wl_global_destroy(struct wl_global *global)
{
	struct wl_display *display = global->display;
	struct wl_resource *resource;

	wl_list_for_each(resource, &display->registry_resource_list, link)
		wl_resource_post_event(resource, WL_REGISTRY_GLOBAL_REMOVE,
				       global->name);
	wl_list_remove(&global->link);
	free(global);
}

WL_EXPORT uint32_t
wl_display_get_serial(struct wl_display *display)
{
	return display->serial;
}

WL_EXPORT uint32_t
wl_display_next_serial(struct wl_display *display)
{
	display->serial++;

	return display->serial;
}

WL_EXPORT struct wl_event_loop *
wl_display_get_event_loop(struct wl_display *display)
{
	return display->loop;
}

WL_EXPORT void
wl_display_terminate(struct wl_display *display)
{
	display->run = 0;
}

WL_EXPORT void
wl_display_run(struct wl_display *display)
{
	display->run = 1;

	while (display->run) {
		wl_display_flush_clients(display);
		wl_event_loop_dispatch(display->loop, -1);
	}
}

WL_EXPORT void
wl_display_flush_clients(struct wl_display *display)
{
	struct wl_client *client, *next;
	int ret;

	wl_list_for_each_safe(client, next, &display->client_list, link) {
		ret = wl_connection_flush(client->connection);
		if (ret < 0 && errno == EAGAIN) {
			wl_event_source_fd_update(client->source,
						  WL_EVENT_WRITABLE |
						  WL_EVENT_READABLE);
		} else if (ret < 0) {
			wl_client_destroy(client);
		}
	}
}

static int
socket_data(int fd, uint32_t mask, void *data)
{
	struct wl_display *display = data;
	struct sockaddr_un name;
	socklen_t length;
	int client_fd;

	length = sizeof name;
	client_fd = wl_os_accept_cloexec(fd, (struct sockaddr *) &name,
					 &length);
	if (client_fd < 0)
		wl_log("failed to accept: %m\n");
	else
		if (!wl_client_create(display, client_fd))
			close(client_fd);

	return 1;
}

static int
get_socket_lock(struct wl_socket *socket)
{
	struct stat socket_stat;
	int fd_lock;

	snprintf(socket->lock_addr, sizeof socket->lock_addr,
		 "%s%s", socket->addr.sun_path, LOCK_SUFFIX);

	fd_lock = open(socket->lock_addr, O_CREAT | O_CLOEXEC,
	               (S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP));

	if (fd_lock < 0) {
		wl_log("unable to open lockfile %s check permissions\n",
			socket->lock_addr);
		return -1;
	}

	if (flock(fd_lock, LOCK_EX | LOCK_NB) < 0) {
		wl_log("unable to lock lockfile %s, maybe another compositor is running\n",
			socket->lock_addr);
		close(fd_lock);
		return -1;
	}

	if (stat(socket->addr.sun_path, &socket_stat) < 0 ) {
		if (errno != ENOENT) {
			wl_log("did not manage to stat file %s\n",
				socket->addr.sun_path);
			close(fd_lock);
			return -1;
		}
	} else if (socket_stat.st_mode & S_IWUSR ||
		   socket_stat.st_mode & S_IWGRP) {
		unlink(socket->addr.sun_path);
	}

	return fd_lock;
}

WL_EXPORT int
wl_display_add_socket(struct wl_display *display, const char *name)
{
	struct wl_socket *s;
	socklen_t size;
	int name_size;
	const char *runtime_dir;

	runtime_dir = getenv("XDG_RUNTIME_DIR");
	if (!runtime_dir) {
		wl_log("error: XDG_RUNTIME_DIR not set in the environment\n");

		/* to prevent programs reporting
		 * "failed to add socket: Success" */
		errno = ENOENT;
		return -1;
	}

	s = malloc(sizeof *s);
	if (s == NULL)
		return -1;

	s->fd = wl_os_socket_cloexec(PF_LOCAL, SOCK_STREAM, 0);
	if (s->fd < 0) {
		free(s);
		return -1;
	}

	if (name == NULL)
		name = getenv("WAYLAND_DISPLAY");
	if (name == NULL)
		name = "wayland-0";

	memset(&s->addr, 0, sizeof s->addr);
	s->addr.sun_family = AF_LOCAL;
	name_size = snprintf(s->addr.sun_path, sizeof s->addr.sun_path,
			     "%s/%s", runtime_dir, name) + 1;

	assert(name_size > 0);
	if (name_size > (int)sizeof s->addr.sun_path) {
		wl_log("error: socket path \"%s/%s\" plus null terminator"
		       " exceeds 108 bytes\n", runtime_dir, name);
		close(s->fd);
		free(s);
		/* to prevent programs reporting
		 * "failed to add socket: Success" */
		errno = ENAMETOOLONG;
		return -1;
	};

	wl_log("using socket %s\n", s->addr.sun_path);

	s->fd_lock = get_socket_lock(s);
	if (s->fd_lock < 0) {
		close(s->fd);
		free(s);
		return -1;
	}

	size = offsetof (struct sockaddr_un, sun_path) + name_size;
	if (bind(s->fd, (struct sockaddr *) &s->addr, size) < 0) {
		wl_log("bind() failed with error: %m\n");
		close(s->fd);
		unlink(s->lock_addr);
		close(s->fd_lock);
		free(s);
		return -1;
	}

	if (listen(s->fd, 1) < 0) {
		wl_log("listen() failed with error: %m\n");
		unlink(s->addr.sun_path);
		close(s->fd);
		unlink(s->lock_addr);
		close(s->fd_lock);
		free(s);
		return -1;
	}

	s->source = wl_event_loop_add_fd(display->loop, s->fd,
					 WL_EVENT_READABLE,
					 socket_data, display);
	if (s->source == NULL) {
		unlink(s->addr.sun_path);
		close(s->fd);
		unlink(s->lock_addr);
		close(s->fd_lock);
		free(s);
		return -1;
	}
	wl_list_insert(display->socket_list.prev, &s->link);

	return 0;
}

WL_EXPORT void
wl_display_add_destroy_listener(struct wl_display *display,
				struct wl_listener *listener)
{
	wl_signal_add(&display->destroy_signal, listener);
}

WL_EXPORT struct wl_listener *
wl_display_get_destroy_listener(struct wl_display *display,
				wl_notify_func_t notify)
{
	return wl_signal_get(&display->destroy_signal, notify);
}

WL_EXPORT void
wl_resource_set_implementation(struct wl_resource *resource,
			       const void *implementation,
			       void *data, wl_resource_destroy_func_t destroy)
{
	resource->object.implementation = implementation;
	resource->data = data;
	resource->destroy = destroy;
	resource->dispatcher = NULL;
}

WL_EXPORT void
wl_resource_set_dispatcher(struct wl_resource *resource,
			   wl_dispatcher_func_t dispatcher,
			   const void *implementation,
			   void *data, wl_resource_destroy_func_t destroy)
{
	resource->dispatcher = dispatcher;
	resource->object.implementation = implementation;
	resource->data = data;
	resource->destroy = destroy;
}

WL_EXPORT struct wl_resource *
wl_resource_create(struct wl_client *client,
		   const struct wl_interface *interface,
		   int version, uint32_t id)
{
	struct wl_resource *resource;

	resource = malloc(sizeof *resource);
	if (resource == NULL)
		return NULL;

	if (id == 0)
		id = wl_map_insert_new(&client->objects, 0, NULL);

	resource->object.id = id;
	resource->object.interface = interface;
	resource->object.implementation = NULL;

	wl_signal_init(&resource->destroy_signal);

	resource->destroy = NULL;
	resource->client = client;
	resource->data = NULL;
	resource->version = version;
	resource->dispatcher = NULL;

	if (wl_map_insert_at(&client->objects, 0, resource->object.id, resource) < 0) {
		wl_resource_post_error(client->display_resource,
				       WL_DISPLAY_ERROR_INVALID_OBJECT,
				       "invalid new id %d",
				       resource->object.id);
		free(resource);
		return NULL;
	}

	return resource;
}

WL_EXPORT void
wl_log_set_handler_server(wl_log_func_t handler)
{
	wl_log_handler = handler;
}

/* Deprecated functions below. */

uint32_t
wl_client_add_resource(struct wl_client *client,
		       struct wl_resource *resource) WL_DEPRECATED;

WL_EXPORT uint32_t
wl_client_add_resource(struct wl_client *client,
		       struct wl_resource *resource)
{
	if (resource->object.id == 0) {
		resource->object.id =
			wl_map_insert_new(&client->objects,
					  WL_MAP_ENTRY_LEGACY, resource);
	} else if (wl_map_insert_at(&client->objects, WL_MAP_ENTRY_LEGACY,
				  resource->object.id, resource) < 0) {
		wl_resource_post_error(client->display_resource,
				       WL_DISPLAY_ERROR_INVALID_OBJECT,
				       "invalid new id %d",
				       resource->object.id);
		return 0;
	}

	resource->client = client;
	wl_signal_init(&resource->destroy_signal);

	return resource->object.id;
}

struct wl_resource *
wl_client_add_object(struct wl_client *client,
		     const struct wl_interface *interface,
		     const void *implementation,
		     uint32_t id, void *data) WL_DEPRECATED;

WL_EXPORT struct wl_resource *
wl_client_add_object(struct wl_client *client,
		     const struct wl_interface *interface,
		     const void *implementation, uint32_t id, void *data)
{
	struct wl_resource *resource;

	resource = wl_resource_create(client, interface, -1, id);
	if (resource == NULL)
		wl_client_post_no_memory(client);
	else
		wl_resource_set_implementation(resource,
					       implementation, data, NULL);

	return resource;
}

struct wl_resource *
wl_client_new_object(struct wl_client *client,
		     const struct wl_interface *interface,
		     const void *implementation, void *data) WL_DEPRECATED;

WL_EXPORT struct wl_resource *
wl_client_new_object(struct wl_client *client,
		     const struct wl_interface *interface,
		     const void *implementation, void *data)
{
	struct wl_resource *resource;

	resource = wl_resource_create(client, interface, -1, 0);
	if (resource == NULL)
		wl_client_post_no_memory(client);
	else
		wl_resource_set_implementation(resource,
					       implementation, data, NULL);

	return resource;
}

struct wl_global *
wl_display_add_global(struct wl_display *display,
		      const struct wl_interface *interface,
		      void *data, wl_global_bind_func_t bind) WL_DEPRECATED;

WL_EXPORT struct wl_global *
wl_display_add_global(struct wl_display *display,
		      const struct wl_interface *interface,
		      void *data, wl_global_bind_func_t bind)
{
	return wl_global_create(display, interface, interface->version, data, bind);
}

void
wl_display_remove_global(struct wl_display *display,
			 struct wl_global *global) WL_DEPRECATED;

WL_EXPORT void
wl_display_remove_global(struct wl_display *display, struct wl_global *global)
{
	wl_global_destroy(global);
}

WL_EXPORT void
wl_display_add_shm_format(struct wl_display *display, uint32_t format)
{
	uint32_t *p;

	p = wl_array_add(&display->additional_shm_formats, sizeof *p);
	*p = format;
}

struct wl_array *
wl_display_get_additional_shm_formats(struct wl_display *display)
{
	return &display->additional_shm_formats;
}
