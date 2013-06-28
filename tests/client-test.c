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
#include <sys/types.h>
#include <sys/stat.h>

#include "wayland-private.h"
#include "wayland-server.h"
#include "test-runner.h"

struct client_destroy_listener {
	struct wl_listener listener;
	int done;
};

static void
client_destroy_notify(struct wl_listener *l, void *data)
{
	struct client_destroy_listener *listener =
		container_of(l, struct client_destroy_listener, listener);

	listener->done = 1;
}

TEST(client_destroy_listener)
{
	struct wl_display *display;
	struct wl_client *client;
	struct client_destroy_listener a, b;
	int s[2];

	assert(socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, s) == 0);
	display = wl_display_create();
	assert(display);
	client = wl_client_create(display, s[0]);
	assert(client);

	a.listener.notify = client_destroy_notify;
	a.done = 0;
	wl_client_add_destroy_listener(client, &a.listener);

	assert(wl_client_get_destroy_listener(client, client_destroy_notify) ==
	       &a.listener);

	b.listener.notify = client_destroy_notify;
	b.done = 0;
	wl_client_add_destroy_listener(client, &b.listener);

	wl_list_remove(&a.listener.link);

	wl_client_destroy(client);

	assert(!a.done);
	assert(b.done);

	close(s[0]);
	close(s[1]);

	wl_display_destroy(display);
}

