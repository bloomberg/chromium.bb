/*
 * Copyright © 2011 Kristian Høgsberg
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
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <assert.h>

#include "compositor.h"
#include "shared/helpers.h"

struct weston_drag {
	struct wl_client *client;
	struct weston_data_source *data_source;
	struct wl_listener data_source_listener;
	struct weston_view *focus;
	struct wl_resource *focus_resource;
	struct wl_listener focus_listener;
	struct weston_view *icon;
	struct wl_listener icon_destroy_listener;
	int32_t dx, dy;
};

struct weston_pointer_drag {
	struct weston_drag  base;
	struct weston_pointer_grab grab;
};

struct weston_touch_drag {
	struct weston_drag base;
	struct weston_touch_grab grab;
};

static void
data_offer_accept(struct wl_client *client, struct wl_resource *resource,
		  uint32_t serial, const char *mime_type)
{
	struct weston_data_offer *offer = wl_resource_get_user_data(resource);

	/* FIXME: Check that client is currently focused by the input
	 * device that is currently dragging this data source.  Should
	 * this be a wl_data_device request? */

	if (offer->source)
		offer->source->accept(offer->source, serial, mime_type);
}

static void
data_offer_receive(struct wl_client *client, struct wl_resource *resource,
		   const char *mime_type, int32_t fd)
{
	struct weston_data_offer *offer = wl_resource_get_user_data(resource);

	if (offer->source)
		offer->source->send(offer->source, mime_type, fd);
	else
		close(fd);
}

static void
data_offer_destroy(struct wl_client *client, struct wl_resource *resource)
{
	wl_resource_destroy(resource);
}

static const struct wl_data_offer_interface data_offer_interface = {
	data_offer_accept,
	data_offer_receive,
	data_offer_destroy,
};

static void
destroy_data_offer(struct wl_resource *resource)
{
	struct weston_data_offer *offer = wl_resource_get_user_data(resource);

	if (offer->source)
		wl_list_remove(&offer->source_destroy_listener.link);
	free(offer);
}

static void
destroy_offer_data_source(struct wl_listener *listener, void *data)
{
	struct weston_data_offer *offer;

	offer = container_of(listener, struct weston_data_offer,
			     source_destroy_listener);

	offer->source = NULL;
}

static struct wl_resource *
weston_data_source_send_offer(struct weston_data_source *source,
			      struct wl_resource *target)
{
	struct weston_data_offer *offer;
	char **p;

	offer = malloc(sizeof *offer);
	if (offer == NULL)
		return NULL;

	offer->resource =
		wl_resource_create(wl_resource_get_client(target),
				   &wl_data_offer_interface, 1, 0);
	if (offer->resource == NULL) {
		free(offer);
		return NULL;
	}

	wl_resource_set_implementation(offer->resource, &data_offer_interface,
				       offer, destroy_data_offer);

	offer->source = source;
	offer->source_destroy_listener.notify = destroy_offer_data_source;
	wl_signal_add(&source->destroy_signal,
		      &offer->source_destroy_listener);

	wl_data_device_send_data_offer(target, offer->resource);

	wl_array_for_each(p, &source->mime_types)
		wl_data_offer_send_offer(offer->resource, *p);

	return offer->resource;
}

static void
data_source_offer(struct wl_client *client,
		  struct wl_resource *resource,
		  const char *type)
{
	struct weston_data_source *source =
		wl_resource_get_user_data(resource);
	char **p;

	p = wl_array_add(&source->mime_types, sizeof *p);
	if (p)
		*p = strdup(type);
	if (!p || !*p)
		wl_resource_post_no_memory(resource);
}

static void
data_source_destroy(struct wl_client *client, struct wl_resource *resource)
{
	wl_resource_destroy(resource);
}

static struct wl_data_source_interface data_source_interface = {
	data_source_offer,
	data_source_destroy
};

