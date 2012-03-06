/*
 * Copyright © 2012 Intel Corporation
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

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <assert.h>
#include <sys/socket.h>
#include <unistd.h>
#include <errno.h>

#include "../src/wayland-private.h"
#include "test-runner.h"

static const char message[] = "Hello, world";

static int
update_func(struct wl_connection *connection, uint32_t mask, void *data)
{
	uint32_t *m = data;

	*m = mask;

	return 0;
}

static struct wl_connection *
setup(int *s, uint32_t *mask)
{
	struct wl_connection *connection;

	assert(socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, s) == 0);

	connection = wl_connection_create(s[0], update_func, mask);
	assert(connection);
	assert(*mask == WL_CONNECTION_READABLE);

	return connection;
}

TEST(connection_create)
{
	struct wl_connection *connection;
	int s[2];
	uint32_t mask;

	connection = setup(s, &mask);
	wl_connection_destroy(connection);
	close(s[1]);
}

TEST(connection_write)
{
	struct wl_connection *connection;
	int s[2];
	uint32_t mask;
	char buffer[64];

	connection = setup(s, &mask);

	assert(wl_connection_write(connection, message, sizeof message) == 0);
	assert(mask == (WL_CONNECTION_WRITABLE | WL_CONNECTION_READABLE));
	assert(wl_connection_data(connection, WL_CONNECTION_WRITABLE) == 0);
	assert(mask == WL_CONNECTION_READABLE);
	assert(read(s[1], buffer, sizeof buffer) == sizeof message);
	assert(memcmp(message, buffer, sizeof message) == 0);

	wl_connection_destroy(connection);
	close(s[1]);
}

TEST(connection_data)
{
	struct wl_connection *connection;
	int s[2];
	uint32_t mask;
	char buffer[64];

	connection = setup(s, &mask);

	assert(write(s[1], message, sizeof message) == sizeof message);
	assert(mask == WL_CONNECTION_READABLE);
	assert(wl_connection_data(connection, WL_CONNECTION_READABLE) == 
	       sizeof message);
	wl_connection_copy(connection, buffer, sizeof message);
	assert(memcmp(message, buffer, sizeof message) == 0);
	wl_connection_consume(connection, sizeof message);

	wl_connection_destroy(connection);
	close(s[1]);
}

TEST(connection_queue)
{
	struct wl_connection *connection;
	int s[2];
	uint32_t mask;
	char buffer[64];

	connection = setup(s, &mask);

	/* Test that wl_connection_queue() puts data in the output
	 * buffer without asking for WL_CONNECTION_WRITABLE.  Verify
	 * that the data did get in the buffer by writing another
	 * message and making sure that we receive the two messages on
	 * the other fd. */

	assert(wl_connection_queue(connection, message, sizeof message) == 0);
	assert(mask == WL_CONNECTION_READABLE);
	assert(wl_connection_write(connection, message, sizeof message) == 0);
	assert(mask == (WL_CONNECTION_WRITABLE | WL_CONNECTION_READABLE));
	assert(wl_connection_data(connection, WL_CONNECTION_WRITABLE) == 0);
	assert(mask == WL_CONNECTION_READABLE);
	assert(read(s[1], buffer, sizeof buffer) == 2 * sizeof message);
	assert(memcmp(message, buffer, sizeof message) == 0);
	assert(memcmp(message, buffer + sizeof message, sizeof message) == 0);

	wl_connection_destroy(connection);
	close(s[1]);
}

struct marshal_data {
	struct wl_connection *connection;
	int s[2];
	uint32_t mask;
	uint32_t buffer[10];
};

