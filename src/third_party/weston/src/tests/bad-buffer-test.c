/*
 * Copyright © 2012 Intel Corporation
 * Copyright © 2013 Collabora, Ltd.
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial
 * portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT.  IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "config.h"

#include <unistd.h>

#include "shared/os-compatibility.h"
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

TEST(test_truncated_shm_file)
{
	struct client *client;
	struct wl_buffer *bad_buffer;
	struct wl_surface *surface;
	int frame;

	client = create_client_and_test_surface(46, 76, 111, 134);
	assert(client);
	surface = client->surface->wl_surface;

	bad_buffer = create_bad_shm_buffer(client, 200, 200);

	wl_surface_attach(surface, bad_buffer, 0, 0);
	wl_surface_damage(surface, 0, 0, 200, 200);
	frame_callback_set(surface, &frame);
	wl_surface_commit(surface);
	frame_callback_wait_nofail(client, &frame);

	expect_protocol_error(client, &wl_buffer_interface,
			      WL_SHM_ERROR_INVALID_FD);
}
