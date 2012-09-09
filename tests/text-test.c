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

struct text_test_data {
	struct weston_layer *layer;

	unsigned int expected_activated_count;
	unsigned int expected_deactivated_count;

	const char *next_command;
	void (*next_handle)(struct test_client *client);
};

static void
pre_assert_state(struct test_client *client,
		 unsigned int expected_activated_count,
		 unsigned int expected_deactivated_count)
{
	unsigned int activated_count, deactivated_count;

	assert(sscanf(client->buf, "activated %u deactivated %u", &activated_count, &deactivated_count) == 2);
	fprintf(stderr, "Text model activations: %u deactivations: %u\n", activated_count, deactivated_count);
	assert(activated_count == expected_activated_count);
	assert(deactivated_count == expected_deactivated_count);
}

static void
handle_assert_state(struct test_client *client)
{
	struct text_test_data *data = client->data;

	pre_assert_state(client, data->expected_activated_count, data->expected_deactivated_count);

	test_client_send(client, data->next_command);
	client->handle = data->next_handle;
}

static void
post_assert_state(struct test_client *client,
		  unsigned int expected_activated_count,
		  unsigned int expected_deactivated_count,
		  const char *next_command,
		  void (*next_handle)(struct test_client *client))
{
	struct text_test_data *data = client->data;

	data->expected_activated_count = expected_activated_count;
	data->expected_deactivated_count = expected_deactivated_count;

	data->next_command = next_command;
	data->next_handle = next_handle;

	test_client_send(client, "assert-state\n");
	client->handle = handle_assert_state;
}

static struct weston_seat*
get_seat(struct test_client *client)
{
	struct wl_list *seat_list;
	struct weston_seat *seat;

	seat_list = &client->compositor->seat_list;
	assert(wl_list_length(seat_list) == 1);
	seat = container_of(seat_list->next, struct weston_seat, link);

	return seat;
}

static void
handle_surface_unfocus(struct test_client *client)
{
	struct weston_seat *seat;

	seat = get_seat(client);

	pre_assert_state(client, 2, 1);

	/* Unfocus the surface */
	wl_keyboard_set_focus(&seat->keyboard, NULL);

	post_assert_state(client, 2, 2, "bye\n", NULL);
}

static void
handle_reactivate_text_model(struct test_client *client)
{
	pre_assert_state(client, 1, 1);

	/* text_model is activated */

	post_assert_state(client, 2, 1,
			  "assert-state\n", handle_surface_unfocus);
}

static void
handle_deactivate_text_model(struct test_client *client)
{
	pre_assert_state(client, 1, 0);

	/* text_model is deactivated */

	post_assert_state(client, 1, 1,
			  "activate-text-model\n", handle_reactivate_text_model);
}

static void
handle_activate_text_model(struct test_client *client)
{
	pre_assert_state(client, 0, 0);

	/* text_model is activated */

	post_assert_state(client, 1, 0,
			  "deactivate-text-model\n", handle_deactivate_text_model);
}

static void
handle_text_model(struct test_client *client)
{
	uint32_t id;
	struct wl_resource *resource;

	assert(sscanf(client->buf, "text_model %u", &id) == 1);
	fprintf(stderr, "got text_model id %u\n", id);
	resource = wl_client_get_object(client->client, id);
	assert(resource);
	assert(strcmp(resource->object.interface->name, "text_model") == 0);

	test_client_send(client, "activate-text-model\n");
	client->handle = handle_activate_text_model;
}

static void
handle_surface(struct test_client *client)
{
	uint32_t id;
	struct wl_resource *resource;
	struct weston_surface *surface;
	struct text_test_data *data = client->data;
	struct weston_seat *seat;

	assert(sscanf(client->buf, "surface %u", &id) == 1);
	fprintf(stderr, "got surface id %u\n", id);
	resource = wl_client_get_object(client->client, id);
	assert(resource);
	assert(strcmp(resource->object.interface->name, "wl_surface") == 0);

	surface = (struct weston_surface *) resource;

	weston_surface_configure(surface, 100, 100, 200, 200);
	weston_surface_assign_output(surface);
	weston_surface_set_color(surface, 0.0, 0.0, 0.0, 1.0);

	data->layer = malloc(sizeof *data->layer);
	weston_layer_init(data->layer, &client->compositor->cursor_layer.link);
	wl_list_insert(&data->layer->surface_list, &surface->layer_link);
	weston_surface_damage(surface);

	seat = get_seat(client);
	client->compositor->focus = 1; /* Make it work even if pointer is
					* outside X window. */
	wl_keyboard_set_focus(&seat->keyboard, &surface->surface);

	test_client_send(client, "create-text-model\n");
	client->handle = handle_text_model;
}

TEST(text_test)
{
	struct test_client *client;
	struct text_test_data *data;

	client = test_client_launch(compositor, "test-text-client");
	client->terminate = 1;

	test_client_send(client, "create-surface\n");
	client->handle = handle_surface;

	data = malloc(sizeof *data);
	assert(data);
	client->data = data;

}
