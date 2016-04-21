/*
 * Copyright © 2013 Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial
 * portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT.  IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "config.h"

#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <sys/mman.h>
#include <assert.h>
#include <unistd.h>
#include <fcntl.h>
#include <limits.h>

#include "shared/helpers.h"
#include "shared/os-compatibility.h"
#include "compositor.h"

static void
empty_region(pixman_region32_t *region)
{
	pixman_region32_fini(region);
	pixman_region32_init(region);
}

static struct weston_pointer_client *
weston_pointer_client_create(struct wl_client *client)
{
	struct weston_pointer_client *pointer_client;

	pointer_client = zalloc(sizeof *pointer_client);
	if (!pointer_client)
		return NULL;

	pointer_client->client = client;
	wl_list_init(&pointer_client->pointer_resources);

	return pointer_client;
}

static void
weston_pointer_client_destroy(struct weston_pointer_client *pointer_client)
{
	free(pointer_client);
}

static bool
weston_pointer_client_is_empty(struct weston_pointer_client *pointer_client)
{
	return wl_list_empty(&pointer_client->pointer_resources);
}

static struct weston_pointer_client *
weston_pointer_get_pointer_client(struct weston_pointer *pointer,
				  struct wl_client *client)
{
	struct weston_pointer_client *pointer_client;

	wl_list_for_each(pointer_client, &pointer->pointer_clients, link) {
		if (pointer_client->client == client)
			return pointer_client;
	}

	return NULL;
}

static struct weston_pointer_client *
weston_pointer_ensure_pointer_client(struct weston_pointer *pointer,
				     struct wl_client *client)
{
	struct weston_pointer_client *pointer_client;

	pointer_client = weston_pointer_get_pointer_client(pointer, client);
	if (pointer_client)
		return pointer_client;

	pointer_client = weston_pointer_client_create(client);
	wl_list_insert(&pointer->pointer_clients, &pointer_client->link);

	if (pointer->focus &&
	    pointer->focus->surface->resource &&
	    wl_resource_get_client(pointer->focus->surface->resource) == client) {
		pointer->focus_client = pointer_client;
	}

	return pointer_client;
}

static void
weston_pointer_cleanup_pointer_client(struct weston_pointer *pointer,
				      struct weston_pointer_client *pointer_client)
{
	if (weston_pointer_client_is_empty(pointer_client)) {
		if (pointer->focus_client == pointer_client)
			pointer->focus_client = NULL;
		wl_list_remove(&pointer_client->link);
		weston_pointer_client_destroy(pointer_client);
	}
}

static void
unbind_pointer_client_resource(struct wl_resource *resource)
{
	struct weston_pointer *pointer = wl_resource_get_user_data(resource);
	struct wl_client *client = wl_resource_get_client(resource);
	struct weston_pointer_client *pointer_client;

	pointer_client = weston_pointer_get_pointer_client(pointer, client);
	assert(pointer_client);

	wl_list_remove(wl_resource_get_link(resource));
	weston_pointer_cleanup_pointer_client(pointer, pointer_client);
}

static void unbind_resource(struct wl_resource *resource)
{
	wl_list_remove(wl_resource_get_link(resource));
}

WL_EXPORT void
weston_seat_repick(struct weston_seat *seat)
{
	const struct weston_pointer *pointer = weston_seat_get_pointer(seat);

	if (!pointer)
		return;

	pointer->grab->interface->focus(pointer->grab);
}

static void
weston_compositor_idle_inhibit(struct weston_compositor *compositor)
{
	weston_compositor_wake(compositor);
	compositor->idle_inhibit++;
}

static void
weston_compositor_idle_release(struct weston_compositor *compositor)
{
	compositor->idle_inhibit--;
	weston_compositor_wake(compositor);
}

static void
pointer_focus_view_destroyed(struct wl_listener *listener, void *data)
{
	struct weston_pointer *pointer =
		container_of(listener, struct weston_pointer,
			     focus_view_listener);

	weston_pointer_clear_focus(pointer);
}

static void
pointer_focus_resource_destroyed(struct wl_listener *listener, void *data)
{
	struct weston_pointer *pointer =
		container_of(listener, struct weston_pointer,
			     focus_resource_listener);

	weston_pointer_clear_focus(pointer);
}

static void
keyboard_focus_resource_destroyed(struct wl_listener *listener, void *data)
{
	struct weston_keyboard *keyboard =
		container_of(listener, struct weston_keyboard,
			     focus_resource_listener);

	weston_keyboard_set_focus(keyboard, NULL);
}

static void
touch_focus_view_destroyed(struct wl_listener *listener, void *data)
{
	struct weston_touch *touch =
		container_of(listener, struct weston_touch,
			     focus_view_listener);

	weston_touch_set_focus(touch, NULL);
}

static void
touch_focus_resource_destroyed(struct wl_listener *listener, void *data)
{
	struct weston_touch *touch =
		container_of(listener, struct weston_touch,
			     focus_resource_listener);

	weston_touch_set_focus(touch, NULL);
}

static void
move_resources(struct wl_list *destination, struct wl_list *source)
{
	wl_list_insert_list(destination, source);
	wl_list_init(source);
}

static void
move_resources_for_client(struct wl_list *destination,
			  struct wl_list *source,
			  struct wl_client *client)
{
	struct wl_resource *resource, *tmp;
	wl_resource_for_each_safe(resource, tmp, source) {
		if (wl_resource_get_client(resource) == client) {
			wl_list_remove(wl_resource_get_link(resource));
			wl_list_insert(destination,
				       wl_resource_get_link(resource));
		}
	}
}

static void
default_grab_pointer_focus(struct weston_pointer_grab *grab)
{
	struct weston_pointer *pointer = grab->pointer;
	struct weston_view *view;
	wl_fixed_t sx, sy;

	if (pointer->button_count > 0)
		return;

	view = weston_compositor_pick_view(pointer->seat->compositor,
					   pointer->x, pointer->y,
					   &sx, &sy);

	if (pointer->focus != view || pointer->sx != sx || pointer->sy != sy)
		weston_pointer_set_focus(pointer, view, sx, sy);
}

static void
default_grab_pointer_motion(struct weston_pointer_grab *grab, uint32_t time,
			    struct weston_pointer_motion_event *event)
{
	struct weston_pointer *pointer = grab->pointer;
	struct wl_list *resource_list;
	struct wl_resource *resource;
	wl_fixed_t x, y;
	wl_fixed_t old_sx = pointer->sx;
	wl_fixed_t old_sy = pointer->sy;

	if (pointer->focus) {
		weston_pointer_motion_to_abs(pointer, event, &x, &y);
		weston_view_from_global_fixed(pointer->focus, x, y,
					      &pointer->sx, &pointer->sy);
	}

	weston_pointer_move(pointer, event);

	if (pointer->focus_client &&
	    (old_sx != pointer->sx || old_sy != pointer->sy)) {
		resource_list = &pointer->focus_client->pointer_resources;
		wl_resource_for_each(resource, resource_list) {
			wl_pointer_send_motion(resource, time,
					       pointer->sx, pointer->sy);
		}
	}
}

static void
default_grab_pointer_button(struct weston_pointer_grab *grab,
			    uint32_t time, uint32_t button, uint32_t state_w)
{
	struct weston_pointer *pointer = grab->pointer;
	struct weston_compositor *compositor = pointer->seat->compositor;
	struct weston_view *view;
	struct wl_resource *resource;
	uint32_t serial;
	enum wl_pointer_button_state state = state_w;
	struct wl_display *display = compositor->wl_display;
	wl_fixed_t sx, sy;
	struct wl_list *resource_list = NULL;

	if (pointer->focus_client)
		resource_list = &pointer->focus_client->pointer_resources;
	if (resource_list && !wl_list_empty(resource_list)) {
		resource_list = &pointer->focus_client->pointer_resources;
		serial = wl_display_next_serial(display);
		wl_resource_for_each(resource, resource_list)
			wl_pointer_send_button(resource,
					       serial,
					       time,
					       button,
					       state_w);
	}

	if (pointer->button_count == 0 &&
	    state == WL_POINTER_BUTTON_STATE_RELEASED) {
		view = weston_compositor_pick_view(compositor,
						   pointer->x, pointer->y,
						   &sx, &sy);

		weston_pointer_set_focus(pointer, view, sx, sy);
	}
}

/** Send wl_pointer.axis events to focused resources.
 *
 * \param pointer The pointer where the axis events originates from.
 * \param time The timestamp of the event
 * \param axis The axis enum value of the event
 * \param value The axis value of the event
 *
 * For every resource that is currently in focus, send a wl_pointer.axis event
 * with the passed parameters. The focused resources are the wl_pointer
 * resources of the client which currently has the surface with pointer focus.
 */
WL_EXPORT void
weston_pointer_send_axis(struct weston_pointer *pointer,
			 uint32_t time,
			 struct weston_pointer_axis_event *event)
{
	struct wl_resource *resource;
	struct wl_list *resource_list;

	if (!pointer->focus_client)
		return;

	resource_list = &pointer->focus_client->pointer_resources;
	wl_resource_for_each(resource, resource_list) {
		if (event->has_discrete &&
		    wl_resource_get_version(resource) >=
		    WL_POINTER_AXIS_DISCRETE_SINCE_VERSION)
			wl_pointer_send_axis_discrete(resource, event->axis,
						      event->discrete);

		if (event->value)
			wl_pointer_send_axis(resource, time,
					     event->axis,
					     wl_fixed_from_double(event->value));
		else if (wl_resource_get_version(resource) >=
			 WL_POINTER_AXIS_STOP_SINCE_VERSION)
			wl_pointer_send_axis_stop(resource, time,
						  event->axis);
	}
}

WL_EXPORT void
weston_pointer_send_axis_source(struct weston_pointer *pointer, uint32_t source)
{
	struct wl_resource *resource;
	struct wl_list *resource_list;

	if (!pointer->focus_client)
		return;

	resource_list = &pointer->focus_client->pointer_resources;
	wl_resource_for_each(resource, resource_list) {
		if (wl_resource_get_version(resource) >=
		    WL_POINTER_AXIS_SOURCE_SINCE_VERSION) {
			wl_pointer_send_axis_source(resource, source);
		}
	}
}

static void
pointer_send_frame(struct wl_resource *resource)
{
	if (wl_resource_get_version(resource) >=
	    WL_POINTER_FRAME_SINCE_VERSION) {
		wl_pointer_send_frame(resource);
	}
}

WL_EXPORT void
weston_pointer_send_frame(struct weston_pointer *pointer)
{
	struct wl_resource *resource;
	struct wl_list *resource_list;

	if (!pointer->focus_client)
		return;

	resource_list = &pointer->focus_client->pointer_resources;
	wl_resource_for_each(resource, resource_list)
		pointer_send_frame(resource);
}

