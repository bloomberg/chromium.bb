/*
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
#include <errno.h>
#include <string.h>
#include <sys/un.h>

#include "wayland-client.h"
#include "wayland-server.h"
#include "test-runner.h"

/* Paths longer than what the .sun_path array can contain must be rejected.
   This is a hard limitation of assigning a name to AF_UNIX/AF_LOCAL sockets.
   See `man 7 unix`. */

static const struct sockaddr_un example_sockaddr_un;

#define TOO_LONG (1 + sizeof example_sockaddr_un.sun_path)

/* Ensure the connection doesn't fail due to lack of XDG_RUNTIME_DIR. */
static void require_xdg_runtime_dir()
{
	char *val = getenv("XDG_RUNTIME_DIR");
	if (!val)
		assert(0 && "set $XDG_RUNTIME_DIR to run this test");
}

TEST(socket_path_overflow_client_connect)
{
	char path[TOO_LONG];
	struct wl_display *d;

	require_xdg_runtime_dir();

	memset(path, 'a', sizeof path);
	path[sizeof path - 1] = '\0';

	d = wl_display_connect(path);
	assert(d == NULL);
	assert(errno == ENAMETOOLONG);
}

TEST(socket_path_overflow_server_create)
{
	char path[TOO_LONG];
	struct wl_display *d;
	int ret;

	require_xdg_runtime_dir();

	memset(path, 'a', sizeof path);
	path[sizeof path - 1] = '\0';

	d = wl_display_create();
	assert(d != NULL);

	ret = wl_display_add_socket(d, path);
	assert(ret < 0);
	assert(errno == ENAMETOOLONG);

	wl_display_destroy(d);
}
