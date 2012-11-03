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

static int
sigchld_handler(int signal_number, void *data)
{
	struct display *display = data;
	int status;

	waitpid(-1, &status, 0);
	display->child_exit_status = WEXITSTATUS(status);

	wl_display_terminate(display->display);

	return 0;
}

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

static void
client_alarm_handler(int sig)
{
	exit(EXIT_FAILURE);
}

static void
client_continue_handler(int sig)
{
}

static int
client_main(void)
{
	struct wl_display *display;
	struct wl_registry *registry;
	int counter = 0;

	signal(SIGALRM, client_alarm_handler);
	signal(SIGCONT, client_continue_handler);
	alarm(20);
	pause();

	display = wl_display_connect(SOCKET_NAME);
	client_assert(display);

	registry = wl_display_get_registry(display);
	wl_registry_add_listener(registry, &registry_listener, &counter);
	wl_display_roundtrip(display);

	client_assert(counter == 1);

	wl_display_disconnect(display);

	return EXIT_SUCCESS;
}

static void
dummy_bind(struct wl_client *client,
	   void *data, uint32_t version, uint32_t id)
{
}

TEST(queue_destroy_proxy)
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
	int ret;

	pid = fork();
	if (pid == -1) {
		perror("fork");
		exit(EXIT_FAILURE);
	} else if (pid == 0) {
		exit(client_main());
	}

	display.child_exit_status = EXIT_FAILURE;
	display.display = wl_display_create();
	assert(display.display);

	for (i = 0; i < ARRAY_LENGTH(dummy_interfaces); i++)
		wl_display_add_global(display.display, dummy_interfaces[i],
				      NULL, dummy_bind);

	ret = wl_display_add_socket(display.display, SOCKET_NAME);
	assert(ret == 0);

	loop = wl_display_get_event_loop(display.display);
	signal_source = wl_event_loop_add_signal(loop, SIGCHLD, sigchld_handler,
						 &display);

	kill(pid, SIGCONT);
	wl_display_run(display.display);

	wl_event_source_remove(signal_source);
	wl_display_destroy(display.display);

	assert(display.child_exit_status == EXIT_SUCCESS);
}