static void
default_grab_pointer_axis(struct weston_pointer_grab *grab,
			  uint32_t time,
			  struct weston_pointer_axis_event *event)
{
	weston_pointer_send_axis(grab->pointer, time, event);
}

static void
default_grab_pointer_axis_source(struct weston_pointer_grab *grab,
				 uint32_t source)
{
	weston_pointer_send_axis_source(grab->pointer, source);
}

static void
default_grab_pointer_frame(struct weston_pointer_grab *grab)
{
	weston_pointer_send_frame(grab->pointer);
}

static void
default_grab_pointer_cancel(struct weston_pointer_grab *grab)
{
}

static const struct weston_pointer_grab_interface
				default_pointer_grab_interface = {
	default_grab_pointer_focus,
	default_grab_pointer_motion,
	default_grab_pointer_button,
	default_grab_pointer_axis,
	default_grab_pointer_axis_source,
	default_grab_pointer_frame,
	default_grab_pointer_cancel,
};

static void
default_grab_touch_down(struct weston_touch_grab *grab, uint32_t time,
			int touch_id, wl_fixed_t x, wl_fixed_t y)
{
	struct weston_touch *touch = grab->touch;
	struct wl_display *display = touch->seat->compositor->wl_display;
	uint32_t serial;
	struct wl_resource *resource;
	struct wl_list *resource_list;
	wl_fixed_t sx, sy;

	if (!touch->focus)
		return;

	weston_view_from_global_fixed(touch->focus, x, y, &sx, &sy);

	resource_list = &touch->focus_resource_list;

	if (!wl_list_empty(resource_list)) {
		serial = wl_display_next_serial(display);
		wl_resource_for_each(resource, resource_list)
				wl_touch_send_down(resource, serial, time,
						   touch->focus->surface->resource,
						   touch_id, sx, sy);
	}
}

static void
default_grab_touch_up(struct weston_touch_grab *grab,
		      uint32_t time, int touch_id)
{
	struct weston_touch *touch = grab->touch;
	struct wl_display *display = touch->seat->compositor->wl_display;
	uint32_t serial;
	struct wl_resource *resource;
	struct wl_list *resource_list;

	resource_list = &touch->focus_resource_list;

	if (!wl_list_empty(resource_list)) {
		serial = wl_display_next_serial(display);
		wl_resource_for_each(resource, resource_list)
			wl_touch_send_up(resource, serial, time, touch_id);
	}
}

static void
default_grab_touch_motion(struct weston_touch_grab *grab, uint32_t time,
			  int touch_id, wl_fixed_t x, wl_fixed_t y)
{
	struct weston_touch *touch = grab->touch;
	struct wl_resource *resource;
	struct wl_list *resource_list;
	wl_fixed_t sx, sy;

	weston_view_from_global_fixed(touch->focus, x, y, &sx, &sy);

	resource_list = &touch->focus_resource_list;

	wl_resource_for_each(resource, resource_list) {
		wl_touch_send_motion(resource, time,
				     touch_id, sx, sy);
	}
}

static void
default_grab_touch_frame(struct weston_touch_grab *grab)
{
	struct wl_resource *resource;

	wl_resource_for_each(resource, &grab->touch->focus_resource_list)
		wl_touch_send_frame(resource);
}

static void
default_grab_touch_cancel(struct weston_touch_grab *grab)
{
}

static const struct weston_touch_grab_interface default_touch_grab_interface = {
	default_grab_touch_down,
	default_grab_touch_up,
	default_grab_touch_motion,
	default_grab_touch_frame,
	default_grab_touch_cancel,
};

static void
default_grab_keyboard_key(struct weston_keyboard_grab *grab,
			  uint32_t time, uint32_t key, uint32_t state)
{
	struct weston_keyboard *keyboard = grab->keyboard;
	struct wl_resource *resource;
	struct wl_display *display = keyboard->seat->compositor->wl_display;
	uint32_t serial;
	struct wl_list *resource_list;

	resource_list = &keyboard->focus_resource_list;
	if (!wl_list_empty(resource_list)) {
		serial = wl_display_next_serial(display);
		wl_resource_for_each(resource, resource_list)
			wl_keyboard_send_key(resource,
					     serial,
					     time,
					     key,
					     state);
	}
}

static void
send_modifiers_to_resource(struct weston_keyboard *keyboard,
			   struct wl_resource *resource,
			   uint32_t serial)
{
	wl_keyboard_send_modifiers(resource,
				   serial,
				   keyboard->modifiers.mods_depressed,
				   keyboard->modifiers.mods_latched,
				   keyboard->modifiers.mods_locked,
				   keyboard->modifiers.group);
}

static void
send_modifiers_to_client_in_list(struct wl_client *client,
				 struct wl_list *list,
				 uint32_t serial,
				 struct weston_keyboard *keyboard)
{
	struct wl_resource *resource;

	wl_resource_for_each(resource, list) {
		if (wl_resource_get_client(resource) == client)
			send_modifiers_to_resource(keyboard,
						   resource,
						   serial);
	}
}

static struct weston_pointer_client *
find_pointer_client_for_surface(struct weston_pointer *pointer,
				struct weston_surface *surface)
{
	struct wl_client *client;

	if (!surface)
		return NULL;

	if (!surface->resource)
		return NULL;

	client = wl_resource_get_client(surface->resource);
	return weston_pointer_get_pointer_client(pointer, client);
}

static struct weston_pointer_client *
find_pointer_client_for_view(struct weston_pointer *pointer, struct weston_view *view)
{
	if (!view)
		return NULL;

	return find_pointer_client_for_surface(pointer, view->surface);
}

static struct wl_resource *
find_resource_for_surface(struct wl_list *list, struct weston_surface *surface)
{
	if (!surface)
		return NULL;

	if (!surface->resource)
		return NULL;

	return wl_resource_find_for_client(list, wl_resource_get_client(surface->resource));
}

static void
default_grab_keyboard_modifiers(struct weston_keyboard_grab *grab,
				uint32_t serial, uint32_t mods_depressed,
				uint32_t mods_latched,
				uint32_t mods_locked, uint32_t group)
{
	struct weston_keyboard *keyboard = grab->keyboard;
	struct weston_pointer *pointer =
		weston_seat_get_pointer(grab->keyboard->seat);
	struct wl_resource *resource;
	struct wl_list *resource_list;

	resource_list = &keyboard->focus_resource_list;

	wl_resource_for_each(resource, resource_list) {
		wl_keyboard_send_modifiers(resource, serial, mods_depressed,
					   mods_latched, mods_locked, group);
	}
	if (pointer && pointer->focus && pointer->focus->surface->resource &&
	    pointer->focus->surface != keyboard->focus) {
		struct wl_client *pointer_client =
			wl_resource_get_client(pointer->focus->surface->resource);
		send_modifiers_to_client_in_list(pointer_client,
						 &keyboard->resource_list,
						 serial,
						 keyboard);
	}
}

static void
default_grab_keyboard_cancel(struct weston_keyboard_grab *grab)
{
}

static const struct weston_keyboard_grab_interface
				default_keyboard_grab_interface = {
	default_grab_keyboard_key,
	default_grab_keyboard_modifiers,
	default_grab_keyboard_cancel,
};

static void
pointer_unmap_sprite(struct weston_pointer *pointer)
{
	struct weston_surface *surface = pointer->sprite->surface;

	if (weston_surface_is_mapped(surface))
		weston_surface_unmap(surface);

	wl_list_remove(&pointer->sprite_destroy_listener.link);
	surface->configure = NULL;
	surface->configure_private = NULL;
	weston_surface_set_label_func(surface, NULL);
	weston_view_destroy(pointer->sprite);
	pointer->sprite = NULL;
}

static void
pointer_handle_sprite_destroy(struct wl_listener *listener, void *data)
{
	struct weston_pointer *pointer =
		container_of(listener, struct weston_pointer,
			     sprite_destroy_listener);

	pointer->sprite = NULL;
}

static void
weston_pointer_reset_state(struct weston_pointer *pointer)
{
	pointer->button_count = 0;
}

static void
weston_pointer_handle_output_destroy(struct wl_listener *listener, void *data);

WL_EXPORT struct weston_pointer *
weston_pointer_create(struct weston_seat *seat)
{
	struct weston_pointer *pointer;

	pointer = zalloc(sizeof *pointer);
	if (pointer == NULL)
		return NULL;

	wl_list_init(&pointer->pointer_clients);
	weston_pointer_set_default_grab(pointer,
					seat->compositor->default_pointer_grab);
	wl_list_init(&pointer->focus_resource_listener.link);
	pointer->focus_resource_listener.notify = pointer_focus_resource_destroyed;
	pointer->default_grab.pointer = pointer;
	pointer->grab = &pointer->default_grab;
	wl_signal_init(&pointer->motion_signal);
	wl_signal_init(&pointer->focus_signal);
	wl_list_init(&pointer->focus_view_listener.link);

	pointer->sprite_destroy_listener.notify = pointer_handle_sprite_destroy;

	/* FIXME: Pick better co-ords. */
	pointer->x = wl_fixed_from_int(100);
	pointer->y = wl_fixed_from_int(100);

	pointer->output_destroy_listener.notify =
		weston_pointer_handle_output_destroy;
	wl_signal_add(&seat->compositor->output_destroyed_signal,
		      &pointer->output_destroy_listener);

	pointer->sx = wl_fixed_from_int(-1000000);
	pointer->sy = wl_fixed_from_int(-1000000);

	return pointer;
}

WL_EXPORT void
weston_pointer_destroy(struct weston_pointer *pointer)
{
	if (pointer->sprite)
		pointer_unmap_sprite(pointer);

	/* XXX: What about pointer->resource_list? */

	wl_list_remove(&pointer->focus_resource_listener.link);
	wl_list_remove(&pointer->focus_view_listener.link);
	wl_list_remove(&pointer->output_destroy_listener.link);
	free(pointer);
}

void
weston_pointer_set_default_grab(struct weston_pointer *pointer,
		const struct weston_pointer_grab_interface *interface)
{
	if (interface)
		pointer->default_grab.interface = interface;
	else
		pointer->default_grab.interface =
			&default_pointer_grab_interface;
}

WL_EXPORT struct weston_keyboard *
weston_keyboard_create(void)
{
	struct weston_keyboard *keyboard;

	keyboard = zalloc(sizeof *keyboard);
	if (keyboard == NULL)
	    return NULL;

	wl_list_init(&keyboard->resource_list);
	wl_list_init(&keyboard->focus_resource_list);
	wl_list_init(&keyboard->focus_resource_listener.link);
	keyboard->focus_resource_listener.notify = keyboard_focus_resource_destroyed;
	wl_array_init(&keyboard->keys);
	keyboard->default_grab.interface = &default_keyboard_grab_interface;
	keyboard->default_grab.keyboard = keyboard;
	keyboard->grab = &keyboard->default_grab;
	wl_signal_init(&keyboard->focus_signal);

	return keyboard;
}