static void
drag_surface_configure(struct weston_drag *drag,
		       struct weston_pointer *pointer,
		       struct weston_touch *touch,
		       struct weston_surface *es,
		       int32_t sx, int32_t sy)
{
	struct weston_layer_entry *list;
	float fx, fy;

	assert((pointer != NULL && touch == NULL) ||
			(pointer == NULL && touch != NULL));

	if (!weston_surface_is_mapped(es) && es->buffer_ref.buffer) {
		if (pointer && pointer->sprite &&
			weston_view_is_mapped(pointer->sprite))
			list = &pointer->sprite->layer_link;
		else
			list = &es->compositor->cursor_layer.view_list;

		weston_layer_entry_remove(&drag->icon->layer_link);
		weston_layer_entry_insert(list, &drag->icon->layer_link);
		weston_view_update_transform(drag->icon);
		pixman_region32_clear(&es->pending.input);
	}

	drag->dx += sx;
	drag->dy += sy;

	/* init to 0 for avoiding a compile warning */
	fx = fy = 0;
	if (pointer) {
		fx = wl_fixed_to_double(pointer->x) + drag->dx;
		fy = wl_fixed_to_double(pointer->y) + drag->dy;
	} else if (touch) {
		fx = wl_fixed_to_double(touch->grab_x) + drag->dx;
		fy = wl_fixed_to_double(touch->grab_y) + drag->dy;
	}
	weston_view_set_position(drag->icon, fx, fy);
}

static int
pointer_drag_surface_get_label(struct weston_surface *surface,
			       char *buf, size_t len)
{
	return snprintf(buf, len, "pointer drag icon");
}

static void
pointer_drag_surface_configure(struct weston_surface *es,
			       int32_t sx, int32_t sy)
{
	struct weston_pointer_drag *drag = es->configure_private;
	struct weston_pointer *pointer = drag->grab.pointer;

	assert(es->configure == pointer_drag_surface_configure);

	drag_surface_configure(&drag->base, pointer, NULL, es, sx, sy);
}

static int
touch_drag_surface_get_label(struct weston_surface *surface,
			     char *buf, size_t len)
{
	return snprintf(buf, len, "touch drag icon");
}

static void
touch_drag_surface_configure(struct weston_surface *es, int32_t sx, int32_t sy)
{
	struct weston_touch_drag *drag = es->configure_private;
	struct weston_touch *touch = drag->grab.touch;

	assert(es->configure == touch_drag_surface_configure);

	drag_surface_configure(&drag->base, NULL, touch, es, sx, sy);
}

static void
destroy_drag_focus(struct wl_listener *listener, void *data)
{
	struct weston_drag *drag =
		container_of(listener, struct weston_drag, focus_listener);

	drag->focus_resource = NULL;
}

static void
weston_drag_set_focus(struct weston_drag *drag,
			struct weston_seat *seat,
			struct weston_view *view,
			wl_fixed_t sx, wl_fixed_t sy)
{
	struct wl_resource *resource, *offer = NULL;
	struct wl_display *display = seat->compositor->wl_display;
	uint32_t serial;

	if (drag->focus && view && drag->focus->surface == view->surface) {
		drag->focus = view;
		return;
	}

	if (drag->focus_resource) {
		wl_data_device_send_leave(drag->focus_resource);
		wl_list_remove(&drag->focus_listener.link);
		drag->focus_resource = NULL;
		drag->focus = NULL;
	}

	if (!view || !view->surface->resource)
		return;

	if (!drag->data_source &&
	    wl_resource_get_client(view->surface->resource) != drag->client)
		return;

	resource = wl_resource_find_for_client(&seat->drag_resource_list,
					       wl_resource_get_client(view->surface->resource));
	if (!resource)
		return;

	serial = wl_display_next_serial(display);

	if (drag->data_source) {
		offer = weston_data_source_send_offer(drag->data_source,
						      resource);
		if (offer == NULL)
			return;
	}

	wl_data_device_send_enter(resource, serial, view->surface->resource,
				  sx, sy, offer);

	drag->focus = view;
	drag->focus_listener.notify = destroy_drag_focus;
	wl_resource_add_destroy_listener(resource, &drag->focus_listener);
	drag->focus_resource = resource;
}

