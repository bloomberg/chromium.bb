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
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include <wayland-client.h>

static void
handle_global(struct wl_display *display, uint32_t id,
	      const char *interface, uint32_t version, void *data)
{
}

int main(int argc, char *argv[])
{
	struct wl_display *display;
	char buf[256], *p;
	int ret, fd;

	display = wl_display_connect(NULL);
	assert(display);

	wl_display_add_global_listener(display, handle_global, display);
	wl_display_iterate(display, WL_DISPLAY_READABLE);
	wl_display_roundtrip(display);

	fd = 0;
	p = getenv("TEST_SOCKET");
	if (p)
		fd = strtol(p, NULL, 0);

	while (1) {
		ret = read(fd, buf, sizeof buf);
		if (ret == -1) {
			fprintf(stderr, "read error: fd %d, %m\n", fd);
			return -1;
		}

		fprintf(stderr, "test-client: got %.*s\n", ret - 1, buf, ret);

		if (strncmp(buf, "bye\n", ret) == 0) {
			return 0;
		} else {
			fprintf(stderr, "unknown command %.*s\n", ret, buf);
			return -1;
		}
	}

	assert(0);
}