static void
weston_xkb_info_destroy(struct weston_xkb_info *xkb_info);

WL_EXPORT void
weston_keyboard_destroy(struct weston_keyboard *keyboard)
{
	/* XXX: What about keyboard->resource_list? */

#ifdef ENABLE_XKBCOMMON
	if (keyboard->seat->compositor->use_xkbcommon) {
		xkb_state_unref(keyboard->xkb_state.state);
		if (keyboard->xkb_info)
			weston_xkb_info_destroy(keyboard->xkb_info);
		xkb_keymap_unref(keyboard->pending_keymap);
	}
#endif

	wl_array_release(&keyboard->keys);
	wl_list_remove(&keyboard->focus_resource_listener.link);
	free(keyboard);
}

static void
weston_touch_reset_state(struct weston_touch *touch)
{
	touch->num_tp = 0;
}

WL_EXPORT struct weston_touch *
weston_touch_create(void)
{
	struct weston_touch *touch;

	touch = zalloc(sizeof *touch);
	if (touch == NULL)
		return NULL;

	wl_list_init(&touch->resource_list);
	wl_list_init(&touch->focus_resource_list);
	wl_list_init(&touch->focus_view_listener.link);
	touch->focus_view_listener.notify = touch_focus_view_destroyed;
	wl_list_init(&touch->focus_resource_listener.link);
	touch->focus_resource_listener.notify = touch_focus_resource_destroyed;
	touch->default_grab.interface = &default_touch_grab_interface;
	touch->default_grab.touch = touch;
	touch->grab = &touch->default_grab;
	wl_signal_init(&touch->focus_signal);

	return touch;
}

WL_EXPORT void
weston_touch_destroy(struct weston_touch *touch)
{
	/* XXX: What about touch->resource_list? */

	wl_list_remove(&touch->focus_view_listener.link);
	wl_list_remove(&touch->focus_resource_listener.link);
	free(touch);
}

static void
seat_send_updated_caps(struct weston_seat *seat)
{
	enum wl_seat_capability caps = 0;
	struct wl_resource *resource;

	if (seat->pointer_device_count > 0)
		caps |= WL_SEAT_CAPABILITY_POINTER;
	if (seat->keyboard_device_count > 0)
		caps |= WL_SEAT_CAPABILITY_KEYBOARD;
	if (seat->touch_device_count > 0)
		caps |= WL_SEAT_CAPABILITY_TOUCH;

	wl_resource_for_each(resource, &seat->base_resource_list) {
		wl_seat_send_capabilities(resource, caps);
	}
	wl_signal_emit(&seat->updated_caps_signal, seat);
}


/** Clear the pointer focus
 *
 * \param pointer the pointer to clear focus for.
 *
 * This can be used to unset pointer focus and set the co-ordinates to the
 * arbitrary values we use for the no focus case.
 *
 * There's no requirement to use this function.  For example, passing the
 * results of a weston_compositor_pick_view() directly to
 * weston_pointer_set_focus() will do the right thing when no view is found.
 */
WL_EXPORT void
weston_pointer_clear_focus(struct weston_pointer *pointer)
{
	weston_pointer_set_focus(pointer, NULL,
				 wl_fixed_from_int(-1000000),
				 wl_fixed_from_int(-1000000));
}

WL_EXPORT void
weston_pointer_set_focus(struct weston_pointer *pointer,
			 struct weston_view *view,
			 wl_fixed_t sx, wl_fixed_t sy)
{
	struct weston_pointer_client *pointer_client;
	struct weston_keyboard *kbd = weston_seat_get_keyboard(pointer->seat);
	struct wl_resource *resource;
	struct wl_resource *surface_resource;
	struct wl_display *display = pointer->seat->compositor->wl_display;
	uint32_t serial;
	struct wl_list *focus_resource_list;
	int refocus = 0;

	if ((!pointer->focus && view) ||
	    (pointer->focus && !view) ||
	    (pointer->focus && pointer->focus->surface != view->surface) ||
	    pointer->sx != sx || pointer->sy != sy)
		refocus = 1;

	if (pointer->focus_client && refocus) {
		focus_resource_list = &pointer->focus_client->pointer_resources;
		if (!wl_list_empty(focus_resource_list)) {
			serial = wl_display_next_serial(display);
			surface_resource = pointer->focus->surface->resource;
			wl_resource_for_each(resource, focus_resource_list) {
				wl_pointer_send_leave(resource, serial,
						      surface_resource);
				pointer_send_frame(resource);
			}
		}

		pointer->focus_client = NULL;
	}

	pointer_client = find_pointer_client_for_view(pointer, view);
	if (pointer_client && refocus) {
		struct wl_client *surface_client = pointer_client->client;

		serial = wl_display_next_serial(display);

		if (kbd && kbd->focus != view->surface)
			send_modifiers_to_client_in_list(surface_client,
							 &kbd->resource_list,
							 serial,
							 kbd);

		pointer->focus_client = pointer_client;

		focus_resource_list = &pointer->focus_client->pointer_resources;
		wl_resource_for_each(resource, focus_resource_list) {
			wl_pointer_send_enter(resource,
					      serial,
					      view->surface->resource,
					      sx, sy);
			pointer_send_frame(resource);
		}

		pointer->focus_serial = serial;
	}

	wl_list_remove(&pointer->focus_view_listener.link);
	wl_list_init(&pointer->focus_view_listener.link);
	wl_list_remove(&pointer->focus_resource_listener.link);
	wl_list_init(&pointer->focus_resource_listener.link);
	if (view)
		wl_signal_add(&view->destroy_signal, &pointer->focus_view_listener);
	if (view && view->surface->resource)
		wl_resource_add_destroy_listener(view->surface->resource,
						 &pointer->focus_resource_listener);

	pointer->focus = view;
	pointer->focus_view_listener.notify = pointer_focus_view_destroyed;
	pointer->sx = sx;
	pointer->sy = sy;

	assert(view || sx == wl_fixed_from_int(-1000000));
	assert(view || sy == wl_fixed_from_int(-1000000));

	wl_signal_emit(&pointer->focus_signal, pointer);
}

static void
send_enter_to_resource_list(struct wl_list *list,
			    struct weston_keyboard *keyboard,
			    struct weston_surface *surface,
			    uint32_t serial)
{
	struct wl_resource *resource;

	wl_resource_for_each(resource, list) {
		send_modifiers_to_resource(keyboard, resource, serial);
		wl_keyboard_send_enter(resource, serial,
				       surface->resource,
				       &keyboard->keys);
	}
}

WL_EXPORT void
weston_keyboard_set_focus(struct weston_keyboard *keyboard,
			  struct weston_surface *surface)
{
	struct wl_resource *resource;
	struct wl_display *display = keyboard->seat->compositor->wl_display;
	uint32_t serial;
	struct wl_list *focus_resource_list;

	focus_resource_list = &keyboard->focus_resource_list;

	if (!wl_list_empty(focus_resource_list) && keyboard->focus != surface) {
		serial = wl_display_next_serial(display);
		wl_resource_for_each(resource, focus_resource_list) {
			wl_keyboard_send_leave(resource, serial,
					keyboard->focus->resource);
		}
		move_resources(&keyboard->resource_list, focus_resource_list);
	}

	if (find_resource_for_surface(&keyboard->resource_list, surface) &&
	    keyboard->focus != surface) {
		struct wl_client *surface_client =
			wl_resource_get_client(surface->resource);

		serial = wl_display_next_serial(display);

		move_resources_for_client(focus_resource_list,
					  &keyboard->resource_list,
					  surface_client);
		send_enter_to_resource_list(focus_resource_list,
					    keyboard,
					    surface,
					    serial);
		keyboard->focus_serial = serial;
	}

	wl_list_remove(&keyboard->focus_resource_listener.link);
	wl_list_init(&keyboard->focus_resource_listener.link);
	if (surface && surface->resource)
		wl_resource_add_destroy_listener(surface->resource,
						 &keyboard->focus_resource_listener);

	keyboard->focus = surface;
	wl_signal_emit(&keyboard->focus_signal, keyboard);
}

/* Users of this function must manually manage the keyboard focus */
WL_EXPORT void
weston_keyboard_start_grab(struct weston_keyboard *keyboard,
			   struct weston_keyboard_grab *grab)
{
	keyboard->grab = grab;
	grab->keyboard = keyboard;
}

WL_EXPORT void
weston_keyboard_end_grab(struct weston_keyboard *keyboard)
{
	keyboard->grab = &keyboard->default_grab;
}

static void
weston_keyboard_cancel_grab(struct weston_keyboard *keyboard)
{
	keyboard->grab->interface->cancel(keyboard->grab);
}

WL_EXPORT void
weston_pointer_start_grab(struct weston_pointer *pointer,
			  struct weston_pointer_grab *grab)
{
	pointer->grab = grab;
	grab->pointer = pointer;
	pointer->grab->interface->focus(pointer->grab);
}

WL_EXPORT void
weston_pointer_end_grab(struct weston_pointer *pointer)
{
	pointer->grab = &pointer->default_grab;
	pointer->grab->interface->focus(pointer->grab);
}

static void
weston_pointer_cancel_grab(struct weston_pointer *pointer)
{
	pointer->grab->interface->cancel(pointer->grab);
}

WL_EXPORT void
weston_touch_start_grab(struct weston_touch *touch, struct weston_touch_grab *grab)
{
	touch->grab = grab;
	grab->touch = touch;
}

WL_EXPORT void
weston_touch_end_grab(struct weston_touch *touch)
{
	touch->grab = &touch->default_grab;
}

static void
weston_touch_cancel_grab(struct weston_touch *touch)
{
	touch->grab->interface->cancel(touch->grab);
}

static void
weston_pointer_clamp_for_output(struct weston_pointer *pointer,
				struct weston_output *output,
				wl_fixed_t *fx, wl_fixed_t *fy)
{
	int x, y;

	x = wl_fixed_to_int(*fx);
	y = wl_fixed_to_int(*fy);

	if (x < output->x)
		*fx = wl_fixed_from_int(output->x);
	else if (x >= output->x + output->width)
		*fx = wl_fixed_from_int(output->x +
					output->width - 1);
	if (y < output->y)
		*fy = wl_fixed_from_int(output->y);
	else if (y >= output->y + output->height)
		*fy = wl_fixed_from_int(output->y +
					output->height - 1);
}

