/*
 * Copyright © 2013 Marek Chalupa
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

#include <assert.h>
#include <sys/socket.h>
#include <unistd.h>

#include "wayland-server.h"
#include "test-runner.h"

TEST(create_resource_tst)
{
	struct wl_display *display;
	struct wl_client *client;
	struct wl_resource *res;
	struct wl_list *link;
	int s[2];
	uint32_t id;

	assert(socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, s) == 0);
	display = wl_display_create();
	assert(display);
	client = wl_client_create(display, s[0]);
	assert(client);

	res = wl_resource_create(client, &wl_display_interface, 4, 0);
	assert(res);

	/* setters/getters */
	assert(wl_resource_get_version(res) == 4);

	assert(client == wl_resource_get_client(res));
	id = wl_resource_get_id(res);
	assert(wl_client_get_object(client, id) == res);

	link = wl_resource_get_link(res);
	assert(link);
	assert(wl_resource_from_link(link) == res);

	wl_resource_set_user_data(res, (void *) 0xbee);
	assert(wl_resource_get_user_data(res) == (void *) 0xbee);

	wl_resource_destroy(res);
	wl_client_destroy(client);
	wl_display_destroy(display);
	close(s[1]);
}

static void
res_destroy_func(struct wl_resource *res)
{
	assert(res);

	_Bool *destr = wl_resource_get_user_data(res);
	*destr = 1;
}

static _Bool notify_called = 0;
static void
destroy_notify(struct wl_listener *l, void *data)
{
	assert(l && data);
	notify_called = 1;
}

TEST(destroy_res_tst)
{
	struct wl_display *display;
	struct wl_client *client;
	struct wl_resource *res;
	int s[2];
	unsigned id;
	struct wl_list *link;

	_Bool destroyed = 0;
	struct wl_listener destroy_listener = {
		.notify = &destroy_notify
	};

	assert(socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, s) == 0);
	display = wl_display_create();
	assert(display);
	client = wl_client_create(display, s[0]);
	assert(client);

	res = wl_resource_create(client, &wl_display_interface, 4, 0);
	assert(res);
	wl_resource_set_implementation(res, NULL, &destroyed, res_destroy_func);
	wl_resource_add_destroy_listener(res, &destroy_listener);

	id = wl_resource_get_id(res);
	link = wl_resource_get_link(res);
	assert(link);

	wl_resource_destroy(res);
	assert(destroyed);
	assert(notify_called); /* check if signal was emitted */
	assert(wl_client_get_object(client, id) == NULL);

	res = wl_resource_create(client, &wl_display_interface, 2, 0);
	assert(res);
	destroyed = 0;
	notify_called = 0;
	wl_resource_set_destructor(res, res_destroy_func);
	wl_resource_set_user_data(res, &destroyed);
	wl_resource_add_destroy_listener(res, &destroy_listener);
	/* client should destroy the resource upon its destruction */
	wl_client_destroy(client);
	assert(destroyed);
	assert(notify_called);

	wl_display_destroy(display);
	close(s[1]);
}

TEST(create_resource_with_same_id)
{
	struct wl_display *display;
	struct wl_client *client;
	struct wl_resource *res, *res2;
	int s[2];
	uint32_t id;

	assert(socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, s) == 0);
	display = wl_display_create();
	assert(display);
	client = wl_client_create(display, s[0]);
	assert(client);

	res = wl_resource_create(client, &wl_display_interface, 2, 0);
	assert(res);
	id = wl_resource_get_id(res);
	assert(wl_client_get_object(client, id) == res);

	/* this one should replace the old one */
	res2 = wl_resource_create(client, &wl_display_interface, 1, id);
	assert(res2 != NULL);
	assert(wl_client_get_object(client, id) == res2);

	wl_resource_destroy(res2);
	wl_resource_destroy(res);

	wl_client_destroy(client);
	wl_display_destroy(display);
	close(s[1]);
}
