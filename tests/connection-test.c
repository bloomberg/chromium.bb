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

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <assert.h>
#include <sys/socket.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>

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
	struct wl_connection *read_connection;
	struct wl_connection *write_connection;
	int s[2];
	uint32_t read_mask;
	uint32_t write_mask;
	uint32_t buffer[10];
	union {
		uint32_t u;
		int32_t i;
		const char *s;
		int h;
	} value;
};

static void
setup_marshal_data(struct marshal_data *data)
{
	assert(socketpair(AF_UNIX,
			  SOCK_STREAM | SOCK_CLOEXEC, 0, data->s) == 0);

	data->read_connection =
		wl_connection_create(data->s[0],
				     update_func, &data->read_mask);
	assert(data->read_connection);
	assert(data->read_mask == WL_CONNECTION_READABLE);

	data->write_connection =
		wl_connection_create(data->s[1],
				     update_func, &data->write_mask);
	assert(data->write_connection);
	assert(data->write_mask == WL_CONNECTION_READABLE);
}

static void
release_marshal_data(struct marshal_data *data)
{
	wl_connection_destroy(data->read_connection);
	wl_connection_destroy(data->write_connection);
}

static void
marshal(struct marshal_data *data, const char *format, int size, ...)
{
	struct wl_closure closure;
	static const uint32_t opcode = 4444;
	static struct wl_object sender = { NULL, NULL, 1234 };
	struct wl_message message = { "test", format, NULL };
	va_list ap;
	int ret;

	va_start(ap, size);
	ret = wl_closure_vmarshal(&closure, &sender, opcode, ap, &message);
	va_end(ap);

	assert(ret == 0);
	assert(wl_closure_send(&closure, data->write_connection) == 0);
	wl_closure_destroy(&closure);
	assert(data->write_mask ==
	       (WL_CONNECTION_WRITABLE | WL_CONNECTION_READABLE));
	assert(wl_connection_data(data->write_connection,
				  WL_CONNECTION_WRITABLE) == 0);
	assert(data->write_mask == WL_CONNECTION_READABLE);
	assert(read(data->s[0], data->buffer, sizeof data->buffer) == size);

	assert(data->buffer[0] == sender.id);
	assert(data->buffer[1] == (opcode | (size << 16)));
}

TEST(connection_marshal)
{
	struct marshal_data data;
	struct wl_object object;
	struct wl_array array;
	static const char text[] = "curry";

	setup_marshal_data(&data);

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

	release_marshal_data(&data);
}

static void
validate_demarshal_u(struct marshal_data *data,
		     struct wl_object *object, uint32_t u)
{
	assert(data->value.u == u);
}

static void
validate_demarshal_i(struct marshal_data *data,
		     struct wl_object *object, int32_t i)
{
	assert(data->value.i == i);
}

static void
validate_demarshal_s(struct marshal_data *data,
		     struct wl_object *object, const char *s)
{
	assert(strcmp(data->value.s, s) == 0);
}

static void
validate_demarshal_h(struct marshal_data *data,
		     struct wl_object *object, int fd)
{
	struct stat buf1, buf2;

	assert(fd != data->value.h);
	fstat(fd, &buf1);
	fstat(data->value.h, &buf2);
	assert(buf1.st_dev == buf2.st_dev);
	assert(buf1.st_ino == buf2.st_ino);
	close(fd);
	close(data->value.h);
}

static void
validate_demarshal_f(struct marshal_data *data,
		     struct wl_object *object, wl_fixed_t f)
{
	assert(data->value.i == f);
}

static void
demarshal(struct marshal_data *data, const char *format,
	  uint32_t *msg, void (*func)(void))
{
	struct wl_message message = { "test", format, NULL };
	struct wl_closure closure;
	struct wl_map objects;
	struct wl_object object;
	int size = msg[1], ret;

	assert(write(data->s[1], msg, size) == size);
	assert(wl_connection_data(data->read_connection,
				  WL_CONNECTION_READABLE) == size);

	wl_map_init(&objects);
	object.id = msg[0];
	ret = wl_connection_demarshal(data->read_connection,
				      &closure, size, &objects, &message);
	assert(ret == 0);
	wl_closure_invoke(&closure, &object, func, data);
}

TEST(connection_demarshal)
{
	struct marshal_data data;
	uint32_t msg[10];

	setup_marshal_data(&data);

	data.value.u = 8000;
	msg[0] = 400200;	/* object id */
	msg[1] = 12;		/* size = 12, opcode = 0 */
	msg[2] = data.value.u;
	demarshal(&data, "u", msg, (void *) validate_demarshal_u);

	data.value.i = -557799;
	msg[0] = 400200;
	msg[1] = 12;
	msg[2] = data.value.i;
	demarshal(&data, "i", msg, (void *) validate_demarshal_i);

	data.value.s = "superdude";
	msg[0] = 400200;
	msg[1] = 24;
	msg[2] = 10;
	memcpy(&msg[3], data.value.s, msg[2]);
	demarshal(&data, "s", msg, (void *) validate_demarshal_s);

	data.value.i = wl_fixed_from_double(-90000.2390);
	msg[0] = 400200;
	msg[1] = 12;
	msg[2] = data.value.i;
	demarshal(&data, "f", msg, (void *) validate_demarshal_f);

	release_marshal_data(&data);
}

