/*
 * Copyright © 2012 Intel Corporation
 *
 * Permission to use, copy, modify, distribute, and sell this software and
 * its documentation for any purpose is hereby granted without fee, provided
 * that the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of the copyright holders not be used in
 * advertising or publicity pertaining to distribution of the software
 * without specific, written prior permission.  The copyright holders make
 * no representations about the suitability of this software for any
 * purpose.  It is provided "as is" without express or implied warranty.
 *
 * THE COPYRIGHT HOLDERS DISCLAIM ALL WARRANTIES WITH REGARD TO THIS
 * SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS, IN NO EVENT SHALL THE COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER
 * RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF
 * CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#include "test-runner.h"

struct context {
	struct weston_process proc;
	struct weston_compositor *compositor;
	struct wl_listener client_destroy_listener;
	struct wl_listener compositor_destroy_listener;
	int status;
	int got_client_destroy;
	int done;
};

static void
cleanup(struct weston_process *proc, int status)
{
	struct context *context = container_of(proc, struct context, proc);

	fprintf(stderr, "child exited, status %d\n", status);

	wl_display_terminate(context->compositor->wl_display);
	context->status = status;
	context->done = 1;
}

static void
client_destroy(struct wl_listener *listener, void *data)
{
	struct context *context =
		container_of(listener, struct context,
			     client_destroy_listener);

	context->got_client_destroy = 1;

	fprintf(stderr, "notify child destroy\n");
}

static void
compositor_destroy(struct wl_listener *listener, void *data)
{
	struct context *context =
		container_of(listener, struct context,
			     compositor_destroy_listener);

	fprintf(stderr, "notify compositor destroy, status %d, done %d\n",
		context->status, context->done);

	assert(context->status == 0);
	assert(context->got_client_destroy);
	assert(context->done);
}

TEST(client_test)
{
	struct context *context;
	char path[256];
	struct wl_client *client;

	snprintf(path, sizeof path, "%s/test-client", getenv("abs_builddir"));
	fprintf(stderr, "launching %s\n", path);

	context = malloc(sizeof *context);
	assert(context);
	context->compositor = compositor;
	context->done = 0;
	context->got_client_destroy = 0;
	client = weston_client_launch(compositor,
				      &context->proc, path, cleanup);
	context->client_destroy_listener.notify = client_destroy;
	wl_client_add_destroy_listener(client,
				       &context->client_destroy_listener);

	context->compositor_destroy_listener.notify = compositor_destroy;
	wl_signal_add(&compositor->destroy_signal,
		      &context->compositor_destroy_listener);
}