static void
drag_grab_focus(struct weston_pointer_grab *grab)
{
	struct weston_pointer_drag *drag =
		container_of(grab, struct weston_pointer_drag, grab);
	struct weston_pointer *pointer = grab->pointer;
	struct weston_view *view;
	wl_fixed_t sx, sy;

	view = weston_compositor_pick_view(pointer->seat->compositor,
					   pointer->x, pointer->y,
					   &sx, &sy);
	if (drag->base.focus != view)
		weston_drag_set_focus(&drag->base, pointer->seat, view, sx, sy);
}

static void
drag_grab_motion(struct weston_pointer_grab *grab, uint32_t time,
		 wl_fixed_t x, wl_fixed_t y)
{
	struct weston_pointer_drag *drag =
		container_of(grab, struct weston_pointer_drag, grab);
	struct weston_pointer *pointer = drag->grab.pointer;
	float fx, fy;
	wl_fixed_t sx, sy;

	weston_pointer_move(pointer, x, y);

	if (drag->base.icon) {
		fx = wl_fixed_to_double(pointer->x) + drag->base.dx;
		fy = wl_fixed_to_double(pointer->y) + drag->base.dy;
		weston_view_set_position(drag->base.icon, fx, fy);
		weston_view_schedule_repaint(drag->base.icon);
	}

	if (drag->base.focus_resource) {
		weston_view_from_global_fixed(drag->base.focus,
					      pointer->x, pointer->y,
					      &sx, &sy);

		wl_data_device_send_motion(drag->base.focus_resource, time, sx, sy);
	}
}

static void
data_device_end_drag_grab(struct weston_drag *drag,
		struct weston_seat *seat)
{
	if (drag->icon) {
		if (weston_view_is_mapped(drag->icon))
			weston_view_unmap(drag->icon);

		drag->icon->surface->configure = NULL;
		weston_surface_set_label_func(drag->icon->surface, NULL);
		pixman_region32_clear(&drag->icon->surface->pending.input);
		wl_list_remove(&drag->icon_destroy_listener.link);
		weston_view_destroy(drag->icon);
	}

	weston_drag_set_focus(drag, seat, NULL, 0, 0);
}

static void
data_device_end_pointer_drag_grab(struct weston_pointer_drag *drag)
{
	struct weston_pointer *pointer = drag->grab.pointer;

	data_device_end_drag_grab(&drag->base, pointer->seat);
	weston_pointer_end_grab(pointer);
	free(drag);
}

static void
drag_grab_button(struct weston_pointer_grab *grab,
		 uint32_t time, uint32_t button, uint32_t state_w)
{
	struct weston_pointer_drag *drag =
		container_of(grab, struct weston_pointer_drag, grab);
	struct weston_pointer *pointer = drag->grab.pointer;
	enum wl_pointer_button_state state = state_w;

	if (drag->base.focus_resource &&
	    pointer->grab_button == button &&
	    state == WL_POINTER_BUTTON_STATE_RELEASED)
		wl_data_device_send_drop(drag->base.focus_resource);

	if (pointer->button_count == 0 &&
	    state == WL_POINTER_BUTTON_STATE_RELEASED) {
		if (drag->base.data_source)
			wl_list_remove(&drag->base.data_source_listener.link);
		data_device_end_pointer_drag_grab(drag);
	}
}

static void
drag_grab_cancel(struct weston_pointer_grab *grab)
{
	struct weston_pointer_drag *drag =
		container_of(grab, struct weston_pointer_drag, grab);

	if (drag->base.data_source)
		wl_list_remove(&drag->base.data_source_listener.link);

	data_device_end_pointer_drag_grab(drag);
}