WL_EXPORT void
weston_pointer_clamp(struct weston_pointer *pointer, wl_fixed_t *fx, wl_fixed_t *fy)
{
	struct weston_compositor *ec = pointer->seat->compositor;
	struct weston_output *output, *prev = NULL;
	int x, y, old_x, old_y, valid = 0;

	x = wl_fixed_to_int(*fx);
	y = wl_fixed_to_int(*fy);
	old_x = wl_fixed_to_int(pointer->x);
	old_y = wl_fixed_to_int(pointer->y);

	wl_list_for_each(output, &ec->output_list, link) {
		if (pointer->seat->output && pointer->seat->output != output)
			continue;
		if (pixman_region32_contains_point(&output->region,
						   x, y, NULL))
			valid = 1;
		if (pixman_region32_contains_point(&output->region,
						   old_x, old_y, NULL))
			prev = output;
	}

	if (!prev)
		prev = pointer->seat->output;

	if (prev && !valid)
		weston_pointer_clamp_for_output(pointer, prev, fx, fy);
}

static void
weston_pointer_move_to(struct weston_pointer *pointer,
		       wl_fixed_t x, wl_fixed_t y)
{
	int32_t ix, iy;

	weston_pointer_clamp (pointer, &x, &y);

	pointer->x = x;
	pointer->y = y;

	ix = wl_fixed_to_int(x);
	iy = wl_fixed_to_int(y);

	if (pointer->sprite) {
		weston_view_set_position(pointer->sprite,
					 ix - pointer->hotspot_x,
					 iy - pointer->hotspot_y);
		weston_view_schedule_repaint(pointer->sprite);
	}

	pointer->grab->interface->focus(pointer->grab);
	wl_signal_emit(&pointer->motion_signal, pointer);
}

WL_EXPORT void
weston_pointer_motion_to_abs(struct weston_pointer *pointer,
			     struct weston_pointer_motion_event *event,
			     wl_fixed_t *x, wl_fixed_t *y)
{
	if (event->mask & WESTON_POINTER_MOTION_ABS) {
		*x = wl_fixed_from_double(event->x);
		*y = wl_fixed_from_double(event->y);
	} else if (event->mask & WESTON_POINTER_MOTION_REL) {
		*x = pointer->x + wl_fixed_from_double(event->dx);
		*y = pointer->y + wl_fixed_from_double(event->dy);
	} else {
		assert(!"invalid motion event");
		*x = *y = 0;
	}
}

WL_EXPORT void
weston_pointer_move(struct weston_pointer *pointer,
		    struct weston_pointer_motion_event *event)
{
	wl_fixed_t x, y;

	weston_pointer_motion_to_abs(pointer, event, &x, &y);
	weston_pointer_move_to(pointer, x, y);
}

/** Verify if the pointer is in a valid position and move it if it isn't.
 */
static void
weston_pointer_handle_output_destroy(struct wl_listener *listener, void *data)
{
	struct weston_pointer *pointer;
	struct weston_compositor *ec;
	struct weston_output *output, *closest = NULL;
	int x, y, distance, min = INT_MAX;
	wl_fixed_t fx, fy;

	pointer = container_of(listener, struct weston_pointer,
			       output_destroy_listener);
	ec = pointer->seat->compositor;

	x = wl_fixed_to_int(pointer->x);
	y = wl_fixed_to_int(pointer->y);

	wl_list_for_each(output, &ec->output_list, link) {
		if (pixman_region32_contains_point(&output->region,
						   x, y, NULL))
			return;

		/* Aproximante the distance from the pointer to the center of
		 * the output. */
		distance = abs(output->x + output->width / 2 - x) +
			   abs(output->y + output->height / 2 - y);
		if (distance < min) {
			min = distance;
			closest = output;
		}
	}

	/* Nothing to do if there's no output left. */
	if (!closest)
		return;

	fx = pointer->x;
	fy = pointer->y;

	weston_pointer_clamp_for_output(pointer, closest, &fx, &fy);
	weston_pointer_move_to(pointer, fx, fy);
}

WL_EXPORT void
notify_motion(struct weston_seat *seat,
	      uint32_t time,
	      struct weston_pointer_motion_event *event)
{
	struct weston_compositor *ec = seat->compositor;
	struct weston_pointer *pointer = weston_seat_get_pointer(seat);

	weston_compositor_wake(ec);
	pointer->grab->interface->motion(pointer->grab, time, event);
}

static void
run_modifier_bindings(struct weston_seat *seat, uint32_t old, uint32_t new)
{
	struct weston_compositor *compositor = seat->compositor;
	struct weston_keyboard *keyboard = weston_seat_get_keyboard(seat);
	uint32_t diff;
	unsigned int i;
	struct {
		uint32_t xkb;
		enum weston_keyboard_modifier weston;
	} mods[] = {
		{ keyboard->xkb_info->ctrl_mod, MODIFIER_CTRL },
		{ keyboard->xkb_info->alt_mod, MODIFIER_ALT },
		{ keyboard->xkb_info->super_mod, MODIFIER_SUPER },
		{ keyboard->xkb_info->shift_mod, MODIFIER_SHIFT },
	};

	diff = new & ~old;
	for (i = 0; i < ARRAY_LENGTH(mods); i++) {
		if (diff & (1 << mods[i].xkb))
			weston_compositor_run_modifier_binding(compositor,
			                                       keyboard,
			                                       mods[i].weston,
			                                       WL_KEYBOARD_KEY_STATE_PRESSED);
	}

	diff = old & ~new;
	for (i = 0; i < ARRAY_LENGTH(mods); i++) {
		if (diff & (1 << mods[i].xkb))
			weston_compositor_run_modifier_binding(compositor,
			                                       keyboard,
			                                       mods[i].weston,
			                                       WL_KEYBOARD_KEY_STATE_RELEASED);
	}
}

WL_EXPORT void
notify_motion_absolute(struct weston_seat *seat,
		       uint32_t time, double x, double y)
{
	struct weston_compositor *ec = seat->compositor;
	struct weston_pointer *pointer = weston_seat_get_pointer(seat);
	struct weston_pointer_motion_event event = { 0 };

	weston_compositor_wake(ec);

	event = (struct weston_pointer_motion_event) {
		.mask = WESTON_POINTER_MOTION_ABS,
		.x = x,
		.y = y,
	};

	pointer->grab->interface->motion(pointer->grab, time, &event);
}

WL_EXPORT void
weston_surface_activate(struct weston_surface *surface,
			struct weston_seat *seat)
{
	struct weston_compositor *compositor = seat->compositor;
	struct weston_keyboard *keyboard = weston_seat_get_keyboard(seat);

	if (keyboard) {
		weston_keyboard_set_focus(keyboard, surface);
		wl_data_device_set_keyboard_focus(seat);
	}

	wl_signal_emit(&compositor->activate_signal, surface);
}

WL_EXPORT void
notify_button(struct weston_seat *seat, uint32_t time, int32_t button,
	      enum wl_pointer_button_state state)
{
	struct weston_compositor *compositor = seat->compositor;
	struct weston_pointer *pointer = weston_seat_get_pointer(seat);

	if (state == WL_POINTER_BUTTON_STATE_PRESSED) {
		weston_compositor_idle_inhibit(compositor);
		if (pointer->button_count == 0) {
			pointer->grab_button = button;
			pointer->grab_time = time;
			pointer->grab_x = pointer->x;
			pointer->grab_y = pointer->y;
		}
		pointer->button_count++;
	} else {
		weston_compositor_idle_release(compositor);
		pointer->button_count--;
	}

	weston_compositor_run_button_binding(compositor, pointer, time, button,
					     state);

	pointer->grab->interface->button(pointer->grab, time, button, state);

	if (pointer->button_count == 1)
		pointer->grab_serial =
			wl_display_get_serial(compositor->wl_display);
}

WL_EXPORT void
notify_axis(struct weston_seat *seat, uint32_t time,
	    struct weston_pointer_axis_event *event)
{
	struct weston_compositor *compositor = seat->compositor;
	struct weston_pointer *pointer = weston_seat_get_pointer(seat);

	weston_compositor_wake(compositor);

	if (weston_compositor_run_axis_binding(compositor, pointer,
					       time, event))
		return;

	pointer->grab->interface->axis(pointer->grab, time, event);
}

WL_EXPORT void
notify_axis_source(struct weston_seat *seat, uint32_t source)
{
	struct weston_compositor *compositor = seat->compositor;
	struct weston_pointer *pointer = weston_seat_get_pointer(seat);

	weston_compositor_wake(compositor);

	pointer->grab->interface->axis_source(pointer->grab, source);
}

WL_EXPORT void
notify_pointer_frame(struct weston_seat *seat)
{
	struct weston_compositor *compositor = seat->compositor;
	struct weston_pointer *pointer = weston_seat_get_pointer(seat);

	weston_compositor_wake(compositor);

	pointer->grab->interface->frame(pointer->grab);
}

WL_EXPORT int
weston_keyboard_set_locks(struct weston_keyboard *keyboard,
			  uint32_t mask, uint32_t value)
{
#ifdef ENABLE_XKBCOMMON
	uint32_t serial;
	xkb_mod_mask_t mods_depressed, mods_latched, mods_locked, group;
	xkb_mod_mask_t num, caps;

	/* We don't want the leds to go out of sync with the actual state
	 * so if the backend has no way to change the leds don't try to
	 * change the state */
	if (!keyboard->seat->led_update)
		return -1;

	mods_depressed = xkb_state_serialize_mods(keyboard->xkb_state.state,
						XKB_STATE_DEPRESSED);
	mods_latched = xkb_state_serialize_mods(keyboard->xkb_state.state,
						XKB_STATE_LATCHED);
	mods_locked = xkb_state_serialize_mods(keyboard->xkb_state.state,
						XKB_STATE_LOCKED);
	group = xkb_state_serialize_group(keyboard->xkb_state.state,
                                      XKB_STATE_EFFECTIVE);

	num = (1 << keyboard->xkb_info->mod2_mod);
	caps = (1 << keyboard->xkb_info->caps_mod);
	if (mask & WESTON_NUM_LOCK) {
		if (value & WESTON_NUM_LOCK)
			mods_locked |= num;
		else
			mods_locked &= ~num;
	}
	if (mask & WESTON_CAPS_LOCK) {
		if (value & WESTON_CAPS_LOCK)
			mods_locked |= caps;
		else
			mods_locked &= ~caps;
	}

	xkb_state_update_mask(keyboard->xkb_state.state, mods_depressed,
			      mods_latched, mods_locked, 0, 0, group);

	serial = wl_display_next_serial(
				keyboard->seat->compositor->wl_display);
	notify_modifiers(keyboard->seat, serial);

	return 0;
#else
	return -1;
#endif
}

