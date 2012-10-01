/*
 * Copyright © 2012 Intel Corporation
 *
 * Permission to use, copy, modify, distribute, and sell this software and
 * its documentation for any purpose is hereby granted without fee, provided
 * that the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of the copyright holders not be used in
 * advertising or publicity pertaining to distribution of the software
 * without specific, written prior permission.  The copyright holders make
 * no representations about the suitability of this software for any
 * purpose.  It is provided "as is" without express or implied warranty.
 *
 * THE COPYRIGHT HOLDERS DISCLAIM ALL WARRANTIES WITH REGARD TO THIS
 * SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS, IN NO EVENT SHALL THE COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER
 * RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF
 * CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <stdlib.h>
#include <stdio.h>
#include <sys/socket.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

#include "test-runner.h"

struct context {
	struct weston_seat *seat;
	struct weston_surface *surface;
	uint32_t expect_key;
	uint32_t expect_key_state;
	int expect_focus;
};

static void
handle_keyboard_state(struct test_client *client)
{
	struct context *context = client->data;
	uint32_t key, key_state;
	int focus;

	assert(sscanf(client->buf, "%u %u %d", &key, &key_state, &focus));
	
	assert(key == context->expect_key);
	assert(key_state == context->expect_key_state);
	assert(focus == context->expect_focus);

	if (key_state == WL_KEYBOARD_KEY_STATE_PRESSED) {
		context->expect_key_state = WL_KEYBOARD_KEY_STATE_RELEASED;
		notify_key(context->seat, 100, context->expect_key,
			   context->expect_key_state,
			   STATE_UPDATE_AUTOMATIC);
	} else if (focus) {
		context->expect_focus = 0;
		notify_keyboard_focus_out(context->seat);
	} else if (context->expect_key < 10) {
		context->expect_key++;
		context->expect_focus = 1;
		context->expect_key_state = WL_KEYBOARD_KEY_STATE_PRESSED;
		notify_keyboard_focus_in(context->seat,
					 &context->seat->keyboard.keys,
					 STATE_UPDATE_AUTOMATIC);
		notify_key(context->seat, 100, context->expect_key,
			   context->expect_key_state,
			   STATE_UPDATE_AUTOMATIC);
	} else {
		test_client_send(client, "bye\n");
		return;
	}
	
	test_client_send(client, "send-keyboard-state\n");
}

static void
handle_surface(struct test_client *client)
{
	uint32_t id;
	struct context *context = client->data;
	struct wl_resource *resource;
	struct wl_list *seat_list;

	assert(sscanf(client->buf, "surface %u", &id) == 1);
	fprintf(stderr, "server: got surface id %u\n", id);
	resource = wl_client_get_object(client->client, id);
	assert(resource);
	assert(strcmp(resource->object.interface->name, "wl_surface") == 0);

	context->surface = (struct weston_surface *) resource;
	weston_surface_set_color(context->surface, 0.0, 0.0, 0.0, 1.0);
	weston_surface_configure(context->surface, 100, 100, 100, 100);
	weston_surface_update_transform(context->surface);
	weston_surface_damage(context->surface);

	seat_list = &client->compositor->seat_list;
	assert(wl_list_length(seat_list) == 1);
	context->seat = container_of(seat_list->next, struct weston_seat, link);

	context->seat->keyboard.focus = context->surface;
	notify_keyboard_focus_out(context->seat);

	test_client_send(client, "send-keyboard-state\n");
	client->handle = handle_keyboard_state;
}

TEST(keyboard_test)
{
	struct context *context;
	struct test_client *client;

	client = test_client_launch(compositor, "test-client");
	client->terminate = 1;

	test_client_send(client, "create-surface\n");
	client->handle = handle_surface;

	context = calloc(1, sizeof *context);
	assert(context);
	client->data = context;
}
