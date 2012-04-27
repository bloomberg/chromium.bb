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
#include <stdarg.h>
#include <stdio.h>
#include <assert.h>
#include <unistd.h>

#include "test-runner.h"


struct context {
	struct weston_compositor *compositor;
	struct test_client *client;
	struct wl_listener compositor_destroy_listener;
	int status;
	int done;
};

static void
compositor_destroy(struct wl_listener *listener, void *data)
{
	struct context *context =
		container_of(listener, struct context,
			     compositor_destroy_listener);

	fprintf(stderr, "notify compositor destroy, status %d, done %d\n",
		context->client->status, context->client->done);

	assert(context->client->status == 0);
	assert(context->client->done);
}


TEST(client_test)
{
	struct context *context;

	context = malloc(sizeof *context);
	assert(context);
	context->compositor = compositor;
	context->done = 0;
	context->client = test_client_launch(compositor);
	context->client->terminate = 1;

	context->compositor_destroy_listener.notify = compositor_destroy;
	wl_signal_add(&compositor->destroy_signal,
		      &context->compositor_destroy_listener);

	test_client_send(context->client, "bye\n");
}
