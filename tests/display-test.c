/*
 * Copyright © 2012 Intel Corporation
 * Copyright © 2013 Jason Ekstrand
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
#include <stdarg.h>
#include <string.h>
#include <assert.h>
#include <sys/socket.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "wayland-private.h"
#include "wayland-server.h"
#include "wayland-client.h"
#include "test-runner.h"
#include "test-compositor.h"

struct display_destroy_listener {
	struct wl_listener listener;
	int done;
};

static void
display_destroy_notify(struct wl_listener *l, void *data)
{
	struct display_destroy_listener *listener;

	listener = container_of(l, struct display_destroy_listener, listener);
	listener->done = 1;
}

TEST(display_destroy_listener)
{
	struct wl_display *display;
	struct display_destroy_listener a, b;

	display = wl_display_create();
	assert(display);

	a.listener.notify = &display_destroy_notify;
	a.done = 0;
	wl_display_add_destroy_listener(display, &a.listener);

	assert(wl_display_get_destroy_listener(display, display_destroy_notify) ==
	       &a.listener);

	b.listener.notify = display_destroy_notify;
	b.done = 0;
	wl_display_add_destroy_listener(display, &b.listener);

	wl_list_remove(&a.listener.link);

	wl_display_destroy(display);

	assert(!a.done);
	assert(b.done);
}

static void
registry_handle_globals(void *data, struct wl_registry *registry,
			uint32_t id, const char *intf, uint32_t ver)
{
	struct wl_seat **seat = data;

	if (strcmp(intf, "wl_seat") == 0) {
		*seat = wl_registry_bind(registry, id,
					 &wl_seat_interface, ver);
		assert(*seat);
	}
}

static const struct wl_registry_listener registry_listener = {
	registry_handle_globals,
	NULL
};

static struct wl_seat *
client_get_seat(struct client *c)
{
	struct wl_seat *seat;
	struct wl_registry *reg = wl_display_get_registry(c->wl_display);
	assert(reg);

	wl_registry_add_listener(reg, &registry_listener, &seat);
	wl_display_roundtrip(c->wl_display);
	assert(seat);

	wl_registry_destroy(reg);

	return seat;
}

static void
check_for_error(struct client *c, struct wl_proxy *proxy)
{
	uint32_t ec, id;
	int err;
	const struct wl_interface *intf;

	/* client should be disconnected */
	assert(wl_display_roundtrip(c->wl_display) == -1);

	err = wl_display_get_error(c->wl_display);
	assert(err == EPROTO);

	ec = wl_display_get_protocol_error(c->wl_display, &intf, &id);
	assert(ec == 23);
	assert(intf == &wl_seat_interface);
	assert(id == wl_proxy_get_id(proxy));
}

static void
bind_seat(struct wl_client *client, void *data,
	  uint32_t vers, uint32_t id)
{
	struct display *d = data;
	struct client_info *ci;
	struct wl_resource *res;

	/* find the right client_info struct and save the
	 * resource as its data, so that we can use it later */
	wl_list_for_each(ci, &d->clients, link) {
		if (ci->wl_client == client)
			break;
	}

	res = wl_resource_create(client, &wl_seat_interface, vers, id);
	assert(res);

	ci->data = res;
}

static void
post_error_main(void)
{
	struct client *c = client_connect();
	struct wl_seat *seat = client_get_seat(c);

	/* stop display so that it can post the error.
	 * The function should return -1, because of the posted error */
	assert(stop_display(c, 1) == -1);

	/* display should have posted error, check it! */
	check_for_error(c, (struct wl_proxy *) seat);

	/* don't call client_disconnect(c), because then the test would be
	 * aborted due to checks for error in this function */
	wl_proxy_destroy((struct wl_proxy *) seat);
	wl_proxy_destroy((struct wl_proxy *) c->tc);
	wl_display_disconnect(c->wl_display);
}