#ifdef ENABLE_XKBCOMMON
WL_EXPORT void
notify_modifiers(struct weston_seat *seat, uint32_t serial)
{
	struct weston_keyboard *keyboard = weston_seat_get_keyboard(seat);
	struct weston_keyboard_grab *grab = keyboard->grab;
	uint32_t mods_depressed, mods_latched, mods_locked, group;
	uint32_t mods_lookup;
	enum weston_led leds = 0;
	int changed = 0;

	/* Serialize and update our internal state, checking to see if it's
	 * different to the previous state. */
	mods_depressed = xkb_state_serialize_mods(keyboard->xkb_state.state,
						  XKB_STATE_MODS_DEPRESSED);
	mods_latched = xkb_state_serialize_mods(keyboard->xkb_state.state,
						XKB_STATE_MODS_LATCHED);
	mods_locked = xkb_state_serialize_mods(keyboard->xkb_state.state,
					       XKB_STATE_MODS_LOCKED);
	group = xkb_state_serialize_layout(keyboard->xkb_state.state,
					   XKB_STATE_LAYOUT_EFFECTIVE);

	if (mods_depressed != keyboard->modifiers.mods_depressed ||
	    mods_latched != keyboard->modifiers.mods_latched ||
	    mods_locked != keyboard->modifiers.mods_locked ||
	    group != keyboard->modifiers.group)
		changed = 1;

	run_modifier_bindings(seat, keyboard->modifiers.mods_depressed,
	                      mods_depressed);

	keyboard->modifiers.mods_depressed = mods_depressed;
	keyboard->modifiers.mods_latched = mods_latched;
	keyboard->modifiers.mods_locked = mods_locked;
	keyboard->modifiers.group = group;

	/* And update the modifier_state for bindings. */
	mods_lookup = mods_depressed | mods_latched;
	seat->modifier_state = 0;
	if (mods_lookup & (1 << keyboard->xkb_info->ctrl_mod))
		seat->modifier_state |= MODIFIER_CTRL;
	if (mods_lookup & (1 << keyboard->xkb_info->alt_mod))
		seat->modifier_state |= MODIFIER_ALT;
	if (mods_lookup & (1 << keyboard->xkb_info->super_mod))
		seat->modifier_state |= MODIFIER_SUPER;
	if (mods_lookup & (1 << keyboard->xkb_info->shift_mod))
		seat->modifier_state |= MODIFIER_SHIFT;

	/* Finally, notify the compositor that LEDs have changed. */
	if (xkb_state_led_index_is_active(keyboard->xkb_state.state,
					  keyboard->xkb_info->num_led))
		leds |= LED_NUM_LOCK;
	if (xkb_state_led_index_is_active(keyboard->xkb_state.state,
					  keyboard->xkb_info->caps_led))
		leds |= LED_CAPS_LOCK;
	if (xkb_state_led_index_is_active(keyboard->xkb_state.state,
					  keyboard->xkb_info->scroll_led))
		leds |= LED_SCROLL_LOCK;
	if (leds != keyboard->xkb_state.leds && seat->led_update)
		seat->led_update(seat, leds);
	keyboard->xkb_state.leds = leds;

	if (changed) {
		grab->interface->modifiers(grab,
					   serial,
					   keyboard->modifiers.mods_depressed,
					   keyboard->modifiers.mods_latched,
					   keyboard->modifiers.mods_locked,
					   keyboard->modifiers.group);
	}
}

static void
update_modifier_state(struct weston_seat *seat, uint32_t serial, uint32_t key,
		      enum wl_keyboard_key_state state)
{
	struct weston_keyboard *keyboard = weston_seat_get_keyboard(seat);
	enum xkb_key_direction direction;

	/* Keyboard modifiers don't exist in raw keyboard mode */
	if (!seat->compositor->use_xkbcommon)
		return;

	if (state == WL_KEYBOARD_KEY_STATE_PRESSED)
		direction = XKB_KEY_DOWN;
	else
		direction = XKB_KEY_UP;

	/* Offset the keycode by 8, as the evdev XKB rules reflect X's
	 * broken keycode system, which starts at 8. */
	xkb_state_update_key(keyboard->xkb_state.state, key + 8, direction);

	notify_modifiers(seat, serial);
}

static void
send_keymap(struct wl_resource *resource, struct weston_xkb_info *xkb_info)
{
	wl_keyboard_send_keymap(resource,
				WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1,
				xkb_info->keymap_fd,
				xkb_info->keymap_size);
}

static void
send_modifiers(struct wl_resource *resource, uint32_t serial, struct weston_keyboard *keyboard)
{
	wl_keyboard_send_modifiers(resource, serial,
				   keyboard->modifiers.mods_depressed,
				   keyboard->modifiers.mods_latched,
				   keyboard->modifiers.mods_locked,
				   keyboard->modifiers.group);
}

static struct weston_xkb_info *
weston_xkb_info_create(struct xkb_keymap *keymap);

static void
update_keymap(struct weston_seat *seat)
{
	struct weston_keyboard *keyboard = weston_seat_get_keyboard(seat);
	struct wl_resource *resource;
	struct weston_xkb_info *xkb_info;
	struct xkb_state *state;
	xkb_mod_mask_t latched_mods;
	xkb_mod_mask_t locked_mods;

	xkb_info = weston_xkb_info_create(keyboard->pending_keymap);

	xkb_keymap_unref(keyboard->pending_keymap);
	keyboard->pending_keymap = NULL;

	if (!xkb_info) {
		weston_log("failed to create XKB info\n");
		return;
	}

	state = xkb_state_new(xkb_info->keymap);
	if (!state) {
		weston_log("failed to initialise XKB state\n");
		weston_xkb_info_destroy(xkb_info);
		return;
	}

	latched_mods = xkb_state_serialize_mods(keyboard->xkb_state.state,
						XKB_STATE_MODS_LATCHED);
	locked_mods = xkb_state_serialize_mods(keyboard->xkb_state.state,
					       XKB_STATE_MODS_LOCKED);
	xkb_state_update_mask(state,
			      0, /* depressed */
			      latched_mods,
			      locked_mods,
			      0, 0, 0);

	weston_xkb_info_destroy(keyboard->xkb_info);
	keyboard->xkb_info = xkb_info;

	xkb_state_unref(keyboard->xkb_state.state);
	keyboard->xkb_state.state = state;

	wl_resource_for_each(resource, &keyboard->resource_list)
		send_keymap(resource, xkb_info);
	wl_resource_for_each(resource, &keyboard->focus_resource_list)
		send_keymap(resource, xkb_info);

	notify_modifiers(seat, wl_display_next_serial(seat->compositor->wl_display));

	if (!latched_mods && !locked_mods)
		return;

	wl_resource_for_each(resource, &keyboard->resource_list)
		send_modifiers(resource, wl_display_get_serial(seat->compositor->wl_display), keyboard);
	wl_resource_for_each(resource, &keyboard->focus_resource_list)
		send_modifiers(resource, wl_display_get_serial(seat->compositor->wl_display), keyboard);
}
#else
WL_EXPORT void
notify_modifiers(struct weston_seat *seat, uint32_t serial)
{
}

static void
update_modifier_state(struct weston_seat *seat, uint32_t serial, uint32_t key,
		      enum wl_keyboard_key_state state)
{
}

static void
update_keymap(struct weston_seat *seat)
{
}
#endif

WL_EXPORT void
notify_key(struct weston_seat *seat, uint32_t time, uint32_t key,
	   enum wl_keyboard_key_state state,
	   enum weston_key_state_update update_state)
{
	struct weston_compositor *compositor = seat->compositor;
	struct weston_keyboard *keyboard = weston_seat_get_keyboard(seat);
	struct weston_keyboard_grab *grab = keyboard->grab;
	uint32_t *k, *end;

	if (state == WL_KEYBOARD_KEY_STATE_PRESSED) {
		weston_compositor_idle_inhibit(compositor);
	} else {
		weston_compositor_idle_release(compositor);
	}

	end = keyboard->keys.data + keyboard->keys.size;
	for (k = keyboard->keys.data; k < end; k++) {
		if (*k == key) {
			/* Ignore server-generated repeats. */
			if (state == WL_KEYBOARD_KEY_STATE_PRESSED)
				return;
			*k = *--end;
		}
	}
	keyboard->keys.size = (void *) end - keyboard->keys.data;
	if (state == WL_KEYBOARD_KEY_STATE_PRESSED) {
		k = wl_array_add(&keyboard->keys, sizeof *k);
		*k = key;
	}

	if (grab == &keyboard->default_grab ||
	    grab == &keyboard->input_method_grab) {
		weston_compositor_run_key_binding(compositor, keyboard, time,
						  key, state);
		grab = keyboard->grab;
	}

	grab->interface->key(grab, time, key, state);

	if (keyboard->pending_keymap &&
	    keyboard->keys.size == 0)
		update_keymap(seat);

	if (update_state == STATE_UPDATE_AUTOMATIC) {
		update_modifier_state(seat,
				      wl_display_get_serial(compositor->wl_display),
				      key,
				      state);
	}

	if (state == WL_KEYBOARD_KEY_STATE_PRESSED) {
		keyboard->grab_serial =
			wl_display_get_serial(compositor->wl_display);
		keyboard->grab_time = time;
		keyboard->grab_key = key;
	}
}

WL_EXPORT void
notify_pointer_focus(struct weston_seat *seat, struct weston_output *output,
		     double x, double y)
{
	struct weston_pointer *pointer = weston_seat_get_pointer(seat);

	if (output) {
		weston_pointer_move_to(pointer,
				       wl_fixed_from_double(x),
				       wl_fixed_from_double(y));
	} else {
		/* FIXME: We should call weston_pointer_set_focus(seat,
		 * NULL) here, but somehow that breaks re-entry... */
	}
}

static void
destroy_device_saved_kbd_focus(struct wl_listener *listener, void *data)
{
	struct weston_seat *ws;

	ws = container_of(listener, struct weston_seat,
			  saved_kbd_focus_listener);

	ws->saved_kbd_focus = NULL;
}

WL_EXPORT void
notify_keyboard_focus_in(struct weston_seat *seat, struct wl_array *keys,
			 enum weston_key_state_update update_state)
{
	struct weston_compositor *compositor = seat->compositor;
	struct weston_keyboard *keyboard = weston_seat_get_keyboard(seat);
	struct weston_surface *surface;
	uint32_t *k, serial;

