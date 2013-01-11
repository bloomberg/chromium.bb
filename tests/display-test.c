/*
 * Copyright © 2012 Intel Corporation
 * Copyright © 2013 Jason Ekstrand
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

#include "wayland-server.h"
#include "wayland-private.h"
#include "test-runner.h"

struct display_destroy_listener {
	struct wl_listener listener;
	int done;
};

static void
display_destroy_notify(struct wl_listener *l, void *data)
{
	struct display_destroy_listener *listener;

	listener = container_of(l, struct display_destroy_listener, listener);
	listener->done = 1;
}

TEST(display_destroy_listener)
{
	struct wl_display *display;
	struct display_destroy_listener a, b;

	display = wl_display_create();
	assert(display);

	a.listener.notify = &display_destroy_notify;
	a.done = 0;
	wl_display_add_destroy_listener(display, &a.listener);

	assert(wl_display_get_destroy_listener(display, display_destroy_notify) ==
	       &a.listener);

	b.listener.notify = display_destroy_notify;
	b.done = 0;
	wl_display_add_destroy_listener(display, &b.listener);

	wl_list_remove(&a.listener.link);

	wl_display_destroy(display);

	assert(!a.done);
	assert(b.done);
}

