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

#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <signal.h>
#include "../src/wayland-server.h"
#include "test-runner.h"

static int
fd_dispatch(int fd, uint32_t mask, void *data)
{
	int *p = data;

	assert(mask == 0);
	*p = 1;

	return 0;
}

TEST(event_loop_post_dispatch_check)
{
	struct wl_event_loop *loop = wl_event_loop_create();
	struct wl_event_source *source;
	int dispatch_ran = 0;

	source = wl_event_loop_add_fd(loop, 1, WL_EVENT_READABLE,
				      fd_dispatch, &dispatch_ran);
	wl_event_source_check(source);

	wl_event_loop_dispatch(loop, 0);
	assert(dispatch_ran);

	wl_event_source_remove(source);
	wl_event_loop_destroy(loop);
}

struct free_source_context {
	struct wl_event_source *source1, *source2;
	int p1[2], p2[2];
	int count;
};

static int
free_source_callback(int fd, uint32_t mask, void *data)
{
	struct free_source_context *context = data;

	context->count++;

	/* Remove other source */
	if (fd == context->p1[0]) {
		wl_event_source_remove(context->source2);
		context->source2 = NULL;
	} else if (fd == context->p2[0]) {
		wl_event_source_remove(context->source1);
		context->source1 = NULL;
	} else {
		assert(0);
	}

	return 1;
}

TEST(event_loop_free_source_with_data)
{
	struct wl_event_loop *loop = wl_event_loop_create();
	struct free_source_context context;
	int data;

	/* This test is a little tricky to get right, since we don't
	 * have any guarantee from the event loop (ie epoll) on the
	 * order of which it reports events.  We want to have one
	 * source free the other, but we don't know which one is going
	 * to run first.  So we add two fd sources with a callback
	 * that frees the other source and check that only one of them
	 * run (and that we don't crash, of course).
	 */

	context.count = 0;
	assert(pipe(context.p1) == 0);
	assert(pipe(context.p2) == 0);
	context.source1 =
		wl_event_loop_add_fd(loop, context.p1[0], WL_EVENT_READABLE,
				     free_source_callback, &context);
	assert(context.source1);
	context.source2 =
		wl_event_loop_add_fd(loop, context.p2[0], WL_EVENT_READABLE,
				     free_source_callback, &context);
	assert(context.source2);

	data = 5;
	assert(write(context.p1[1], &data, sizeof data) == sizeof data);
	assert(write(context.p2[1], &data, sizeof data) == sizeof data);

	wl_event_loop_dispatch(loop, 0);

	assert(context.count == 1);

	if (context.source1)
		wl_event_source_remove(context.source1);
	if (context.source2)
		wl_event_source_remove(context.source2);
	wl_event_loop_destroy(loop);

	assert(close(context.p1[0]) == 0);
	assert(close(context.p1[1]) == 0);
	assert(close(context.p2[0]) == 0);
	assert(close(context.p2[1]) == 0);
}

static int
signal_callback(int signal_number, void *data)
{
	int *got_it = data;

	assert(signal_number == SIGUSR1);
	*got_it = 1;

	return 1;
}

TEST(event_loop_signal)
{
	struct wl_event_loop *loop = wl_event_loop_create();
	struct wl_event_source *source;
	int got_it = 0;

	source = wl_event_loop_add_signal(loop, SIGUSR1,
					  signal_callback, &got_it);
	wl_event_loop_dispatch(loop, 0);
	assert(!got_it);
	kill(getpid(), SIGUSR1);
	wl_event_loop_dispatch(loop, 0);
	assert(got_it);

	wl_event_source_remove(source);
	wl_event_loop_destroy(loop);
}


static int
timer_callback(void *data)
{
	int *got_it = data;

	*got_it = 1;

	return 1;
}

TEST(event_loop_timer)
{
	struct wl_event_loop *loop = wl_event_loop_create();
	struct wl_event_source *source;
	int got_it = 0;

	source = wl_event_loop_add_timer(loop, timer_callback, &got_it);
	wl_event_source_timer_update(source, 10);
	wl_event_loop_dispatch(loop, 0);
	assert(!got_it);
	wl_event_loop_dispatch(loop, 20);
	assert(got_it);

	wl_event_source_remove(source);
	wl_event_loop_destroy(loop);
}