	serial = wl_display_next_serial(compositor->wl_display);
	wl_array_copy(&keyboard->keys, keys);
	wl_array_for_each(k, &keyboard->keys) {
		weston_compositor_idle_inhibit(compositor);
		if (update_state == STATE_UPDATE_AUTOMATIC)
			update_modifier_state(seat, serial, *k,
					      WL_KEYBOARD_KEY_STATE_PRESSED);
	}

	surface = seat->saved_kbd_focus;

	if (surface) {
		wl_list_remove(&seat->saved_kbd_focus_listener.link);
		weston_keyboard_set_focus(keyboard, surface);
		seat->saved_kbd_focus = NULL;
	}
}

WL_EXPORT void
notify_keyboard_focus_out(struct weston_seat *seat)
{
	struct weston_compositor *compositor = seat->compositor;
	struct weston_keyboard *keyboard = weston_seat_get_keyboard(seat);
	struct weston_pointer *pointer = weston_seat_get_pointer(seat);
	uint32_t *k, serial;

	serial = wl_display_next_serial(compositor->wl_display);
	wl_array_for_each(k, &keyboard->keys) {
		weston_compositor_idle_release(compositor);
		update_modifier_state(seat, serial, *k,
				      WL_KEYBOARD_KEY_STATE_RELEASED);
	}

	seat->modifier_state = 0;

	if (keyboard->focus) {
		seat->saved_kbd_focus = keyboard->focus;
		seat->saved_kbd_focus_listener.notify =
			destroy_device_saved_kbd_focus;
		wl_signal_add(&keyboard->focus->destroy_signal,
			      &seat->saved_kbd_focus_listener);
	}

	weston_keyboard_set_focus(keyboard, NULL);
	weston_keyboard_cancel_grab(keyboard);
	if (pointer)
		weston_pointer_cancel_grab(pointer);
}

WL_EXPORT void
weston_touch_set_focus(struct weston_touch *touch, struct weston_view *view)
{
	struct wl_list *focus_resource_list;

	focus_resource_list = &touch->focus_resource_list;

	if (view && touch->focus &&
	    touch->focus->surface == view->surface) {
		touch->focus = view;
		return;
	}

	wl_list_remove(&touch->focus_resource_listener.link);
	wl_list_init(&touch->focus_resource_listener.link);
	wl_list_remove(&touch->focus_view_listener.link);
	wl_list_init(&touch->focus_view_listener.link);

	if (!wl_list_empty(focus_resource_list)) {
		move_resources(&touch->resource_list,
			       focus_resource_list);
	}

	if (view) {
		struct wl_client *surface_client;

		if (!view->surface->resource) {
			touch->focus = NULL;
			return;
		}

		surface_client = wl_resource_get_client(view->surface->resource);
		move_resources_for_client(focus_resource_list,
					  &touch->resource_list,
					  surface_client);
		wl_resource_add_destroy_listener(view->surface->resource,
						 &touch->focus_resource_listener);
		wl_signal_add(&view->destroy_signal, &touch->focus_view_listener);
	}
	touch->focus = view;
}

/**
 * notify_touch - emulates button touches and notifies surfaces accordingly.
 *
 * It assumes always the correct cycle sequence until it gets here: touch_down
 * → touch_update → ... → touch_update → touch_end. The driver is responsible
 * for sending along such order.
 *
 */
WL_EXPORT void
notify_touch(struct weston_seat *seat, uint32_t time, int touch_id,
             double double_x, double double_y, int touch_type)
{
	struct weston_compositor *ec = seat->compositor;
	struct weston_touch *touch = weston_seat_get_touch(seat);
	struct weston_touch_grab *grab = touch->grab;
	struct weston_view *ev;
	wl_fixed_t sx, sy;
	wl_fixed_t x = wl_fixed_from_double(double_x);
	wl_fixed_t y = wl_fixed_from_double(double_y);

	/* Update grab's global coordinates. */
	if (touch_id == touch->grab_touch_id && touch_type != WL_TOUCH_UP) {
		touch->grab_x = x;
		touch->grab_y = y;
	}

	switch (touch_type) {
	case WL_TOUCH_DOWN:
		weston_compositor_idle_inhibit(ec);

		touch->num_tp++;

		/* the first finger down picks the view, and all further go
		 * to that view for the remainder of the touch session i.e.
		 * until all touch points are up again. */
		if (touch->num_tp == 1) {
			ev = weston_compositor_pick_view(ec, x, y, &sx, &sy);
			weston_touch_set_focus(touch, ev);
		} else if (!touch->focus) {
			/* Unexpected condition: We have non-initial touch but
			 * there is no focused surface.
			 */
			weston_log("touch event received with %d points down "
				   "but no surface focused\n", touch->num_tp);
			return;
		}

		weston_compositor_run_touch_binding(ec, touch,
						    time, touch_type);

		grab->interface->down(grab, time, touch_id, x, y);
		if (touch->num_tp == 1) {
			touch->grab_serial =
				wl_display_get_serial(ec->wl_display);
			touch->grab_touch_id = touch_id;
			touch->grab_time = time;
			touch->grab_x = x;
			touch->grab_y = y;
		}

		break;
	case WL_TOUCH_MOTION:
		ev = touch->focus;
		if (!ev)
			break;

		grab->interface->motion(grab, time, touch_id, x, y);
		break;
	case WL_TOUCH_UP:
		if (touch->num_tp == 0) {
			/* This can happen if we start out with one or
			 * more fingers on the touch screen, in which
			 * case we didn't get the corresponding down
			 * event. */
			weston_log("unmatched touch up event\n");
			break;
		}
		weston_compositor_idle_release(ec);
		touch->num_tp--;

		grab->interface->up(grab, time, touch_id);
		if (touch->num_tp == 0)
			weston_touch_set_focus(touch, NULL);
		break;
	}
}

WL_EXPORT void
notify_touch_frame(struct weston_seat *seat)
{
	struct weston_touch *touch = weston_seat_get_touch(seat);
	struct weston_touch_grab *grab = touch->grab;

	grab->interface->frame(grab);
}

WL_EXPORT void
notify_touch_cancel(struct weston_seat *seat)
{
	struct weston_touch *touch = weston_seat_get_touch(seat);
	struct weston_touch_grab *grab = touch->grab;

	grab->interface->cancel(grab);
}

static int
pointer_cursor_surface_get_label(struct weston_surface *surface,
				 char *buf, size_t len)
{
	return snprintf(buf, len, "cursor");
}

static void
pointer_cursor_surface_configure(struct weston_surface *es,
				 int32_t dx, int32_t dy)
{
	struct weston_pointer *pointer = es->configure_private;
	int x, y;

	if (es->width == 0)
		return;

	assert(es == pointer->sprite->surface);

	pointer->hotspot_x -= dx;
	pointer->hotspot_y -= dy;

	x = wl_fixed_to_int(pointer->x) - pointer->hotspot_x;
	y = wl_fixed_to_int(pointer->y) - pointer->hotspot_y;

	weston_view_set_position(pointer->sprite, x, y);

	empty_region(&es->pending.input);
	empty_region(&es->input);

	if (!weston_surface_is_mapped(es)) {
		weston_layer_entry_insert(&es->compositor->cursor_layer.view_list,
					  &pointer->sprite->layer_link);
		weston_view_update_transform(pointer->sprite);
	}
}

static void
pointer_set_cursor(struct wl_client *client, struct wl_resource *resource,
		   uint32_t serial, struct wl_resource *surface_resource,
		   int32_t x, int32_t y)
{
	struct weston_pointer *pointer = wl_resource_get_user_data(resource);
	struct weston_surface *surface = NULL;

	if (surface_resource)
		surface = wl_resource_get_user_data(surface_resource);

	if (pointer->focus == NULL)
		return;
	/* pointer->focus->surface->resource can be NULL. Surfaces like the
	black_surface used in shell.c for fullscreen don't have
	a resource, but can still have focus */
	if (pointer->focus->surface->resource == NULL)
		return;
	if (wl_resource_get_client(pointer->focus->surface->resource) != client)
		return;
	if (pointer->focus_serial - serial > UINT32_MAX / 2)
		return;

	if (!surface) {
		if (pointer->sprite)
			pointer_unmap_sprite(pointer);
		return;
	}

	if (pointer->sprite && pointer->sprite->surface == surface &&
	    pointer->hotspot_x == x && pointer->hotspot_y == y)
		return;

	if (!pointer->sprite || pointer->sprite->surface != surface) {
		if (weston_surface_set_role(surface, "wl_pointer-cursor",
					    resource,
					    WL_POINTER_ERROR_ROLE) < 0)
			return;

		if (pointer->sprite)
			pointer_unmap_sprite(pointer);

		wl_signal_add(&surface->destroy_signal,
			      &pointer->sprite_destroy_listener);

		surface->configure = pointer_cursor_surface_configure;
		surface->configure_private = pointer;
		weston_surface_set_label_func(surface,
					    pointer_cursor_surface_get_label);
		pointer->sprite = weston_view_create(surface);
	}

	pointer->hotspot_x = x;
	pointer->hotspot_y = y;

	if (surface->buffer_ref.buffer) {
		pointer_cursor_surface_configure(surface, 0, 0);
		weston_view_schedule_repaint(pointer->sprite);
	}
}

static void
pointer_release(struct wl_client *client, struct wl_resource *resource)
{
	wl_resource_destroy(resource);
}

static const struct wl_pointer_interface pointer_interface = {
	pointer_set_cursor,
	pointer_release
};

static void
seat_get_pointer(struct wl_client *client, struct wl_resource *resource,
		 uint32_t id)
{
	struct weston_seat *seat = wl_resource_get_user_data(resource);
	/* We use the pointer_state directly, which means we'll
	 * give a wl_pointer if the seat has ever had one - even though
	 * the spec explicitly states that this request only takes effect
	 * if the seat has the pointer capability.
	 *
	 * This prevents a race between the compositor sending new
	 * capabilities and the client trying to use the old ones.
	 */
	struct weston_pointer *pointer = seat->pointer_state;
	struct wl_resource *cr;
	struct weston_pointer_client *pointer_client;

	if (!pointer)
		return;

        cr = wl_resource_create(client, &wl_pointer_interface,
				wl_resource_get_version(resource), id);
	if (cr == NULL) {
		wl_client_post_no_memory(client);
		return;
	}

	pointer_client = weston_pointer_ensure_pointer_client(pointer, client);
	if (!pointer_client) {
		wl_client_post_no_memory(client);
		return;
	}

	wl_list_insert(&pointer_client->pointer_resources,
		       wl_resource_get_link(cr));
	wl_resource_set_implementation(cr, &pointer_interface, pointer,
				       unbind_pointer_client_resource);

	if (pointer->focus && pointer->focus->surface->resource &&
	    wl_resource_get_client(pointer->focus->surface->resource) == client) {
		wl_fixed_t sx, sy;

		weston_view_from_global_fixed(pointer->focus,
					      pointer->x,
					      pointer->y,
					      &sx, &sy);

		wl_pointer_send_enter(cr,
				      pointer->focus_serial,
				      pointer->focus->surface->resource,
				      sx, sy);
		pointer_send_frame(cr);
	}
}