TEST(post_error_to_one_client)
{
	struct display *d = display_create();
	struct client_info *cl;

	wl_global_create(d->wl_display, &wl_seat_interface,
			 1, d, bind_seat);

	cl = client_create(d, post_error_main);
	display_run(d);

	/* the display was stopped by client, so it can
	 * proceed in the code and post an error */
	assert(cl->data);
	wl_resource_post_error((struct wl_resource *) cl->data,
			       23, "Dummy error");

	/* this one should be ignored */
	wl_resource_post_error((struct wl_resource *) cl->data,
			       21, "Dummy error (ignore)");

	display_resume(d);
	display_destroy(d);
}

static void
post_error_main2(void)
{
	struct client *c = client_connect();
	struct wl_seat *seat = client_get_seat(c);

	/* the error should not be posted for this client */
	assert(stop_display(c, 2) >= 0);

	wl_proxy_destroy((struct wl_proxy *) seat);
	client_disconnect(c);
}

static void
post_error_main3(void)
{
	struct client *c = client_connect();
	struct wl_seat *seat = client_get_seat(c);

	assert(stop_display(c, 2) == -1);
	check_for_error(c, (struct wl_proxy *) seat);

	/* don't call client_disconnect(c), because then the test would be
	 * aborted due to checks for error in this function */
	wl_proxy_destroy((struct wl_proxy *) seat);
	wl_proxy_destroy((struct wl_proxy *) c->tc);
	wl_display_disconnect(c->wl_display);
}

/* all the testcases could be in one TEST, but splitting it
 * apart is better for debugging when the test fails */
TEST(post_error_to_one_from_two_clients)
{
	struct display *d = display_create();
	struct client_info *cl;

	wl_global_create(d->wl_display, &wl_seat_interface,
			 1, d, bind_seat);

	client_create(d, post_error_main2);
	cl = client_create(d, post_error_main3);
	display_run(d);

	/* post error only to the second client */
	assert(cl->data);
	wl_resource_post_error((struct wl_resource *) cl->data,
			       23, "Dummy error");
	wl_resource_post_error((struct wl_resource *) cl->data,
			       21, "Dummy error (ignore)");

	display_resume(d);
	display_destroy(d);
}

/* all the testcases could be in one TEST, but splitting it
 * apart is better for debugging when the test fails */
TEST(post_error_to_two_clients)
{
	struct display *d = display_create();
	struct client_info *cl, *cl2;

	wl_global_create(d->wl_display, &wl_seat_interface,
			 1, d, bind_seat);

	cl = client_create(d, post_error_main3);
	cl2 = client_create(d, post_error_main3);

	display_run(d);

	/* Try to send the error to both clients */
	assert(cl->data && cl2->data);
	wl_resource_post_error((struct wl_resource *) cl->data,
			       23, "Dummy error");
	wl_resource_post_error((struct wl_resource *) cl->data,
			       21, "Dummy error (ignore)");

	wl_resource_post_error((struct wl_resource *) cl2->data,
			       23, "Dummy error");
	wl_resource_post_error((struct wl_resource *) cl2->data,
			       21, "Dummy error (ignore)");

	display_resume(d);
	display_destroy(d);
}

static void
post_nomem_main(void)
{
	struct client *c = client_connect();
	struct wl_seat *seat = client_get_seat(c);

	assert(stop_display(c, 1) == -1);
	assert(wl_display_get_error(c->wl_display) == ENOMEM);

	wl_proxy_destroy((struct wl_proxy *) seat);
	wl_proxy_destroy((struct wl_proxy *) c->tc);
	wl_display_disconnect(c->wl_display);
}

TEST(post_nomem_tst)
{
	struct display *d = display_create();
	struct client_info *cl;

	wl_global_create(d->wl_display, &wl_seat_interface,
			 1, d, bind_seat);

	cl = client_create(d, post_nomem_main);
	display_run(d);

	assert(cl->data);
	wl_resource_post_no_memory((struct wl_resource *) cl->data);
	display_resume(d);

	/* first client terminated. Run it again,
	 * but post no memory to client */
	cl = client_create(d, post_nomem_main);
	display_run(d);

	assert(cl->data);
	wl_client_post_no_memory(cl->wl_client);
	display_resume(d);

	display_destroy(d);
}
