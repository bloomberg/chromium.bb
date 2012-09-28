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

struct state {
	int px; /* pointer x */
	int py; /* pointer y */
	int sx; /* surface x */
	int sy; /* surface y */
	int sw; /* surface width */
	int sh; /* surface height */
};

static size_t state_size = sizeof(struct state);

struct context {
	struct weston_layer *layer;
	struct weston_seat *seat;
	struct weston_surface *surface;
	int pointer_x; /* server pointer x */
	int pointer_y; /* server pointer y */
	size_t index;
	struct wl_array states;
};

static void
resize(struct context *context, int w, int h)
{
	/* resize the surface only if the width or height is different */
	if (context->surface->geometry.width != w ||
	    context->surface->geometry.height != h) {

		weston_surface_configure(context->surface,
					 context->surface->geometry.x,
					 context->surface->geometry.y,
					 w, h);
		weston_surface_update_transform(context->surface);
		weston_surface_damage(context->surface);

		fprintf(stderr, "resize surface: %d %d\n",
			context->surface->geometry.width,
			context->surface->geometry.height);
	}
}

static void
move(struct context *context, int x, int y)
{
	/* move the surface only if x or y is different */
	if (context->surface->geometry.x != x ||
	    context->surface->geometry.y != y) {

		weston_surface_configure(context->surface,
					 x, y,
					 context->surface->geometry.width,
					 context->surface->geometry.height);
		weston_surface_update_transform(context->surface);
		weston_surface_damage(context->surface);

		fprintf(stderr, "move surface: %f %f\n",
			context->surface->geometry.x,
			context->surface->geometry.y);
	}
}

static int
contains(struct context *context, int x, int y)
{
	/* test whether a global x,y point is contained in the surface */
	int sx = context->surface->geometry.x;
	int sy = context->surface->geometry.y;
	int sw = context->surface->geometry.width;
	int sh = context->surface->geometry.height;
	return x >= sx && y >= sy && x < sx + sw && y < sy + sh;
}

static void
move_pointer(struct context *context, int x, int y)
{
	if (contains(context, context->pointer_x, context->pointer_y)) {
		/* pointer is currently on the surface */
		notify_motion(context->seat, 100,
			      wl_fixed_from_int(x), wl_fixed_from_int(y));
	} else {
		/* pointer is not currently on the surface */
		notify_pointer_focus(context->seat, context->surface->output,
				     wl_fixed_from_int(x),
				     wl_fixed_from_int(y));
	}

	/* update server expected pointer location */
	context->pointer_x = x;
	context->pointer_y = y;

	fprintf(stderr, "move pointer: %d %d\n", x, y);
}

static void
check_pointer(struct context *context, int cx, int cy)
{
	/*
	 * Check whether the client reported pointer position matches
	 * the server expected pointer position.  The client
	 * reports -1,-1 when the pointer is not on its surface and
	 * a surface relative x,y otherwise.
	 */
	int gx = context->surface->geometry.x + cx;
	int gy = context->surface->geometry.y + cy;
	if (!contains(context, gx, gy)) {
		assert(!contains(context, context->pointer_x,
				 context->pointer_y));
	} else {
		assert(gx == context->pointer_x);
		assert(gy == context->pointer_y);
	}
}

static void
check_visible(struct context *context, int visible)
{
	/*
	 * Check whether the client reported surface visibility matches
	 * the servers expected surface visibility
	 */
	int ow = context->surface->output->width;
	int oh = context->surface->output->height;
	int sx = context->surface->geometry.x;
	int sy = context->surface->geometry.y;
	int sw = context->surface->geometry.width;
	int sh = context->surface->geometry.height;

	const int expect = sx < ow && sy < oh && sx + sw > 0 && sy + sh > 0;

	assert(visible == expect);
}

static void
handle_state(struct test_client *);

