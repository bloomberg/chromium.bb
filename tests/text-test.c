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

#include <string.h>
#include <stdio.h>
#include <linux/input.h>
#include "weston-test-client-helper.h"
#include "../clients/text-client-protocol.h"

struct text_model_state {
	int activated;
	int deactivated;
};

static void
text_model_commit_string(void *data,
			 struct text_model *text_model,
			 uint32_t serial,
			 const char *text)
{
}

static void
text_model_preedit_string(void *data,
			  struct text_model *text_model,
			  uint32_t serial,
			  const char *text,
			  const char *commit)
{
}

static void
text_model_delete_surrounding_text(void *data,
				   struct text_model *text_model,
				   uint32_t serial,
				   int32_t index,
				   uint32_t length)
{
}

static void
text_model_cursor_position(void *data,
			   struct text_model *text_model,
			   uint32_t serial,
			   int32_t index,
			   int32_t anchor)
{
}

static void
text_model_preedit_styling(void *data,
			   struct text_model *text_model,
			   uint32_t serial,
			   uint32_t index,
			   uint32_t length,
			   uint32_t style)
{
}

static void
text_model_preedit_cursor(void *data,
			  struct text_model *text_model,
			  uint32_t serial,
			  int32_t index)
{
}

static void
text_model_modifiers_map(void *data,
			 struct text_model *text_model,
			 struct wl_array *map)
{
}

static void
text_model_keysym(void *data,
		  struct text_model *text_model,
		  uint32_t serial,
		  uint32_t time,
		  uint32_t sym,
		  uint32_t state,
		  uint32_t modifiers)
{
}

static void
text_model_enter(void *data,
		 struct text_model *text_model,
		 struct wl_surface *surface)

{
	struct text_model_state *state = data;

	fprintf(stderr, "%s\n", __FUNCTION__);

	state->activated += 1;
}

static void
text_model_leave(void *data,
		 struct text_model *text_model)
{
	struct text_model_state *state = data;

	state->deactivated += 1;
}

static void
text_model_input_panel_state(void *data,
			     struct text_model *text_model,
			     uint32_t state)
{
}

static void
text_model_language(void *data,
		    struct text_model *text_model,
		    uint32_t serial,
		    const char *language)
{
}

static void
text_model_text_direction(void *data,
			  struct text_model *text_model,
			  uint32_t serial,
			  uint32_t direction)
{
}

static const struct text_model_listener text_model_listener = {
	text_model_enter,
	text_model_leave,
	text_model_modifiers_map,
	text_model_input_panel_state,
	text_model_preedit_string,
	text_model_preedit_styling,
	text_model_preedit_cursor,
	text_model_commit_string,
	text_model_cursor_position,
	text_model_delete_surrounding_text,
	text_model_keysym,
	text_model_language,
	text_model_text_direction
};

TEST(text_test)
{
	struct client *client;
	struct global *global;
	struct text_model_factory *factory;
	struct text_model *text_model;
	struct text_model_state state;

	client = client_create(100, 100, 100, 100);
	assert(client);

	factory = NULL;
	wl_list_for_each(global, &client->global_list, link) {
		if (strcmp(global->interface, "text_model_factory") == 0)
			factory = wl_registry_bind(client->wl_registry,
						   global->name,
						   &text_model_factory_interface, 1);
	}

	assert(factory);

	memset(&state, 0, sizeof state);
	text_model = text_model_factory_create_text_model(factory);
	text_model_add_listener(text_model, &text_model_listener, &state);

	/* Make sure our test surface has keyboard focus. */
	wl_test_activate_surface(client->test->wl_test,
				 client->surface->wl_surface);
	client_roundtrip(client);
	assert(client->input->keyboard->focus == client->surface);

	/* Activate test model and make sure we get enter event. */
	text_model_activate(text_model, 0, client->input->wl_seat,
			    client->surface->wl_surface);
	client_roundtrip(client);
	assert(state.activated == 1 && state.deactivated == 0);

	/* Deactivate test model and make sure we get leave event. */
	text_model_deactivate(text_model, client->input->wl_seat);
	client_roundtrip(client);
	assert(state.activated == 1 && state.deactivated == 1);

	/* Activate test model again. */
	text_model_activate(text_model, 0, client->input->wl_seat,
			    client->surface->wl_surface);
	client_roundtrip(client);
	assert(state.activated == 2 && state.deactivated == 1);

	/* Take keyboard focus away and verify we get leave event. */
	wl_test_activate_surface(client->test->wl_test, NULL);
	client_roundtrip(client);
	assert(state.activated == 2 && state.deactivated == 2);
}