static void
keyboard_release(struct wl_client *client, struct wl_resource *resource)
{
	wl_resource_destroy(resource);
}

static const struct wl_keyboard_interface keyboard_interface = {
	keyboard_release
};

static bool
should_send_modifiers_to_client(struct weston_seat *seat,
				struct wl_client *client)
{
	struct weston_keyboard *keyboard = weston_seat_get_keyboard(seat);
	struct weston_pointer *pointer = weston_seat_get_pointer(seat);

	if (keyboard &&
	    keyboard->focus &&
	    keyboard->focus->resource &&
	    wl_resource_get_client(keyboard->focus->resource) == client)
		return true;

	if (pointer &&
	    pointer->focus &&
	    pointer->focus->surface->resource &&
	    wl_resource_get_client(pointer->focus->surface->resource) == client)
		return true;

	return false;
}

static void
seat_get_keyboard(struct wl_client *client, struct wl_resource *resource,
		  uint32_t id)
{
	struct weston_seat *seat = wl_resource_get_user_data(resource);
	/* We use the keyboard_state directly, which means we'll
	 * give a wl_keyboard if the seat has ever had one - even though
	 * the spec explicitly states that this request only takes effect
	 * if the seat has the keyboard capability.
	 *
	 * This prevents a race between the compositor sending new
	 * capabilities and the client trying to use the old ones.
	 */
	struct weston_keyboard *keyboard = seat->keyboard_state;
	struct wl_resource *cr;

	if (!keyboard)
		return;

        cr = wl_resource_create(client, &wl_keyboard_interface,
				wl_resource_get_version(resource), id);
	if (cr == NULL) {
		wl_client_post_no_memory(client);
		return;
	}

	/* May be moved to focused list later by either
	 * weston_keyboard_set_focus or directly if this client is already
	 * focused */
	wl_list_insert(&keyboard->resource_list, wl_resource_get_link(cr));
	wl_resource_set_implementation(cr, &keyboard_interface,
				       seat, unbind_resource);

	if (wl_resource_get_version(cr) >= WL_KEYBOARD_REPEAT_INFO_SINCE_VERSION) {
		wl_keyboard_send_repeat_info(cr,
					     seat->compositor->kb_repeat_rate,
					     seat->compositor->kb_repeat_delay);
	}

	if (seat->compositor->use_xkbcommon) {
		wl_keyboard_send_keymap(cr, WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1,
					keyboard->xkb_info->keymap_fd,
					keyboard->xkb_info->keymap_size);
	} else {
		int null_fd = open("/dev/null", O_RDONLY);
		wl_keyboard_send_keymap(cr, WL_KEYBOARD_KEYMAP_FORMAT_NO_KEYMAP,
					null_fd,
					0);
		close(null_fd);
	}

	if (should_send_modifiers_to_client(seat, client)) {
		send_modifiers_to_resource(keyboard,
					   cr,
					   keyboard->focus_serial);
	}

	if (keyboard->focus && keyboard->focus->resource &&
	    wl_resource_get_client(keyboard->focus->resource) == client) {
		struct weston_surface *surface =
			(struct weston_surface *)keyboard->focus;

		wl_list_remove(wl_resource_get_link(cr));
		wl_list_insert(&keyboard->focus_resource_list,
			       wl_resource_get_link(cr));
		wl_keyboard_send_enter(cr,
				       keyboard->focus_serial,
				       surface->resource,
				       &keyboard->keys);

		/* If this is the first keyboard resource for this
		 * client... */
		if (keyboard->focus_resource_list.prev ==
		    wl_resource_get_link(cr))
			wl_data_device_set_keyboard_focus(seat);
	}
}

static void
touch_release(struct wl_client *client, struct wl_resource *resource)
{
	wl_resource_destroy(resource);
}

static const struct wl_touch_interface touch_interface = {
	touch_release
};

static void
seat_get_touch(struct wl_client *client, struct wl_resource *resource,
	       uint32_t id)
{
	struct weston_seat *seat = wl_resource_get_user_data(resource);
	/* We use the touch_state directly, which means we'll
	 * give a wl_touch if the seat has ever had one - even though
	 * the spec explicitly states that this request only takes effect
	 * if the seat has the touch capability.
	 *
	 * This prevents a race between the compositor sending new
	 * capabilities and the client trying to use the old ones.
	 */
	struct weston_touch *touch = seat->touch_state;
	struct wl_resource *cr;

	if (!touch)
		return;

        cr = wl_resource_create(client, &wl_touch_interface,
				wl_resource_get_version(resource), id);
	if (cr == NULL) {
		wl_client_post_no_memory(client);
		return;
	}

	if (touch->focus &&
	    wl_resource_get_client(touch->focus->surface->resource) == client) {
		wl_list_insert(&touch->focus_resource_list,
			       wl_resource_get_link(cr));
	} else {
		wl_list_insert(&touch->resource_list,
			       wl_resource_get_link(cr));
	}
	wl_resource_set_implementation(cr, &touch_interface,
				       seat, unbind_resource);
}

static void
seat_release(struct wl_client *client, struct wl_resource *resource)
{
	wl_resource_destroy(resource);
}

static const struct wl_seat_interface seat_interface = {
	seat_get_pointer,
	seat_get_keyboard,
	seat_get_touch,
	seat_release,
};

static void
bind_seat(struct wl_client *client, void *data, uint32_t version, uint32_t id)
{
	struct weston_seat *seat = data;
	struct wl_resource *resource;
	enum wl_seat_capability caps = 0;

	resource = wl_resource_create(client,
				      &wl_seat_interface, version, id);
	wl_list_insert(&seat->base_resource_list, wl_resource_get_link(resource));
	wl_resource_set_implementation(resource, &seat_interface, data,
				       unbind_resource);

	if (weston_seat_get_pointer(seat))
		caps |= WL_SEAT_CAPABILITY_POINTER;
	if (weston_seat_get_keyboard(seat))
		caps |= WL_SEAT_CAPABILITY_KEYBOARD;
	if (weston_seat_get_touch(seat))
		caps |= WL_SEAT_CAPABILITY_TOUCH;

	wl_seat_send_capabilities(resource, caps);
	if (version >= WL_SEAT_NAME_SINCE_VERSION)
		wl_seat_send_name(resource, seat->seat_name);
}

#ifdef ENABLE_XKBCOMMON
int
weston_compositor_xkb_init(struct weston_compositor *ec,
			   struct xkb_rule_names *names)
{
	ec->use_xkbcommon = 1;

	if (ec->xkb_context == NULL) {
		ec->xkb_context = xkb_context_new(0);
		if (ec->xkb_context == NULL) {
			weston_log("failed to create XKB context\n");
			return -1;
		}
	}

	if (names)
		ec->xkb_names = *names;
	if (!ec->xkb_names.rules)
		ec->xkb_names.rules = strdup("evdev");
	if (!ec->xkb_names.model)
		ec->xkb_names.model = strdup("pc105");
	if (!ec->xkb_names.layout)
		ec->xkb_names.layout = strdup("us");

	return 0;
}

static void
weston_xkb_info_destroy(struct weston_xkb_info *xkb_info)
{
	if (--xkb_info->ref_count > 0)
		return;

	xkb_keymap_unref(xkb_info->keymap);

	if (xkb_info->keymap_area)
		munmap(xkb_info->keymap_area, xkb_info->keymap_size);
	if (xkb_info->keymap_fd >= 0)
		close(xkb_info->keymap_fd);
	free(xkb_info);
}

void
weston_compositor_xkb_destroy(struct weston_compositor *ec)
{
	/*
	 * If we're operating in raw keyboard mode, we never initialized
	 * libxkbcommon so there's no cleanup to do either.
	 */
	if (!ec->use_xkbcommon)
		return;

	free((char *) ec->xkb_names.rules);
	free((char *) ec->xkb_names.model);
	free((char *) ec->xkb_names.layout);
	free((char *) ec->xkb_names.variant);
	free((char *) ec->xkb_names.options);

	if (ec->xkb_info)
		weston_xkb_info_destroy(ec->xkb_info);
	xkb_context_unref(ec->xkb_context);
}

static struct weston_xkb_info *
weston_xkb_info_create(struct xkb_keymap *keymap)
{
	struct weston_xkb_info *xkb_info = zalloc(sizeof *xkb_info);
	if (xkb_info == NULL)
		return NULL;

	xkb_info->keymap = xkb_keymap_ref(keymap);
	xkb_info->ref_count = 1;

	char *keymap_str;

	xkb_info->shift_mod = xkb_keymap_mod_get_index(xkb_info->keymap,
						       XKB_MOD_NAME_SHIFT);
	xkb_info->caps_mod = xkb_keymap_mod_get_index(xkb_info->keymap,
						      XKB_MOD_NAME_CAPS);
	xkb_info->ctrl_mod = xkb_keymap_mod_get_index(xkb_info->keymap,
						      XKB_MOD_NAME_CTRL);
	xkb_info->alt_mod = xkb_keymap_mod_get_index(xkb_info->keymap,
						     XKB_MOD_NAME_ALT);
	xkb_info->mod2_mod = xkb_keymap_mod_get_index(xkb_info->keymap,
						      "Mod2");
	xkb_info->mod3_mod = xkb_keymap_mod_get_index(xkb_info->keymap,
						      "Mod3");
	xkb_info->super_mod = xkb_keymap_mod_get_index(xkb_info->keymap,
						       XKB_MOD_NAME_LOGO);
	xkb_info->mod5_mod = xkb_keymap_mod_get_index(xkb_info->keymap,
						      "Mod5");

	xkb_info->num_led = xkb_keymap_led_get_index(xkb_info->keymap,
						     XKB_LED_NAME_NUM);
	xkb_info->caps_led = xkb_keymap_led_get_index(xkb_info->keymap,
						      XKB_LED_NAME_CAPS);
	xkb_info->scroll_led = xkb_keymap_led_get_index(xkb_info->keymap,
							XKB_LED_NAME_SCROLL);

	keymap_str = xkb_keymap_get_as_string(xkb_info->keymap,
					      XKB_KEYMAP_FORMAT_TEXT_V1);
	if (keymap_str == NULL) {
		weston_log("failed to get string version of keymap\n");
		goto err_keymap;
	}
	xkb_info->keymap_size = strlen(keymap_str) + 1;

	xkb_info->keymap_fd = os_create_anonymous_file(xkb_info->keymap_size);
	if (xkb_info->keymap_fd < 0) {
		weston_log("creating a keymap file for %lu bytes failed: %m\n",
			(unsigned long) xkb_info->keymap_size);
		goto err_keymap_str;
	}

	xkb_info->keymap_area = mmap(NULL, xkb_info->keymap_size,
				     PROT_READ | PROT_WRITE,
				     MAP_SHARED, xkb_info->keymap_fd, 0);
	if (xkb_info->keymap_area == MAP_FAILED) {
		weston_log("failed to mmap() %lu bytes\n",
			(unsigned long) xkb_info->keymap_size);
		goto err_dev_zero;
	}
	strcpy(xkb_info->keymap_area, keymap_str);
	free(keymap_str);

	return xkb_info;

err_dev_zero:
	close(xkb_info->keymap_fd);
err_keymap_str:
	free(keymap_str);
err_keymap:
	xkb_keymap_unref(xkb_info->keymap);
	free(xkb_info);
	return NULL;
}

