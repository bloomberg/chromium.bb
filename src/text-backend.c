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

	struct weston_compositor *ec;

	struct wl_list input_methods;

	struct wl_surface *surface;
};

struct input_method {
	struct wl_resource *input_method_binding;
	struct wl_global *input_method_global;
	struct wl_global *text_model_factory_global;
	struct wl_listener destroy_listener;

	struct weston_compositor *ec;
	struct text_model *model;

	struct wl_list link;

	struct wl_listener keyboard_focus_listener;

	int focus_listener_initialized;
};

static void input_method_init_seat(struct weston_seat *seat);

static void
deactivate_text_model(struct text_model *text_model,
		      struct input_method *input_method)
{
	struct weston_compositor *ec = text_model->ec;

	if (input_method->model == text_model) {
		wl_list_remove(&input_method->link);
		input_method->model = NULL;
		wl_signal_emit(&ec->hide_input_panel_signal, ec);
		text_model_send_deactivated(&text_model->resource);
	}
}

static void
destroy_text_model(struct wl_resource *resource)
{
	struct text_model *text_model =
		container_of(resource, struct text_model, resource);
	struct input_method *input_method, *next;

	wl_list_for_each_safe(input_method, next, &text_model->input_methods, link)
		deactivate_text_model(text_model, input_method);

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
	            struct wl_resource *resource,
		    struct wl_resource *seat,
		    struct wl_resource *surface)
{
	struct text_model *text_model = resource->data;
	struct weston_seat *weston_seat = seat->data;
	struct text_model *old = weston_seat->input_method->model;
	struct weston_compositor *ec = text_model->ec;

	if (old == text_model)
		return;

	if (old) {
		deactivate_text_model(old,
				      weston_seat->input_method);
	}

	weston_seat->input_method->model = text_model;
	wl_list_insert(&text_model->input_methods, &weston_seat->input_method->link);
	input_method_init_seat(weston_seat);

	text_model->surface = surface->data;

	wl_signal_emit(&ec->show_input_panel_signal, ec);

	text_model_send_activated(&text_model->resource);
}

static void
text_model_deactivate(struct wl_client *client,
		      struct wl_resource *resource,
		      struct wl_resource *seat)
{
	struct text_model *text_model = resource->data;
	struct weston_seat *weston_seat = seat->data;

	deactivate_text_model(text_model,
			      weston_seat->input_method);
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

static const struct text_model_interface text_model_implementation = {
	text_model_set_surrounding_text,
	text_model_set_cursor_index,
	text_model_activate,
	text_model_deactivate,
	text_model_set_selected_text,
	text_model_set_micro_focus,
	text_model_set_preedit,
	text_model_set_content_type
};

static void text_model_factory_create_text_model(struct wl_client *client,
						 struct wl_resource *resource,
						 uint32_t id,
						 struct wl_resource *surface)
{
	struct input_method *input_method = resource->data;
	struct text_model *text_model;

	text_model = calloc(1, sizeof *text_model);

	text_model->resource.destroy = destroy_text_model;

	text_model->resource.object.id = id;
	text_model->resource.object.interface = &text_model_interface;
	text_model->resource.object.implementation =
		(void (**)(void)) &text_model_implementation;
	text_model->resource.data = text_model;

	text_model->ec = input_method->ec;

	wl_client_add_resource(client, &text_model->resource);

	wl_list_init(&text_model->input_methods);
};

static const struct text_model_factory_interface text_model_factory_implementation = {
	text_model_factory_create_text_model
};

static void
bind_text_model_factory(struct wl_client *client,
			void *data,
			uint32_t version,
			uint32_t id)
{
	struct input_method *input_method = data;

	/* No checking for duplicate binding necessary.
	 * No events have to be sent, so we don't need the return value. */
	wl_client_add_object(client, &text_model_factory_interface,
			     &text_model_factory_implementation,
			     id, input_method);
}

static void
input_method_commit_string(struct wl_client *client,
			   struct wl_resource *resource,
			   const char *text,
			   uint32_t index)
{
	struct input_method *input_method = resource->data;

	if (input_method->model) {
		text_model_send_commit_string(&input_method->model->resource, text, index);
	}
}

static const struct input_method_interface input_method_implementation = {
	input_method_commit_string
};

static void
unbind_input_method(struct wl_resource *resource)
{
	struct input_method *input_method = resource->data;

	input_method->input_method_binding = NULL;
	free(resource);
}

static void
bind_input_method(struct wl_client *client,
		  void *data,
		  uint32_t version,
		  uint32_t id)
{
	struct input_method *input_method = data;
	struct wl_resource *resource;

	resource = wl_client_add_object(client, &input_method_interface,
					&input_method_implementation,
					id, input_method);

	if (input_method->input_method_binding == NULL) {
		resource->destroy = unbind_input_method;
		input_method->input_method_binding = resource;
		return;
	}

	wl_resource_post_error(resource, WL_DISPLAY_ERROR_INVALID_OBJECT,
			       "interface object already bound");
	wl_resource_destroy(resource);
}

static void
input_method_notifier_destroy(struct wl_listener *listener, void *data)
{
	struct input_method *input_method =
		container_of(listener, struct input_method, destroy_listener);

	wl_display_remove_global(input_method->ec->wl_display,
				 input_method->input_method_global);
	wl_display_remove_global(input_method->ec->wl_display,
				 input_method->text_model_factory_global);
	free(input_method);
}

static void
handle_keyboard_focus(struct wl_listener *listener, void *data)
{
	struct wl_keyboard *keyboard = data;
	struct input_method *input_method =
		container_of(listener, struct input_method, keyboard_focus_listener);
	struct wl_surface *surface = keyboard->focus;

	if (!input_method->model)
		return;

	if (!surface || input_method->model->surface != surface)
		deactivate_text_model(input_method->model,
				      input_method);
}

static void
input_method_init_seat(struct weston_seat *seat)
{
	if (seat->input_method->focus_listener_initialized)
		return;

	if (seat->has_keyboard) {
		seat->input_method->keyboard_focus_listener.notify = handle_keyboard_focus;
		wl_signal_add(&seat->seat.keyboard->focus_signal, &seat->input_method->keyboard_focus_listener);
	}

	seat->input_method->focus_listener_initialized = 1;
}

void
input_method_create(struct weston_compositor *ec,
		    struct weston_seat *seat)
{
	struct input_method *input_method;

	input_method = calloc(1, sizeof *input_method);

	input_method->ec = ec;
	input_method->model = NULL;
	input_method->focus_listener_initialized = 0;

	input_method->input_method_global =
		wl_display_add_global(ec->wl_display,
				      &input_method_interface,
				      input_method, bind_input_method);

	input_method->text_model_factory_global =
		wl_display_add_global(ec->wl_display,
				      &text_model_factory_interface,
				      input_method, bind_text_model_factory);

	input_method->destroy_listener.notify = input_method_notifier_destroy;
	wl_signal_add(&ec->destroy_signal, &input_method->destroy_listener);

	seat->input_method = input_method;
}

