/*
 * Copyright © 2012 Openismus GmbH
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

#include "compositor.h"
#include "text-server-protocol.h"
#include "input-method-server-protocol.h"

struct input_method;
struct input_method_context;

struct text_model {
	struct wl_resource resource;

	struct weston_compositor *ec;

	struct wl_list input_methods;

	struct wl_surface *surface;
};

struct text_model_factory {
	struct wl_global *text_model_factory_global;
	struct wl_listener destroy_listener;

	struct weston_compositor *ec;
};

struct input_method {
	struct wl_resource *input_method_binding;
	struct wl_global *input_method_global;
	struct wl_listener destroy_listener;

	struct weston_seat *seat;
	struct text_model *model;

	struct wl_list link;

	struct wl_listener keyboard_focus_listener;

	int focus_listener_initialized;

	struct input_method_context *context;
};

struct input_method_context {
	struct wl_resource resource;

	struct text_model *model;

	struct wl_list link;
};

static void input_method_context_create(struct text_model *model,
					struct input_method *input_method);

static void input_method_init_seat(struct weston_seat *seat);

static void
deactivate_text_model(struct text_model *text_model,
		      struct input_method *input_method)
{
	struct weston_compositor *ec = text_model->ec;

	if (input_method->model == text_model) {
		if (input_method->input_method_binding)
			input_method_send_deactivate(input_method->input_method_binding, &input_method->context->resource);
		wl_list_remove(&input_method->link);
		input_method->model = NULL;
		input_method->context = NULL;
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
				const char *text,
				uint32_t cursor,
				uint32_t anchor)
{
	struct text_model *text_model = resource->data;
	struct input_method *input_method, *next;

	wl_list_for_each_safe(input_method, next, &text_model->input_methods, link) {
		if (!input_method->context)
			continue;
		input_method_context_send_surrounding_text(&input_method->context->resource,
							   text,
							   cursor,
							   anchor);
	}
}

static void
text_model_activate(struct wl_client *client,
	            struct wl_resource *resource,
		    struct wl_resource *seat,
		    struct wl_resource *surface)
{
	struct text_model *text_model = resource->data;
	struct weston_seat *weston_seat = seat->data;
	struct input_method *input_method = weston_seat->input_method;
	struct text_model *old = weston_seat->input_method->model;
	struct weston_compositor *ec = text_model->ec;

	if (old == text_model)
		return;

	if (old) {
		deactivate_text_model(old,
				      weston_seat->input_method);
	}

	input_method->model = text_model;
	wl_list_insert(&text_model->input_methods, &input_method->link);
	input_method_init_seat(weston_seat);

	text_model->surface = surface->data;

	input_method_context_create(text_model, input_method);

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
text_model_reset(struct wl_client *client,
		 struct wl_resource *resource)
{
	struct text_model *text_model = resource->data;
	struct input_method *input_method, *next;

	wl_list_for_each_safe(input_method, next, &text_model->input_methods, link) {
		if (!input_method->context)
			continue;
		input_method_context_send_reset(&input_method->context->resource);
	}
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
	text_model_activate,
	text_model_deactivate,
	text_model_reset,
	text_model_set_micro_focus,
	text_model_set_preedit,
	text_model_set_content_type
};

static void text_model_factory_create_text_model(struct wl_client *client,
						 struct wl_resource *resource,
						 uint32_t id)
{
	struct text_model_factory *text_model_factory = resource->data;
	struct text_model *text_model;

	text_model = calloc(1, sizeof *text_model);

	text_model->resource.object.id = id;
	text_model->resource.object.interface = &text_model_interface;
	text_model->resource.object.implementation =
		(void (**)(void)) &text_model_implementation;

	text_model->resource.data = text_model;
	text_model->resource.destroy = destroy_text_model;

	text_model->ec = text_model_factory->ec;

	wl_list_init(&text_model->input_methods);

	wl_client_add_resource(client, &text_model->resource);
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
	struct text_model_factory *text_model_factory = data;

	/* No checking for duplicate binding necessary.
	 * No events have to be sent, so we don't need the return value. */
	wl_client_add_object(client, &text_model_factory_interface,
			     &text_model_factory_implementation,
			     id, text_model_factory);
}