static void
marshal_demarshal(struct marshal_data *data, 
		  void (*func)(void), int size, const char *format, ...)
{
	struct wl_closure closure;
	static const int opcode = 4444;
	static struct wl_object sender = { NULL, NULL, 1234 };
	struct wl_message message = { "test", format, NULL };
	struct wl_map objects;
	struct wl_object object;
	va_list ap;
	uint32_t msg[1] = { 1234 };
	int ret;

	va_start(ap, format);
	ret = wl_closure_vmarshal(&closure, &sender, opcode, ap, &message);
	va_end(ap);

	assert(ret == 0);
	assert(wl_closure_send(&closure, data->write_connection) == 0);
	wl_closure_destroy(&closure);
	assert(data->write_mask ==
	       (WL_CONNECTION_WRITABLE | WL_CONNECTION_READABLE));
	assert(wl_connection_data(data->write_connection,
				  WL_CONNECTION_WRITABLE) == 0);
	assert(data->write_mask == WL_CONNECTION_READABLE);

	assert(wl_connection_data(data->read_connection,
				  WL_CONNECTION_READABLE) == size);

	wl_map_init(&objects);
	object.id = msg[0];
	ret  = wl_connection_demarshal(data->read_connection,
				       &closure, size, &objects, &message);
	wl_closure_invoke(&closure, &object, func, data);
	wl_closure_destroy(&closure);
}

TEST(connection_marshal_demarshal)
{
	struct marshal_data data;
	char f[] = "/tmp/weston-tests-XXXXXX";

	setup_marshal_data(&data);

	data.value.u = 889911;
	marshal_demarshal(&data, (void *) validate_demarshal_u,
			  12, "u", data.value.u);

	data.value.i = -13;
	marshal_demarshal(&data, (void *) validate_demarshal_i,
			  12, "i", data.value.i);

	data.value.s = "cookie robots";
	marshal_demarshal(&data, (void *) validate_demarshal_s,
			  28, "s", data.value.s);

	data.value.h = mkstemp(f);
	assert(data.value.h >= 0);
	marshal_demarshal(&data, (void *) validate_demarshal_h,
			  8, "h", data.value.h);

	data.value.i = wl_fixed_from_double(1234.5678);
	marshal_demarshal(&data, (void *) validate_demarshal_f,
	                  12, "f", data.value.i);

	data.value.i = wl_fixed_from_double(-90000.2390);
	marshal_demarshal(&data, (void *) validate_demarshal_f,
	                  12, "f", data.value.i);

	data.value.i = wl_fixed_from_double((1 << 23) - 1 + 0.0941);
	marshal_demarshal(&data, (void *) validate_demarshal_f,
	                  12, "f", data.value.i);

	release_marshal_data(&data);
}

TEST(connection_marshal_alot)
{
	struct marshal_data data;
	char f[64];
	int i;

	setup_marshal_data(&data);
	
	/* We iterate enough to make sure we wrap the circular buffers
	 * for both regular data an fds. */

	for (i = 0; i < 2000; i++) {
		strcpy(f, "/tmp/weston-tests-XXXXXX");
		data.value.h = mkstemp(f);
		assert(data.value.h >= 0);
		marshal_demarshal(&data, (void *) validate_demarshal_h,
				  8, "h", data.value.h);
	}

	release_marshal_data(&data);
}

static void
marshal_helper(const char *format, void *handler, ...)
{
	struct wl_closure closure;
	static struct wl_object sender = { NULL, NULL, 1234 }, object;
	static const int opcode = 4444;
	struct wl_message message = { "test", format, NULL };
	va_list ap;
	int ret, done;

	va_start(ap, handler);
	ret = wl_closure_vmarshal(&closure, &sender, opcode, ap, &message);
	va_end(ap);

	assert(ret == 0);
	done = 0;
	wl_closure_invoke(&closure, &object, handler, &done);
	wl_closure_destroy(&closure);
	assert(done);
}

static void
suu_handler(void *data, struct wl_object *object,
	    const char *s, uint32_t u1, uint32_t u2)
{
	int *done = data;

	assert(strcmp(s, "foo") == 0);
	assert(u1 = 500);
	assert(u2 = 404040);
	*done = 1;
}

TEST(invoke_closure)
{
	marshal_helper("suu", suu_handler, "foo", 500, 404040);
}
