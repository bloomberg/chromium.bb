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

#define _GNU_SOURCE

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>
#include <assert.h>
#include <sys/socket.h>

#include "test-runner.h"

static void
test_client_cleanup(struct weston_process *proc, int status)
{
	struct test_client *client =
		container_of(proc, struct test_client, proc);

	fprintf(stderr, "test client exited, status %d\n", status);

	client->status = status;
	client->done = 1;

	assert(client->status == 0);

	if (client->terminate)
		wl_display_terminate(client->compositor->wl_display);
}

static int
test_client_data(int fd, uint32_t mask, void *data)
{
	struct test_client *client = data;
	struct wl_event_loop *loop;
	int len;

	len = read(client->fd, client->buf, sizeof client->buf);
	assert(len >= 0);
	fprintf(stderr, "got %.*s from client\n", len - 1, client->buf);
	assert(client->buf[len - 1] == '\n');
	client->buf[len - 1] = '\0';

	loop = wl_display_get_event_loop(client->compositor->wl_display);
	wl_event_loop_add_idle(loop, (void *) client->handle, client);

	return 1;
}

struct test_client *
test_client_launch(struct weston_compositor *compositor, const char *file_name)
{
	struct test_client *client;
	struct wl_event_loop *loop;
	int ret, sv[2], client_fd;
	char buf[256];

	client = malloc(sizeof *client);
	assert(client);
	ret = socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, sv);
	assert(ret == 0);

	client_fd = dup(sv[0]);
	assert(client_fd >= 0);
	snprintf(buf, sizeof buf, "%d", client_fd);
	setenv("TEST_SOCKET", buf, 1);
	snprintf(buf, sizeof buf, "%s/%s", getenv("abs_builddir"), file_name);
	fprintf(stderr, "launching %s\n", buf);

	client->terminate = 0;
	client->compositor = compositor;
	client->client = weston_client_launch(compositor,
					      &client->proc, buf,
					      test_client_cleanup);
	assert(client->client);
	close(sv[0]);
	client->fd = sv[1];

	loop = wl_display_get_event_loop(compositor->wl_display);
	wl_event_loop_add_fd(loop, client->fd, WL_EVENT_READABLE,
			     test_client_data, client);

	return client;
}

void
test_client_send(struct test_client *client, const char *fmt, ...)
{
	char buf[256];
	va_list ap;
	int len;

	va_start(ap, fmt);
	len = vsnprintf(buf, sizeof buf, fmt, ap);
	va_end(ap);

	assert(write(client->fd, buf, len) == len);
}

extern const struct test __start_test_section, __stop_test_section;

static void
run_test(void *data)
{
	struct weston_compositor *compositor = data;
	const struct test *t;

	for (t = &__start_test_section; t < &__stop_test_section; t++)
		t->run(compositor);
}

int
module_init(struct weston_compositor *compositor);

WL_EXPORT int
module_init(struct weston_compositor *compositor)
{
	struct wl_event_loop *loop;

	loop = wl_display_get_event_loop(compositor->wl_display);

	wl_event_loop_add_idle(loop, run_test, compositor);

	return 0;
}
