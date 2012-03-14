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
#include "../src/wayland-server.h"
#include "test-runner.h"

static int
fd_dispatch(int fd, uint32_t mask, void *data)
{
	int *p = data;

	*p = 1;

	return 0;
}

TEST(post_dispatch_check)
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