static void
text_model_factory_notifier_destroy(struct wl_listener *listener, void *data)
{
	struct text_model_factory *text_model_factory =
		container_of(listener, struct text_model_factory, destroy_listener);

	wl_display_remove_global(text_model_factory->ec->wl_display,
				 text_model_factory->text_model_factory_global);

	free(text_model_factory);
}

WL_EXPORT void
text_model_factory_create(struct weston_compositor *ec)
{
	struct text_model_factory *text_model_factory;

	text_model_factory = calloc(1, sizeof *text_model_factory);

	text_model_factory->ec = ec;

	text_model_factory->text_model_factory_global =
		wl_display_add_global(ec->wl_display,
				      &text_model_factory_interface,
				      text_model_factory, bind_text_model_factory);

	text_model_factory->destroy_listener.notify = text_model_factory_notifier_destroy;
	wl_signal_add(&ec->destroy_signal, &text_model_factory->destroy_listener);
}

static void
input_method_context_destroy(struct wl_client *client,
			     struct wl_resource *resource)
{
	wl_resource_destroy(resource);
}

static void
input_method_context_commit_string(struct wl_client *client,
				   struct wl_resource *resource,
				   const char *text,
				   uint32_t index)
{
	struct input_method_context *context = resource->data;

	text_model_send_commit_string(&context->model->resource, text, index);
}

static void
input_method_context_preedit_string(struct wl_client *client,
				   struct wl_resource *resource,
				   const char *text,
				   uint32_t index)
{
	struct input_method_context *context = resource->data;

	text_model_send_preedit_string(&context->model->resource, text, index);
}

static void
input_method_context_delete_surrounding_text(struct wl_client *client,
					     struct wl_resource *resource,
					     int32_t index,
					     uint32_t length)
{
	struct input_method_context *context = resource->data;

	text_model_send_delete_surrounding_text(&context->model->resource, index, length);
}

static void
input_method_context_key(struct wl_client *client,
			 struct wl_resource *resource,
			 uint32_t key,
			 uint32_t state)
{
	struct input_method_context *context = resource->data;

	text_model_send_key(&context->model->resource, key, state);
}

static const struct input_method_context_interface input_method_context_implementation = {
	input_method_context_destroy,
	input_method_context_commit_string,
	input_method_context_preedit_string,
	input_method_context_delete_surrounding_text,
	input_method_context_key
};

static void
destroy_input_method_context(struct wl_resource *resource)
{
	struct input_method_context *context = resource->data;

	free(context);
}

static void
input_method_context_create(struct text_model *model,
			    struct input_method *input_method)
{
	struct input_method_context *context;

	if (!input_method->input_method_binding)
		return;

	context = malloc(sizeof *context);
	if (context == NULL)
		return;

	context->resource.destroy = destroy_input_method_context;
	context->resource.object.id = 0;
	context->resource.object.interface = &input_method_context_interface;
	context->resource.object.implementation =
		(void (**)(void)) &input_method_context_implementation;
	context->resource.data = context;
	wl_signal_init(&context->resource.destroy_signal);

	context->model = model;
	input_method->context = context;

	wl_client_add_resource(input_method->input_method_binding->client, &context->resource);

	input_method_send_activate(input_method->input_method_binding, &context->resource);
}

static void
unbind_input_method(struct wl_resource *resource)
{
	struct input_method *input_method = resource->data;

	input_method->input_method_binding = NULL;
	input_method->context = NULL;

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
					NULL,
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

	if (input_method->model)
		deactivate_text_model(input_method->model, input_method);

	wl_display_remove_global(input_method->seat->compositor->wl_display,
				 input_method->input_method_global);

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

WL_EXPORT void
input_method_create(struct weston_compositor *ec,
		    struct weston_seat *seat)
{
	struct input_method *input_method;

	input_method = calloc(1, sizeof *input_method);

	input_method->seat = seat;
	input_method->model = NULL;
	input_method->focus_listener_initialized = 0;
	input_method->context = NULL;

	input_method->input_method_global =
		wl_display_add_global(ec->wl_display,
				      &input_method_interface,
				      input_method, bind_input_method);

	input_method->destroy_listener.notify = input_method_notifier_destroy;
	wl_signal_add(&seat->seat.destroy_signal, &input_method->destroy_listener);

	seat->input_method = input_method;
}

