/*
 * Copyright © 2012 Intel Corporation
 * Copyright © 2013 Collabora, Ltd.
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

#include "config.h"

#include <unistd.h>
#include <sys/types.h>

#include "../shared/os-compatibility.h"
#include "weston-test-client-helper.h"

/* tests, that attempt to crash the compositor on purpose */

static struct wl_buffer *
create_bad_shm_buffer(struct client *client, int width, int height)
{
	struct wl_shm *shm = client->wl_shm;
	int stride = width * 4;
	int size = stride * height;
	struct wl_shm_pool *pool;
	struct wl_buffer *buffer;
	int fd;

	fd = os_create_anonymous_file(size);
	assert(fd >= 0);

	pool = wl_shm_create_pool(shm, fd, size);
	buffer = wl_shm_pool_create_buffer(pool, 0, width, height, stride,
					   WL_SHM_FORMAT_ARGB8888);
	wl_shm_pool_destroy(pool);

	/* Truncate the file to a small size, so that the compositor
	 * will access it out-of-bounds, and hit SIGBUS.
	 */
	assert(ftruncate(fd, 12) == 0);
	close(fd);

	return buffer;
}

FAIL_TEST(test_truncated_shm_file)
{
	struct client *client;
	struct wl_buffer *bad_buffer;
	struct wl_surface *surface;
	int frame;

	client = client_create(46, 76, 111, 134);
	assert(client);
	surface = client->surface->wl_surface;

	bad_buffer = create_bad_shm_buffer(client, 200, 200);

	wl_surface_attach(surface, bad_buffer, 0, 0);
	wl_surface_damage(surface, 0, 0, 200, 200);
	frame_callback_set(surface, &frame);
	wl_surface_commit(surface);
	frame_callback_wait(client, &frame);
}
