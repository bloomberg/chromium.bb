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
