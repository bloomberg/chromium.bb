/*
 * Copyright (c) 2014 Red Hat, Inc.
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

#include <unistd.h>

#include "wayland-server.h"
#include "wayland-client.h"

/* info about a client on server side */
struct client_info {
	struct display *display;
	struct wl_client *wl_client;
	struct wl_listener destroy_listener;
	const char *name; /* for debugging */

	int pipe;
	pid_t pid;
	int exit_code;

	struct wl_list link;
	void *data; /* for arbitrary use */
};

struct display {
	struct wl_display *wl_display;

	struct wl_list clients;
	uint32_t clients_no;
	uint32_t clients_terminated_no;

	/* list of clients waiting for display_resumed event */
	struct wl_list waiting_for_resume;
	uint32_t wfr_num;
};

/* This is a helper structure for clients.
 * Instead of calling wl_display_connect() and all the other stuff,
 * client can use client_connect and it will return this structure
 * filled. */
struct client {
	struct wl_display *wl_display;
	struct test_compositor *tc;

	int display_stopped;
};

struct client *client_connect(void);
void client_disconnect(struct client *);
int stop_display(struct client *, int);

/**
 * Usual workflow:
 *
 *    d = display_create();
 *
 *    wl_global_create(d->wl_display, ...);
 *    ... other setups ...
 *
 *    client_create(d, client_main);
 *    client_create(d, client_main2);
 *
 *    display_run(d);
 *    display_destroy(d);
 */
struct display *display_create(void);
void display_destroy(struct display *d);
void display_run(struct display *d);

/* After n clients called stop_display(..., n), the display
 * is stopped and can process the code after display_run().
 * This function rerun the display again and send display_resumed
 * event to waiting clients, so the clients will stop waiting and continue */
void display_resume(struct display *d);

struct client_info *client_create_with_name(struct display *d,
					    void (*client_main)(void),
					    const char *name);
#define client_create(d, c) client_create_with_name((d), (c), (#c))