static const struct weston_pointer_grab_interface pointer_drag_grab_interface = {
	drag_grab_focus,
	drag_grab_motion,
	drag_grab_button,
	drag_grab_cancel,
};

static void
drag_grab_touch_down(struct weston_touch_grab *grab, uint32_t time,
		int touch_id, wl_fixed_t sx, wl_fixed_t sy)
{
}

static void
data_device_end_touch_drag_grab(struct weston_touch_drag *drag)
{
	struct weston_touch *touch = drag->grab.touch;

	data_device_end_drag_grab(&drag->base, touch->seat);
	weston_touch_end_grab(touch);
	free(drag);
}

static void
drag_grab_touch_up(struct weston_touch_grab *grab,
		uint32_t time, int touch_id)
{
	struct weston_touch_drag *touch_drag =
		container_of(grab, struct weston_touch_drag, grab);
	struct weston_touch *touch = grab->touch;

	if (touch_id != touch->grab_touch_id)
		return;

	if (touch_drag->base.focus_resource)
		wl_data_device_send_drop(touch_drag->base.focus_resource);
	if (touch_drag->base.data_source)
		wl_list_remove(&touch_drag->base.data_source_listener.link);
	data_device_end_touch_drag_grab(touch_drag);
}

static void
drag_grab_touch_focus(struct weston_touch_drag *drag)
{
	struct weston_touch *touch = drag->grab.touch;
	struct weston_view *view;
	wl_fixed_t view_x, view_y;

	view = weston_compositor_pick_view(touch->seat->compositor,
				touch->grab_x, touch->grab_y,
				&view_x, &view_y);
	if (drag->base.focus != view)
		weston_drag_set_focus(&drag->base, touch->seat,
				view, view_x, view_y);
}

static void
drag_grab_touch_motion(struct weston_touch_grab *grab, uint32_t time,
		int touch_id, wl_fixed_t sx, wl_fixed_t sy)
{
	struct weston_touch_drag *touch_drag =
		container_of(grab, struct weston_touch_drag, grab);
	struct weston_touch *touch = grab->touch;
	wl_fixed_t view_x, view_y;
	float fx, fy;

	if (touch_id != touch->grab_touch_id)
		return;

	drag_grab_touch_focus(touch_drag);
	if (touch_drag->base.icon) {
		fx = wl_fixed_to_double(touch->grab_x) + touch_drag->base.dx;
		fy = wl_fixed_to_double(touch->grab_y) + touch_drag->base.dy;
		weston_view_set_position(touch_drag->base.icon, fx, fy);
		weston_view_schedule_repaint(touch_drag->base.icon);
	}

	if (touch_drag->base.focus_resource) {
		weston_view_from_global_fixed(touch_drag->base.focus,
					touch->grab_x, touch->grab_y,
					&view_x, &view_y);
		wl_data_device_send_motion(touch_drag->base.focus_resource, time,
					view_x, view_y);
	}
}

static void
drag_grab_touch_frame(struct weston_touch_grab *grab)
{
}

static void
drag_grab_touch_cancel(struct weston_touch_grab *grab)
{
	struct weston_touch_drag *touch_drag =
		container_of(grab, struct weston_touch_drag, grab);

	if (touch_drag->base.data_source)
		wl_list_remove(&touch_drag->base.data_source_listener.link);
	data_device_end_touch_drag_grab(touch_drag);
}

static const struct weston_touch_grab_interface touch_drag_grab_interface = {
	drag_grab_touch_down,
	drag_grab_touch_up,
	drag_grab_touch_motion,
	drag_grab_touch_frame,
	drag_grab_touch_cancel
};

static void
destroy_pointer_data_device_source(struct wl_listener *listener, void *data)
{
	struct weston_pointer_drag *drag = container_of(listener,
			struct weston_pointer_drag, base.data_source_listener);

	data_device_end_pointer_drag_grab(drag);
}

