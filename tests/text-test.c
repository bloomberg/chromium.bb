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

#include "config.h"

#include <string.h>
#include <stdio.h>
#include <linux/input.h>
#include "weston-test-client-helper.h"
#include "text-client-protocol.h"

struct text_input_state {
	int activated;
	int deactivated;
};

static void
text_input_commit_string(void *data,
			 struct wl_text_input *text_input,
			 uint32_t serial,
			 const char *text)
{
}

static void
text_input_preedit_string(void *data,
			  struct wl_text_input *text_input,
			  uint32_t serial,
			  const char *text,
			  const char *commit)
{
}

static void
text_input_delete_surrounding_text(void *data,
				   struct wl_text_input *text_input,
				   int32_t index,
				   uint32_t length)
{
}

static void
text_input_cursor_position(void *data,
			   struct wl_text_input *text_input,
			   int32_t index,
			   int32_t anchor)
{
}

static void
text_input_preedit_styling(void *data,
			   struct wl_text_input *text_input,
			   uint32_t index,
			   uint32_t length,
			   uint32_t style)
{
}

static void
text_input_preedit_cursor(void *data,
			  struct wl_text_input *text_input,
			  int32_t index)
{
}

static void
text_input_modifiers_map(void *data,
			 struct wl_text_input *text_input,
			 struct wl_array *map)
{
}

static void
text_input_keysym(void *data,
		  struct wl_text_input *text_input,
		  uint32_t serial,
		  uint32_t time,
		  uint32_t sym,
		  uint32_t state,
		  uint32_t modifiers)
{
}

static void
text_input_enter(void *data,
		 struct wl_text_input *text_input,
		 struct wl_surface *surface)

{
	struct text_input_state *state = data;

	fprintf(stderr, "%s\n", __FUNCTION__);

	state->activated += 1;
}

static void
text_input_leave(void *data,
		 struct wl_text_input *text_input)
{
	struct text_input_state *state = data;

	state->deactivated += 1;
}

static void
text_input_input_panel_state(void *data,
			     struct wl_text_input *text_input,
			     uint32_t state)
{
}

static void
text_input_language(void *data,
		    struct wl_text_input *text_input,
		    uint32_t serial,
		    const char *language)
{
}

static void
text_input_text_direction(void *data,
			  struct wl_text_input *text_input,
			  uint32_t serial,
			  uint32_t direction)
{
}

static const struct wl_text_input_listener text_input_listener = {
	text_input_enter,
	text_input_leave,
	text_input_modifiers_map,
	text_input_input_panel_state,
	text_input_preedit_string,
	text_input_preedit_styling,
	text_input_preedit_cursor,
	text_input_commit_string,
	text_input_cursor_position,
	text_input_delete_surrounding_text,
	text_input_keysym,
	text_input_language,
	text_input_text_direction
};

TEST(text_test)
{
	struct client *client;
	struct global *global;
	struct wl_text_input_manager *factory;
	struct wl_text_input *text_input;
	struct text_input_state state;

	client = client_create(100, 100, 100, 100);
	assert(client);

	factory = NULL;
	wl_list_for_each(global, &client->global_list, link) {
		if (strcmp(global->interface, "wl_text_input_manager") == 0)
			factory = wl_registry_bind(client->wl_registry,
						   global->name,
						   &wl_text_input_manager_interface, 1);
	}

	assert(factory);

	memset(&state, 0, sizeof state);
	text_input = wl_text_input_manager_create_text_input(factory);
	wl_text_input_add_listener(text_input, &text_input_listener, &state);

	/* Make sure our test surface has keyboard focus. */
	wl_test_activate_surface(client->test->wl_test,
				 client->surface->wl_surface);
	client_roundtrip(client);
	assert(client->input->keyboard->focus == client->surface);

	/* Activate test model and make sure we get enter event. */
	wl_text_input_activate(text_input, client->input->wl_seat,
			       client->surface->wl_surface);
	client_roundtrip(client);
	assert(state.activated == 1 && state.deactivated == 0);

	/* Deactivate test model and make sure we get leave event. */
	wl_text_input_deactivate(text_input, client->input->wl_seat);
	client_roundtrip(client);
	assert(state.activated == 1 && state.deactivated == 1);

	/* Activate test model again. */
	wl_text_input_activate(text_input, client->input->wl_seat,
			       client->surface->wl_surface);
	client_roundtrip(client);
	assert(state.activated == 2 && state.deactivated == 1);

	/* Take keyboard focus away and verify we get leave event. */
	wl_test_activate_surface(client->test->wl_test, NULL);
	client_roundtrip(client);
	assert(state.activated == 2 && state.deactivated == 2);
}
