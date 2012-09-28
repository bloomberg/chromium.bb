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
#include <linux/input.h>

#include "test-runner.h"

struct context {
	struct weston_layer *layer;
	struct weston_seat *seat;
	struct weston_surface *surface;
};

static void
handle_button_up(struct test_client *);

static void
handle_button_down(struct test_client *client)
{
	struct context *context = client->data;
	uint32_t mask;

	assert(sscanf(client->buf, "%u", &mask) == 1);
	
	assert(mask == 1);

	notify_button(context->seat, 100, BTN_LEFT,
		      WL_POINTER_BUTTON_STATE_RELEASED);

	test_client_send(client, "send-button-state\n");
	client->handle = handle_button_up;
}

static void
handle_button_up(struct test_client *client)
{
	struct context *context = client->data;
	static int once = 0;
	uint32_t mask;

	assert(sscanf(client->buf, "%u", &mask) == 1);
	
	assert(mask == 0);

	if (!once++) {
		notify_button(context->seat, 100, BTN_LEFT,
			      WL_POINTER_BUTTON_STATE_PRESSED);

		test_client_send(client, "send-button-state\n");
		client->handle = handle_button_down;
	} else {
		test_client_send(client, "bye\n");
		client->handle = NULL;
	}
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

	context->layer = malloc(sizeof *context->layer);
	assert(context->layer);
	weston_layer_init(context->layer,
			  &client->compositor->cursor_layer.link);
	wl_list_insert(&context->layer->surface_list,
		       &context->surface->layer_link);

	seat_list = &client->compositor->seat_list;
	assert(wl_list_length(seat_list) == 1);
	context->seat = container_of(seat_list->next, struct weston_seat, link);

	client->compositor->focus = 1; /* Make it work even if pointer is
					* outside X window. */

	weston_surface_configure(context->surface, 100, 100, 100, 100);
	weston_surface_update_transform(context->surface);
	weston_surface_damage(context->surface);

	notify_pointer_focus(context->seat, context->surface->output,
			     wl_fixed_from_int(150), wl_fixed_from_int(150));

	test_client_send(client, "send-button-state\n");
	client->handle = handle_button_up;
}

TEST(button_test)
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