static void
marshal(struct marshal_data *data, const char *format, int size, ...)
{
	struct wl_closure *closure;
	static const int opcode = 4444;
	static struct wl_object sender = { NULL, NULL, 1234 };
	struct wl_message message = { "test", format, NULL };
	va_list ap;

	va_start(ap, size);
	closure = wl_connection_vmarshal(data->connection,
					 &sender, opcode, ap, &message);
	va_end(ap);

	assert(closure);
	assert(wl_closure_send(closure, data->connection) == 0);
	wl_closure_destroy(closure);
	assert(data->mask ==
	       (WL_CONNECTION_WRITABLE | WL_CONNECTION_READABLE));
	assert(wl_connection_data(data->connection,
				  WL_CONNECTION_WRITABLE) == 0);
	assert(data->mask == WL_CONNECTION_READABLE);
	assert(read(data->s[1], data->buffer, sizeof data->buffer) == size);

	assert(data->buffer[0] == sender.id);
	assert(data->buffer[1] == (opcode | (size << 16)));
}

TEST(connection_marshal)
{
	struct marshal_data data;
	struct wl_object object;
	struct wl_array array;
	static const char text[] = "curry";

	data.connection = setup(data.s, &data.mask);

	marshal(&data, "i", 12, 42);
	assert(data.buffer[2] == 42);

	marshal(&data, "u", 12, 55);
	assert(data.buffer[2] == 55);

	marshal(&data, "s", 20, "frappo");
	assert(data.buffer[2] == 7);
	assert(strcmp((char *) &data.buffer[3], "frappo") == 0);

	object.id = 557799;
	marshal(&data, "o", 12, &object);
	assert(data.buffer[2] == object.id);

	marshal(&data, "o", 12, NULL);
	assert(data.buffer[2] == 0);

	marshal(&data, "n", 12, &object);
	assert(data.buffer[2] == object.id);

	marshal(&data, "n", 12, NULL);
	assert(data.buffer[2] == 0);

	array.data = (void *) text;
	array.size = sizeof text;
	marshal(&data, "a", 20, &array);
	assert(data.buffer[2] == array.size);
	assert(memcmp(&data.buffer[3], text, array.size) == 0);

	wl_connection_destroy(data.connection);
	close(data.s[1]);
}

static void
validate_demarshal_u(void *data, struct wl_object *object, uint32_t u)
{
	uint32_t *msg = data;

	assert(object->id = msg[0]);
	assert(msg[2] == u);
}

static void
validate_demarshal_i(void *data, struct wl_object *object, int32_t i)
{
	uint32_t *msg = data;

	assert(object->id = msg[0]);
	assert((int32_t) msg[2] == i);
}

static void
validate_demarshal_s(void *data, struct wl_object *object, const char *s)
{
	uint32_t *msg = data;

	assert(object->id = msg[0]);
	fprintf(stderr, "s = %s\n", s);
	assert(strcmp(s, "superdude") == 0);
}

static void
demarshal(struct marshal_data *data, const char *format,
	  uint32_t *msg, void (*func)(void))
{
	struct wl_message message = { "test", format, NULL };
	struct wl_closure *closure;
	struct wl_map objects;
	struct wl_object object;
	int size = msg[1];

	assert(write(data->s[1], msg, size) == size);
	assert(wl_connection_data(data->connection,
				  WL_CONNECTION_READABLE) == size);

	wl_map_init(&objects);
	object.id = msg[0];
	closure = wl_connection_demarshal(data->connection,
					  size, &objects, &message);
	wl_closure_invoke(closure, &object, func, msg);
}

TEST(connection_demarshal)
{
	struct marshal_data data;
	uint32_t msg[10];
	static const char text[] = "superdude";

	data.connection = setup(data.s, &data.mask);

	msg[0] = 400200;	/* object id */
	msg[1] = 12;		/* size = 12, opcode = 0 */
	msg[2] = 8000;
	demarshal(&data, "u", msg, (void *) validate_demarshal_u);

	msg[0] = 400200;
	msg[1] = 12;
	msg[2] = -557799;
	demarshal(&data, "i", msg, (void *) validate_demarshal_i);

	msg[0] = 400200;
	msg[1] = 24;
	msg[2] = 10;
	memcpy(&msg[3], text, msg[2]);
	demarshal(&data, "s", msg, (void *) validate_demarshal_s);

	wl_connection_destroy(data.connection);
	close(data.s[1]);
}
