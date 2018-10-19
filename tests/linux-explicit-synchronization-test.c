/*
 * Copyright © 2018 Collabora, Ltd.
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

#include <string.h>
#include <unistd.h>

#include "linux-explicit-synchronization-unstable-v1-client-protocol.h"
#include "weston-test-client-helper.h"
#include "wayland-server-protocol.h"

static struct zwp_linux_explicit_synchronization_v1 *
get_linux_explicit_synchronization(struct client *client)
{
	struct global *g;
	struct global *global_sync = NULL;
	struct zwp_linux_explicit_synchronization_v1 *sync = NULL;

	wl_list_for_each(g, &client->global_list, link) {
		if (strcmp(g->interface,
			   zwp_linux_explicit_synchronization_v1_interface.name))
			continue;

		if (global_sync)
			assert(!"Multiple linux explicit sync objects");

		global_sync = g;
	}

	assert(global_sync);
	assert(global_sync->version == 1);

	sync = wl_registry_bind(
			client->wl_registry, global_sync->name,
			&zwp_linux_explicit_synchronization_v1_interface, 1);
	assert(sync);

	return sync;
}

static struct client *
create_test_client(void)
{
	struct client *cl = create_client_and_test_surface(0, 0, 100, 100);
	assert(cl);
	return cl;
}

TEST(second_surface_synchronization_on_surface_raises_error)
{
	struct client *client = create_test_client();
	struct zwp_linux_explicit_synchronization_v1 *sync =
		get_linux_explicit_synchronization(client);
	struct zwp_linux_surface_synchronization_v1 *surface_sync1;
	struct zwp_linux_surface_synchronization_v1 *surface_sync2;

	surface_sync1 =
		zwp_linux_explicit_synchronization_v1_get_synchronization(
			sync, client->surface->wl_surface);
	client_roundtrip(client);

	/* Second surface_synchronization creation should fail */
	surface_sync2 =
		zwp_linux_explicit_synchronization_v1_get_synchronization(
			sync, client->surface->wl_surface);
	expect_protocol_error(
		client,
		&zwp_linux_explicit_synchronization_v1_interface,
		ZWP_LINUX_EXPLICIT_SYNCHRONIZATION_V1_ERROR_SYNCHRONIZATION_EXISTS);

	zwp_linux_surface_synchronization_v1_destroy(surface_sync2);
	zwp_linux_surface_synchronization_v1_destroy(surface_sync1);
	zwp_linux_explicit_synchronization_v1_destroy(sync);
}

TEST(set_acquire_fence_with_invalid_fence_raises_error)
{
	struct client *client = create_test_client();
	struct zwp_linux_explicit_synchronization_v1 *sync =
		get_linux_explicit_synchronization(client);
	struct zwp_linux_surface_synchronization_v1 *surface_sync =
		zwp_linux_explicit_synchronization_v1_get_synchronization(
			sync, client->surface->wl_surface);
	int pipefd[2] = { -1, -1 };

	assert(pipe(pipefd) == 0);

	zwp_linux_surface_synchronization_v1_set_acquire_fence(surface_sync,
							       pipefd[0]);
	expect_protocol_error(
		client,
		&zwp_linux_surface_synchronization_v1_interface,
		ZWP_LINUX_SURFACE_SYNCHRONIZATION_V1_ERROR_INVALID_FENCE);

	close(pipefd[0]);
	close(pipefd[1]);
	zwp_linux_surface_synchronization_v1_destroy(surface_sync);
	zwp_linux_explicit_synchronization_v1_destroy(sync);
}

TEST(set_acquire_fence_on_destroyed_surface_raises_error)
{
	struct client *client = create_test_client();
	struct zwp_linux_explicit_synchronization_v1 *sync =
		get_linux_explicit_synchronization(client);
	struct zwp_linux_surface_synchronization_v1 *surface_sync =
		zwp_linux_explicit_synchronization_v1_get_synchronization(
			sync, client->surface->wl_surface);
	int pipefd[2] = { -1, -1 };

	assert(pipe(pipefd) == 0);

	wl_surface_destroy(client->surface->wl_surface);
	zwp_linux_surface_synchronization_v1_set_acquire_fence(surface_sync,
							       pipefd[0]);
	expect_protocol_error(
		client,
		&zwp_linux_surface_synchronization_v1_interface,
		ZWP_LINUX_SURFACE_SYNCHRONIZATION_V1_ERROR_NO_SURFACE);

	close(pipefd[0]);
	close(pipefd[1]);
	zwp_linux_surface_synchronization_v1_destroy(surface_sync);
	zwp_linux_explicit_synchronization_v1_destroy(sync);
}