static void
handle_drag_icon_destroy(struct wl_listener *listener, void *data)
{
	struct weston_drag *drag = container_of(listener, struct weston_drag,
						icon_destroy_listener);

	drag->icon = NULL;
}

WL_EXPORT int
weston_pointer_start_drag(struct weston_pointer *pointer,
		       struct weston_data_source *source,
		       struct weston_surface *icon,
		       struct wl_client *client)
{
	struct weston_pointer_drag *drag;

	drag = zalloc(sizeof *drag);
	if (drag == NULL)
		return -1;

	drag->grab.interface = &pointer_drag_grab_interface;
	drag->base.client = client;
	drag->base.data_source = source;

	if (icon) {
		drag->base.icon = weston_view_create(icon);
		if (drag->base.icon == NULL) {
			free(drag);
			return -1;
		}

		drag->base.icon_destroy_listener.notify = handle_drag_icon_destroy;
		wl_signal_add(&icon->destroy_signal,
			      &drag->base.icon_destroy_listener);

		icon->configure = pointer_drag_surface_configure;
		icon->configure_private = drag;
		weston_surface_set_label_func(icon,
					pointer_drag_surface_get_label);
	} else {
		drag->base.icon = NULL;
	}

	if (source) {
		drag->base.data_source_listener.notify = destroy_pointer_data_device_source;
		wl_signal_add(&source->destroy_signal,
			      &drag->base.data_source_listener);
	}

	weston_pointer_set_focus(pointer, NULL,
				 wl_fixed_from_int(0), wl_fixed_from_int(0));
	weston_pointer_start_grab(pointer, &drag->grab);

	return 0;
}

static void
destroy_touch_data_device_source(struct wl_listener *listener, void *data)
{
	struct weston_touch_drag *drag = container_of(listener,
			struct weston_touch_drag, base.data_source_listener);

	data_device_end_touch_drag_grab(drag);
}

WL_EXPORT int
weston_touch_start_drag(struct weston_touch *touch,
		       struct weston_data_source *source,
		       struct weston_surface *icon,
		       struct wl_client *client)
{
	struct weston_touch_drag *drag;

	drag = zalloc(sizeof *drag);
	if (drag == NULL)
		return -1;

	drag->grab.interface = &touch_drag_grab_interface;
	drag->base.client = client;
	drag->base.data_source = source;

	if (icon) {
		drag->base.icon = weston_view_create(icon);
		if (drag->base.icon == NULL) {
			free(drag);
			return -1;
		}

		drag->base.icon_destroy_listener.notify = handle_drag_icon_destroy;
		wl_signal_add(&icon->destroy_signal,
			      &drag->base.icon_destroy_listener);

		icon->configure = touch_drag_surface_configure;
		icon->configure_private = drag;
		weston_surface_set_label_func(icon,
					touch_drag_surface_get_label);
	} else {
		drag->base.icon = NULL;
	}

	if (source) {
		drag->base.data_source_listener.notify = destroy_touch_data_device_source;
		wl_signal_add(&source->destroy_signal,
			      &drag->base.data_source_listener);
	}

	weston_touch_start_grab(touch, &drag->grab);

	drag_grab_touch_focus(drag);

	return 0;
}

static void
data_device_start_drag(struct wl_client *client, struct wl_resource *resource,
		       struct wl_resource *source_resource,
		       struct wl_resource *origin_resource,
		       struct wl_resource *icon_resource, uint32_t serial)
{
	struct weston_seat *seat = wl_resource_get_user_data(resource);
	struct weston_surface *origin = wl_resource_get_user_data(origin_resource);
	struct weston_data_source *source = NULL;
	struct weston_surface *icon = NULL;
	int is_pointer_grab, is_touch_grab;
	int32_t ret = 0;

	is_pointer_grab = seat->pointer &&
			  seat->pointer->button_count == 1 &&
			  seat->pointer->grab_serial == serial &&
			  seat->pointer->focus &&
			  seat->pointer->focus->surface == origin;