static void
set_state(struct test_client *client)
{
	struct state* state;
	struct context *context = client->data;

	if (context->index < context->states.size) {
		state = context->states.data + context->index;
		resize(context, state->sw, state->sh);
		move(context, state->sx, state->sy);
		move_pointer(context, state->px, state->py);
		context->index += state_size;

		test_client_send(client, "send-state\n");
		client->handle = handle_state;
	} else {
		test_client_send(client, "bye\n");
		client->handle = NULL;
	}
}

static void
handle_state(struct test_client *client)
{
	struct context *context = client->data;
	wl_fixed_t x, y;
	int visible;

	assert(sscanf(client->buf, "%d %d %d", &x, &y, &visible) == 3);

	check_pointer(context, wl_fixed_to_int(x), wl_fixed_to_int(y));
	check_visible(context, visible);

	set_state(client);
}

static void
add_state(struct context *context, int px, int py, int sx, int sy,
	  int sw, int sh)
{
	struct state *state = wl_array_add(&context->states,
					   sizeof(struct state));

	assert(state);

	state->px = px;
	state->py = py;
	state->sx = sx;
	state->sy = sy;
	state->sw = sw;
	state->sh = sh;
}

static void
initialize_states(struct test_client *client)
{
	struct context *context = client->data;
	struct weston_surface *surface = context->surface;

	int x = surface->geometry.x;
	int y = surface->geometry.y;
	int w = surface->geometry.width;
	int h = surface->geometry.height;

	wl_array_init(&context->states);

	/* move pointer outside top left */
	add_state(context, x - 1, y - 1, x, y, w, h);
	/* move pointer on top left */
	add_state(context, x, y, x, y, w, h);
	/* move pointer outside bottom left */
	add_state(context, x - 1, y + h, x, y, w, h);
	/* move pointer on bottom left */
	add_state(context, x, y + h - 1, x, y, w, h);
	/* move pointer outside top right */
	add_state(context, x + w, y - 1, x, y, w, h);
	/* move pointer on top right */
	add_state(context, x + w - 1, y, x, y, w, h);
	/* move pointer outside bottom right */
	add_state(context, x + w, y + h, x, y, w, h);
	/* move pointer on bottom right */
	add_state(context, x + w - 1, y + h - 1, x, y, w, h);

	/* move pointer outside top center */
	add_state(context, x + w/2, y - 1, x, y, w, h);
	/* move pointer on top center */
	add_state(context, x + w/2, y, x, y, w, h);
	/* move pointer outside bottom center */
	add_state(context, x + w/2, y + h, x, y, w, h);
	/* move pointer on bottom center */
	add_state(context, x + w/2, y + h - 1, x, y, w, h);
	/* move pointer outside left center */
	add_state(context, x - 1, y + h/2, x, y, w, h);
	/* move pointer on left center */
	add_state(context, x, y + h/2, x, y, w, h);
	/* move pointer outside right center */
	add_state(context, x + w, y + h/2, x, y, w, h);
	/* move pointer on right center */
	add_state(context, x + w - 1, y + h/2, x, y, w, h);

	/* move pointer outside of client */
	add_state(context, 50, 50, x, y, w, h);
	/* move client center to pointer */
	add_state(context, 50, 50, 0, 0, w, h);

	/* not visible */
	add_state(context, 0, 0, 0, -h, w, h);
	/* visible */
	add_state(context, 0, 0, 0, -h+1, w, h);
	/* not visible */
	add_state(context, 0, 0, 0, context->surface->output->height, w, h);
	/* visible */
	add_state(context, 0, 0, 0, context->surface->output->height - 1, w, h);
	/* not visible */
	add_state(context, 0, 0, -w, 0, w, h);
	/* visible */
	add_state(context, 0, 0, -w+1, 0, w, h);
	/* not visible */
	add_state(context, 0, 0, context->surface->output->width, 0, w, h);
	/* visible */
	add_state(context, 0, 0, context->surface->output->width - 1, 0, w, h);

	set_state(client);
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

	resize(context, 100, 100);
	move(context, 100, 100);
	move_pointer(context, 150, 150);

	test_client_send(client, "send-state\n");
	client->handle = initialize_states;
}

TEST(event_test)
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
