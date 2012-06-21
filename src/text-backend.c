/*
 * Copyright © 2012 Openismus GmbH
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

#include "compositor.h"
#include "text-server-protocol.h"

struct input_method;

struct text_model {
	struct wl_resource resource;

	struct wl_list link;

	struct input_method *input_method;
};

struct input_method {
	struct wl_object base;
	struct weston_compositor *ec;
	struct wl_global *global;
	struct wl_listener destroy_listener;
	struct wl_list models;

	struct text_model *active_model;
};

static void
destroy_text_model(struct wl_resource *resource)
{
	struct text_model *text_model = container_of(resource,
						     struct text_model, resource);

	if (text_model->input_method->active_model == text_model) {
		text_model->input_method->active_model = 0;
	}

	wl_list_remove(&text_model->link);
	free(text_model);
}

static void
text_model_set_surrounding_text(struct wl_client *client,
				struct wl_resource *resource,
				const char *text)
{
}

static void
text_model_set_cursor_index(struct wl_client *client,
			    struct wl_resource *resource,
			    uint32_t index)
{
}

static void
text_model_activate(struct wl_client *client,
	            struct wl_resource *resource)
{
	struct text_model *text_model = resource->data;

	text_model->input_method->active_model = text_model;

	wl_signal_emit(&text_model->input_method->ec->show_input_panel_signal, text_model->input_method->ec);
}

static void
text_model_deactivate(struct wl_client *client,
		      struct wl_resource *resource)
{
	struct text_model *text_model = resource->data;

	if (text_model->input_method->active_model == text_model) {
		text_model->input_method->active_model = NULL;
		wl_signal_emit(&text_model->input_method->ec->hide_input_panel_signal, text_model->input_method->ec);
	}
}

static void
text_model_set_selected_text(struct wl_client *client,
			     struct wl_resource *resource,
			     const char *text,
			     int32_t index)
{
}

static void
text_model_set_micro_focus(struct wl_client *client,
			   struct wl_resource *resource,
			   int32_t x,
			   int32_t y,
			   int32_t width,
			   int32_t height)
{
}

static void
text_model_set_preedit(struct wl_client *client,
		       struct wl_resource *resource)
{
}

static void
text_model_set_content_type(struct wl_client *client,
			    struct wl_resource *resource)
{
}

struct text_model_interface text_model_implementation = {
	text_model_set_surrounding_text,
	text_model_set_cursor_index,
	text_model_activate,
	text_model_deactivate,
	text_model_set_selected_text,
	text_model_set_micro_focus,
	text_model_set_preedit,
	text_model_set_content_type
};

static void input_method_create_text_model(struct wl_client *client,
					   struct wl_resource *resource,
					   uint32_t id,
					   struct wl_resource *surface)
{
	struct input_method *input_method = resource->data;
	struct text_model *text_model;

	text_model = malloc(sizeof *text_model);

	text_model->resource.destroy = destroy_text_model;
	
	text_model->resource.object.id = id;
	text_model->resource.object.interface = &text_model_interface;
	text_model->resource.object.implementation = 
		(void (**)(void)) &text_model_implementation;
	text_model->resource.data = text_model;

	text_model->input_method = input_method;

	wl_client_add_resource(client, &text_model->resource);

	wl_list_insert(&input_method->models, &text_model->link);
};

static void
input_method_commit_string(struct wl_client *client,
			   struct wl_resource *resource,
			   const char *text,
			   uint32_t index)
{
	struct input_method *input_method = resource->data;

	if (input_method->active_model) {
		text_model_send_commit_string(&input_method->active_model->resource, text, index);
	}
}

struct input_method_interface input_method_implementation = {
	input_method_create_text_model,
	input_method_commit_string
};

static void
bind_input_method(struct wl_client *client,
		  void *data,
		  uint32_t version,
		  uint32_t id)
{
	wl_client_add_object(client, &input_method_interface,
			     &input_method_implementation, id, data);
}

static void
input_method_notifier_destroy(struct wl_listener *listener, void *data)
{
	struct input_method *input_method = container_of(listener, struct input_method, destroy_listener);

	wl_display_remove_global(input_method->ec->wl_display, input_method->global);
	free(input_method);
}

void
input_method_create(struct weston_compositor *ec)
{
	struct input_method *input_method;

	input_method = malloc(sizeof *input_method);

	input_method->base.interface = &input_method_interface;
	input_method->base.implementation = (void(**)(void)) &input_method_implementation;
	input_method->ec = ec;
	input_method->active_model = NULL;

	wl_list_init(&input_method->models);

	input_method->global = wl_display_add_global(ec->wl_display,
						     &input_method_interface,
						     input_method, bind_input_method);

	input_method->destroy_listener.notify = input_method_notifier_destroy;
	wl_signal_add(&ec->destroy_signal, &input_method->destroy_listener); 
}