	is_touch_grab = seat->touch &&
			seat->touch->num_tp == 1 &&
			seat->touch->grab_serial == serial &&
			seat->touch->focus &&
			seat->touch->focus->surface == origin;

	if (!is_pointer_grab && !is_touch_grab)
		return;

	/* FIXME: Check that the data source type array isn't empty. */

	if (source_resource)
		source = wl_resource_get_user_data(source_resource);
	if (icon_resource)
		icon = wl_resource_get_user_data(icon_resource);

	if (icon) {
		if (weston_surface_set_role(icon, "wl_data_device-icon",
					    resource,
					    WL_DATA_DEVICE_ERROR_ROLE) < 0)
			return;
	}

	if (is_pointer_grab)
		ret = weston_pointer_start_drag(seat->pointer, source, icon, client);
	else if (is_touch_grab)
		ret = weston_touch_start_drag(seat->touch, source, icon, client);

	if (ret < 0)
		wl_resource_post_no_memory(resource);
}

static void
destroy_selection_data_source(struct wl_listener *listener, void *data)
{
	struct weston_seat *seat = container_of(listener, struct weston_seat,
						selection_data_source_listener);
	struct wl_resource *data_device;
	struct weston_surface *focus = NULL;

	seat->selection_data_source = NULL;

	if (seat->keyboard)
		focus = seat->keyboard->focus;
	if (focus && focus->resource) {
		data_device = wl_resource_find_for_client(&seat->drag_resource_list,
							  wl_resource_get_client(focus->resource));
		if (data_device)
			wl_data_device_send_selection(data_device, NULL);
	}

	wl_signal_emit(&seat->selection_signal, seat);
}

/** \brief Send the selection to the specified client
 *
 * This function creates a new wl_data_offer if there is a wl_data_source
 * currently set as the selection and sends it to the specified client,
 * followed by the wl_data_device.selection() event.
 * If there is no current selection the wl_data_device.selection() event
 * will carry a NULL wl_data_offer.
 *
 * If the client does not have a wl_data_device for the specified seat
 * nothing will be done.
 *
 * \param seat The seat owning the wl_data_device used to send the events.
 * \param client The client to which to send the selection.
 */
WL_EXPORT void
weston_seat_send_selection(struct weston_seat *seat, struct wl_client *client)
{
	struct wl_resource *data_device, *offer;

	wl_resource_for_each(data_device, &seat->drag_resource_list) {
		if (wl_resource_get_client(data_device) != client)
		    continue;

		if (seat->selection_data_source) {
			offer = weston_data_source_send_offer(seat->selection_data_source,
								data_device);
			wl_data_device_send_selection(data_device, offer);
		} else {
			wl_data_device_send_selection(data_device, NULL);
		}
	}
}

WL_EXPORT void
weston_seat_set_selection(struct weston_seat *seat,
			  struct weston_data_source *source, uint32_t serial)
{
	struct weston_surface *focus = NULL;

	if (seat->selection_data_source &&
	    seat->selection_serial - serial < UINT32_MAX / 2)
		return;

	if (seat->selection_data_source) {
		seat->selection_data_source->cancel(seat->selection_data_source);
		wl_list_remove(&seat->selection_data_source_listener.link);
		seat->selection_data_source = NULL;
	}

	seat->selection_data_source = source;
	seat->selection_serial = serial;

	if (seat->keyboard)
		focus = seat->keyboard->focus;
	if (focus && focus->resource) {
		weston_seat_send_selection(seat, wl_resource_get_client(focus->resource));
	}

	wl_signal_emit(&seat->selection_signal, seat);

	if (source) {
		seat->selection_data_source_listener.notify =
			destroy_selection_data_source;
		wl_signal_add(&source->destroy_signal,
			      &seat->selection_data_source_listener);
	}
}

static void
data_device_set_selection(struct wl_client *client,
			  struct wl_resource *resource,
			  struct wl_resource *source_resource, uint32_t serial)
{
	if (!source_resource)
		return;