static int
weston_compositor_build_global_keymap(struct weston_compositor *ec)
{
	struct xkb_keymap *keymap;

	if (ec->xkb_info != NULL)
		return 0;

	keymap = xkb_keymap_new_from_names(ec->xkb_context,
					   &ec->xkb_names,
					   0);
	if (keymap == NULL) {
		weston_log("failed to compile global XKB keymap\n");
		weston_log("  tried rules %s, model %s, layout %s, variant %s, "
			"options %s\n",
			ec->xkb_names.rules, ec->xkb_names.model,
			ec->xkb_names.layout, ec->xkb_names.variant,
			ec->xkb_names.options);
		return -1;
	}

	ec->xkb_info = weston_xkb_info_create(keymap);
	xkb_keymap_unref(keymap);
	if (ec->xkb_info == NULL)
		return -1;

	return 0;
}
#else
int
weston_compositor_xkb_init(struct weston_compositor *ec,
			   struct xkb_rule_names *names)
{
	return 0;
}

void
weston_compositor_xkb_destroy(struct weston_compositor *ec)
{
}
#endif

WL_EXPORT void
weston_seat_update_keymap(struct weston_seat *seat, struct xkb_keymap *keymap)
{
	struct weston_keyboard *keyboard = weston_seat_get_keyboard(seat);

	if (!keyboard || !keymap)
		return;

#ifdef ENABLE_XKBCOMMON
	if (!seat->compositor->use_xkbcommon)
		return;

	xkb_keymap_unref(keyboard->pending_keymap);
	keyboard->pending_keymap = xkb_keymap_ref(keymap);

	if (keyboard->keys.size == 0)
		update_keymap(seat);
#endif
}

WL_EXPORT int
weston_seat_init_keyboard(struct weston_seat *seat, struct xkb_keymap *keymap)
{
	struct weston_keyboard *keyboard;

	if (seat->keyboard_state) {
		seat->keyboard_device_count += 1;
		if (seat->keyboard_device_count == 1)
			seat_send_updated_caps(seat);
		return 0;
	}

	keyboard = weston_keyboard_create();
	if (keyboard == NULL) {
		weston_log("failed to allocate weston keyboard struct\n");
		return -1;
	}

#ifdef ENABLE_XKBCOMMON
	if (seat->compositor->use_xkbcommon) {
		if (keymap != NULL) {
			keyboard->xkb_info = weston_xkb_info_create(keymap);
			if (keyboard->xkb_info == NULL)
				goto err;
		} else {
			if (weston_compositor_build_global_keymap(seat->compositor) < 0)
				goto err;
			keyboard->xkb_info = seat->compositor->xkb_info;
			keyboard->xkb_info->ref_count++;
		}

		keyboard->xkb_state.state = xkb_state_new(keyboard->xkb_info->keymap);
		if (keyboard->xkb_state.state == NULL) {
			weston_log("failed to initialise XKB state\n");
			goto err;
		}

		keyboard->xkb_state.leds = 0;
	}
#endif

	seat->keyboard_state = keyboard;
	seat->keyboard_device_count = 1;
	keyboard->seat = seat;

	seat_send_updated_caps(seat);

	return 0;

err:
	if (keyboard->xkb_info)
		weston_xkb_info_destroy(keyboard->xkb_info);
	free(keyboard);

	return -1;
}

static void
weston_keyboard_reset_state(struct weston_keyboard *keyboard)
{
	struct weston_seat *seat = keyboard->seat;
	struct xkb_state *state;

#ifdef ENABLE_XKBCOMMON
	if (seat->compositor->use_xkbcommon) {
		state = xkb_state_new(keyboard->xkb_info->keymap);
		if (!state) {
			weston_log("failed to reset XKB state\n");
			return;
		}
		xkb_state_unref(keyboard->xkb_state.state);
		keyboard->xkb_state.state = state;

		keyboard->xkb_state.leds = 0;
	}
#endif

	seat->modifier_state = 0;
}

WL_EXPORT void
weston_seat_release_keyboard(struct weston_seat *seat)
{
	seat->keyboard_device_count--;
	assert(seat->keyboard_device_count >= 0);
	if (seat->keyboard_device_count == 0) {
		weston_keyboard_set_focus(seat->keyboard_state, NULL);
		weston_keyboard_cancel_grab(seat->keyboard_state);
		weston_keyboard_reset_state(seat->keyboard_state);
		seat_send_updated_caps(seat);
	}
}

WL_EXPORT void
weston_seat_init_pointer(struct weston_seat *seat)
{
	struct weston_pointer *pointer;

	if (seat->pointer_state) {
		seat->pointer_device_count += 1;
		if (seat->pointer_device_count == 1)
			seat_send_updated_caps(seat);
		return;
	}

	pointer = weston_pointer_create(seat);
	if (pointer == NULL)
		return;

	seat->pointer_state = pointer;
	seat->pointer_device_count = 1;
	pointer->seat = seat;

	seat_send_updated_caps(seat);
}

WL_EXPORT void
weston_seat_release_pointer(struct weston_seat *seat)
{
	struct weston_pointer *pointer = seat->pointer_state;

	seat->pointer_device_count--;
	if (seat->pointer_device_count == 0) {
		weston_pointer_clear_focus(pointer);
		weston_pointer_cancel_grab(pointer);

		if (pointer->sprite)
			pointer_unmap_sprite(pointer);

		weston_pointer_reset_state(pointer);
		seat_send_updated_caps(seat);

		/* seat->pointer is intentionally not destroyed so that
		 * a newly attached pointer on this seat will retain
		 * the previous cursor co-ordinates.
		 */
	}
}

WL_EXPORT void
weston_seat_init_touch(struct weston_seat *seat)
{
	struct weston_touch *touch;

	if (seat->touch_state) {
		seat->touch_device_count += 1;
		if (seat->touch_device_count == 1)
			seat_send_updated_caps(seat);
		return;
	}

	touch = weston_touch_create();
	if (touch == NULL)
		return;

	seat->touch_state = touch;
	seat->touch_device_count = 1;
	touch->seat = seat;

	seat_send_updated_caps(seat);
}

WL_EXPORT void
weston_seat_release_touch(struct weston_seat *seat)
{
	seat->touch_device_count--;
	if (seat->touch_device_count == 0) {
		weston_touch_set_focus(seat->touch_state, NULL);
		weston_touch_cancel_grab(seat->touch_state);
		weston_touch_reset_state(seat->touch_state);
		seat_send_updated_caps(seat);
	}
}

WL_EXPORT void
weston_seat_init(struct weston_seat *seat, struct weston_compositor *ec,
		 const char *seat_name)
{
	memset(seat, 0, sizeof *seat);

	seat->selection_data_source = NULL;
	wl_list_init(&seat->base_resource_list);
	wl_signal_init(&seat->selection_signal);
	wl_list_init(&seat->drag_resource_list);
	wl_signal_init(&seat->destroy_signal);
	wl_signal_init(&seat->updated_caps_signal);

	seat->global = wl_global_create(ec->wl_display, &wl_seat_interface, 5,
					seat, bind_seat);

	seat->compositor = ec;
	seat->modifier_state = 0;
	seat->seat_name = strdup(seat_name);

	wl_list_insert(ec->seat_list.prev, &seat->link);

	clipboard_create(seat);

	wl_signal_emit(&ec->seat_created_signal, seat);
}

WL_EXPORT void
weston_seat_release(struct weston_seat *seat)
{
	wl_list_remove(&seat->link);

	if (seat->saved_kbd_focus)
		wl_list_remove(&seat->saved_kbd_focus_listener.link);

	if (seat->pointer_state)
		weston_pointer_destroy(seat->pointer_state);
	if (seat->keyboard_state)
		weston_keyboard_destroy(seat->keyboard_state);
	if (seat->touch_state)
		weston_touch_destroy(seat->touch_state);

	free (seat->seat_name);

	wl_global_destroy(seat->global);

	wl_signal_emit(&seat->destroy_signal, seat);
}

/** Get a seat's keyboard pointer
 *
 * \param seat The seat to query
 * \return The seat's keyboard pointer, or NULL if no keyboard is present
 *
 * The keyboard pointer for a seat isn't freed when all keyboards are removed,
 * so it should only be used when the seat's keyboard_device_count is greater
 * than zero.  This function does that test and only returns a pointer
 * when a keyboard is present.
 */
WL_EXPORT struct weston_keyboard *
weston_seat_get_keyboard(struct weston_seat *seat)
{
	if (!seat)
		return NULL;

	if (seat->keyboard_device_count)
		return seat->keyboard_state;

	return NULL;
}

/** Get a seat's pointer pointer
 *
 * \param seat The seat to query
 * \return The seat's pointer pointer, or NULL if no pointer device is present
 *
 * The pointer pointer for a seat isn't freed when all mice are removed,
 * so it should only be used when the seat's pointer_device_count is greater
 * than zero.  This function does that test and only returns a pointer
 * when a pointing device is present.
 */
WL_EXPORT struct weston_pointer *
weston_seat_get_pointer(struct weston_seat *seat)
{
	if (!seat)
		return NULL;

	if (seat->pointer_device_count)
		return seat->pointer_state;

	return NULL;
}

/** Get a seat's touch pointer
 *
 * \param seat The seat to query
 * \return The seat's touch pointer, or NULL if no touch device is present
 *
 * The touch pointer for a seat isn't freed when all touch devices are removed,
 * so it should only be used when the seat's touch_device_count is greater
 * than zero.  This function does that test and only returns a pointer
 * when a touch device is present.
 */
WL_EXPORT struct weston_touch *
weston_seat_get_touch(struct weston_seat *seat)
{
	if (!seat)
		return NULL;

	if (seat->touch_device_count)
		return seat->touch_state;

	return NULL;
}
