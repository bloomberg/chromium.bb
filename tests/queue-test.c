/*
 * Copyright © 2012 Jonas Ådahl
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
#include <stdio.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <assert.h>

#include "wayland-client.h"
#include "wayland-server.h"
#include "test-runner.h"

#define SOCKET_NAME "wayland-queue-test"

#define ARRAY_LENGTH(a) (sizeof (a) / sizeof (a)[0])

#define client_assert(expr)					\
	do {							\
		if (!(expr)) {					\
			fprintf(stderr, "%s:%d: "		\
				"Assertion `%s' failed.\n",	\
				__FILE__, __LINE__, #expr);	\
			exit(EXIT_FAILURE);			\
		}						\
	} while (0)

struct display {
	struct wl_display *display;
	int child_exit_status;
};

static void
registry_handle_global(void *data, struct wl_registry *registry,
		       uint32_t id, const char *interface, uint32_t version)
{
	int *pcounter = data;
	(*pcounter)++;
	client_assert(*pcounter == 1);
	wl_registry_destroy(registry);
}

static const struct wl_registry_listener registry_listener = {
	registry_handle_global,
	NULL
};

/* Test that destroying a proxy object doesn't result in any more
 * callback being invoked, even though were many queued. */
static int
client_test_proxy_destroy(void)
{
	struct wl_display *display;
	struct wl_registry *registry;
	int counter = 0;

	display = wl_display_connect(SOCKET_NAME);
	client_assert(display);

	registry = wl_display_get_registry(display);
	wl_registry_add_listener(registry, &registry_listener,
				 &counter);
	wl_display_roundtrip(display);

	client_assert(counter == 1);

	wl_display_disconnect(display);

	return 0;
}

struct multiple_queues_state {
	struct wl_display *display;
	struct wl_callback* callback2;
	bool done;
};

static void
sync_callback(void *data, struct wl_callback *callback, uint32_t serial)
{
	struct multiple_queues_state *state = data;

	state->done = true;
	wl_callback_destroy(callback);

	wl_display_dispatch_pending(state->display);

	wl_callback_destroy(state->callback2);
}

static const struct wl_callback_listener sync_listener = {
	sync_callback
};

/* Test that when receiving the first of two synchronization
 * callback events, destroying the second one doesn't cause any
 * errors even if the delete_id event is handled out of order. */
static int
client_test_multiple_queues(void)
{
	struct wl_event_queue *queue;
	struct wl_callback *callback1;
	struct multiple_queues_state state;
	int ret = 0;

	state.display = wl_display_connect(SOCKET_NAME);
	client_assert(state.display);

	/* Make the current thread the display thread. This is because
	 * wl_display_dispatch_queue() will only read the display fd if
	 * the main display thread has been set. */
	wl_display_dispatch_pending(state.display);

	queue = wl_display_create_queue(state.display);
	client_assert(queue);

	state.done = false;
	callback1 = wl_display_sync(state.display);
	wl_callback_add_listener(callback1, &sync_listener, &state);
	wl_proxy_set_queue((struct wl_proxy *) callback1, queue);

	state.callback2 = wl_display_sync(state.display);
	wl_callback_add_listener(state.callback2, &sync_listener, NULL);
	wl_proxy_set_queue((struct wl_proxy *) state.callback2, queue);

	wl_display_flush(state.display);

	while (!state.done && !ret)
		ret = wl_display_dispatch_queue(state.display, queue);

	wl_display_disconnect(state.display);

	return ret == -1 ? -1 : 0;
}

static void
client_alarm_handler(int sig)
{
	fprintf(stderr, "Received SIGALRM signal, aborting.\n");
	exit(EXIT_FAILURE);
}

static void
client_sigsegv_handler(int sig)
{
	fprintf(stderr, "Received SIGSEGV signal, aborting.\n");
	exit(EXIT_FAILURE);
}

static int
client_main(int fd)
{
	bool cont = false;

	signal(SIGSEGV, client_sigsegv_handler);
	signal(SIGALRM, client_alarm_handler);
	alarm(2);

	if (read(fd, &cont, sizeof cont) != 1) {
		close(fd);
		return EXIT_FAILURE;
	}
	close(fd);

	if (!cont)
		return EXIT_FAILURE;

	if (client_test_proxy_destroy() != 0) {
		fprintf(stderr, "proxy destroy test failed\n");
		return EXIT_FAILURE;
	}

	if (client_test_multiple_queues() != 0) {
		fprintf(stderr, "multiple proxy test failed\n");
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}

static void
dummy_bind(struct wl_client *client,
	   void *data, uint32_t version, uint32_t id)
{
}

static int
sigchld_handler(int signal_number, void *data)
{
	struct display *display = data;
	int status;

	waitpid(-1, &status, 0);
	display->child_exit_status = status;

	wl_display_terminate(display->display);

	return 0;
}

static void
signal_client(int fd, bool cont)
{
	int ret;

	ret = write(fd, &cont, sizeof cont);
	close(fd);
	assert(ret == sizeof cont);
}

TEST(queue)
{
	struct display display;
	struct wl_event_loop *loop;
	struct wl_event_source *signal_source;
	const struct wl_interface *dummy_interfaces[] = {
		&wl_seat_interface,
		&wl_pointer_interface,
		&wl_keyboard_interface,
		&wl_surface_interface
	};
	unsigned int i;
	pid_t pid;
	int fds[2];
	int ret;

	ret = pipe(fds);
	assert(ret == 0);

	pid = fork();
	if (pid == -1) {
		perror("fork");
		exit(EXIT_FAILURE);
	} else if (pid == 0) {
		close(fds[1]);
		exit(client_main(fds[0]));
	}
	close(fds[0]);

	display.child_exit_status = EXIT_FAILURE;
	display.display = wl_display_create();
	if (!display.display) {
		signal_client(fds[1], false);
		abort();
	}

	for (i = 0; i < ARRAY_LENGTH(dummy_interfaces); i++)
		wl_display_add_global(display.display, dummy_interfaces[i],
				      NULL, dummy_bind);

	ret = wl_display_add_socket(display.display, SOCKET_NAME);
	assert(ret == 0);

	loop = wl_display_get_event_loop(display.display);
	signal_source = wl_event_loop_add_signal(loop, SIGCHLD, sigchld_handler,
						 &display);

	signal_client(fds[1], true);
	wl_display_run(display.display);

	wl_event_source_remove(signal_source);
	wl_display_destroy(display.display);

	assert(WIFEXITED(display.child_exit_status) &&
	       WEXITSTATUS(display.child_exit_status) == EXIT_SUCCESS);
}