	/* FIXME: Store serial and check against incoming serial here. */
	weston_seat_set_selection(wl_resource_get_user_data(resource),
				  wl_resource_get_user_data(source_resource),
				  serial);
}
static void
data_device_release(struct wl_client *client, struct wl_resource *resource)
{
	wl_resource_destroy(resource);
}

static const struct wl_data_device_interface data_device_interface = {
	data_device_start_drag,
	data_device_set_selection,
	data_device_release
};

static void
destroy_data_source(struct wl_resource *resource)
{
	struct weston_data_source *source =
		wl_resource_get_user_data(resource);
	char **p;

	wl_signal_emit(&source->destroy_signal, source);

	wl_array_for_each(p, &source->mime_types)
		free(*p);

	wl_array_release(&source->mime_types);

	free(source);
}

static void
client_source_accept(struct weston_data_source *source,
		     uint32_t time, const char *mime_type)
{
	wl_data_source_send_target(source->resource, mime_type);
}

static void
client_source_send(struct weston_data_source *source,
		   const char *mime_type, int32_t fd)
{
	wl_data_source_send_send(source->resource, mime_type, fd);
	close(fd);
}

static void
client_source_cancel(struct weston_data_source *source)
{
	wl_data_source_send_cancelled(source->resource);
}

static void
create_data_source(struct wl_client *client,
		   struct wl_resource *resource, uint32_t id)
{
	struct weston_data_source *source;

	source = malloc(sizeof *source);
	if (source == NULL) {
		wl_resource_post_no_memory(resource);
		return;
	}

	wl_signal_init(&source->destroy_signal);
	source->accept = client_source_accept;
	source->send = client_source_send;
	source->cancel = client_source_cancel;

	wl_array_init(&source->mime_types);

	source->resource =
		wl_resource_create(client, &wl_data_source_interface, 1, id);
	wl_resource_set_implementation(source->resource, &data_source_interface,
				       source, destroy_data_source);
}

static void unbind_data_device(struct wl_resource *resource)
{
	wl_list_remove(wl_resource_get_link(resource));
}

static void
get_data_device(struct wl_client *client,
		struct wl_resource *manager_resource,
		uint32_t id, struct wl_resource *seat_resource)
{
	struct weston_seat *seat = wl_resource_get_user_data(seat_resource);
	struct wl_resource *resource;

	resource = wl_resource_create(client,
				      &wl_data_device_interface,
				      wl_resource_get_version(manager_resource),
				      id);
	if (resource == NULL) {
		wl_resource_post_no_memory(manager_resource);
		return;
	}

	wl_list_insert(&seat->drag_resource_list,
		       wl_resource_get_link(resource));
	wl_resource_set_implementation(resource, &data_device_interface,
				       seat, unbind_data_device);
}

static const struct wl_data_device_manager_interface manager_interface = {
	create_data_source,
	get_data_device
};

static void
bind_manager(struct wl_client *client,
	     void *data, uint32_t version, uint32_t id)
{
	struct wl_resource *resource;

	resource = wl_resource_create(client,
				      &wl_data_device_manager_interface,
				      version, id);
	if (resource == NULL) {
		wl_client_post_no_memory(client);
		return;
	}

	wl_resource_set_implementation(resource, &manager_interface,
				       NULL, NULL);
}

WL_EXPORT void
wl_data_device_set_keyboard_focus(struct weston_seat *seat)
{
	struct weston_surface *focus;

	if (!seat->keyboard)
		return;

	focus = seat->keyboard->focus;
	if (!focus || !focus->resource)
		return;

	weston_seat_send_selection(seat, wl_resource_get_client(focus->resource));
}

WL_EXPORT int
wl_data_device_manager_init(struct wl_display *display)
{
	if (wl_global_create(display,
			     &wl_data_device_manager_interface, 2,
			     NULL, bind_manager) == NULL)
		return -1;

	return 0;
}
