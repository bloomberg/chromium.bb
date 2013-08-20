/*
 * Copyright © 2011 Kristian Høgsberg
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that copyright
 * notice and this permission notice appear in supporting documentation, and
 * that the name of the copyright holders not be used in advertising or
 * publicity pertaining to distribution of the software without specific,
 * written prior permission.  The copyright holders make no representations
 * about the suitability of this software for any purpose.  It is provided "as
 * is" without express or implied warranty.
 *
 * THE COPYRIGHT HOLDERS DISCLAIM ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL THE COPYRIGHT HOLDERS BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE
 * OF THIS SOFTWARE.
 */

#include "config.h"

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>

#include "compositor.h"

struct weston_drag {
	struct wl_client *client;
	struct weston_data_source *data_source;
	struct wl_listener data_source_listener;
	struct weston_surface *focus;
	struct wl_resource *focus_resource;
	struct wl_listener focus_listener;
	struct weston_pointer_grab grab;
	struct weston_surface *icon;
	struct wl_listener icon_destroy_listener;
	int32_t dx, dy;
};

static void
empty_region(pixman_region32_t *region)
{
	pixman_region32_fini(region);
	pixman_region32_init(region);
}

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
drag_surface_configure(struct weston_surface *es, int32_t sx, int32_t sy, int32_t width, int32_t height)
{
	struct weston_drag *drag = es->configure_private;
	struct weston_pointer *pointer = drag->grab.pointer;
	struct wl_list *list;
	float fx, fy;

	if (!weston_surface_is_mapped(es) && es->buffer_ref.buffer) {
		if (pointer->sprite && weston_surface_is_mapped(pointer->sprite))
			list = &pointer->sprite->layer_link;
		else
			list = &es->compositor->cursor_layer.surface_list;

		wl_list_insert(list, &es->layer_link);
		weston_surface_update_transform(es);
		empty_region(&es->pending.input);
	}

	drag->dx += sx;
	drag->dy += sy;

	fx = wl_fixed_to_double(pointer->x) + drag->dx;
	fy = wl_fixed_to_double(pointer->y) + drag->dy;
	weston_surface_configure(es, fx, fy, width, height);
}

static void
destroy_drag_focus(struct wl_listener *listener, void *data)
{
	struct weston_drag *drag =
		container_of(listener, struct weston_drag, focus_listener);

	drag->focus_resource = NULL;
}

static void
weston_drag_set_focus(struct weston_drag *drag, struct weston_surface *surface,
		      wl_fixed_t sx, wl_fixed_t sy)
{
	struct weston_pointer *pointer = drag->grab.pointer;
	struct wl_resource *resource, *offer = NULL;
	struct wl_display *display = pointer->seat->compositor->wl_display;
	uint32_t serial;

	if (drag->focus_resource) {
		wl_data_device_send_leave(drag->focus_resource);
		wl_list_remove(&drag->focus_listener.link);
		drag->focus_resource = NULL;
		drag->focus = NULL;
	}

	if (!surface)
		return;

	if (!drag->data_source &&
	    wl_resource_get_client(surface->resource) != drag->client)
		return;

	resource = wl_resource_find_for_client(&pointer->seat->drag_resource_list,
					       wl_resource_get_client(surface->resource));
	if (!resource)
		return;

	serial = wl_display_next_serial(display);

	if (drag->data_source) {
		offer = weston_data_source_send_offer(drag->data_source,
						      resource);
		if (offer == NULL)
			return;
	}

	wl_data_device_send_enter(resource, serial, surface->resource,
				  sx, sy, offer);

	drag->focus = surface;
	drag->focus_listener.notify = destroy_drag_focus;
	wl_resource_add_destroy_listener(resource, &drag->focus_listener);
	drag->focus_resource = resource;
}

static void
drag_grab_focus(struct weston_pointer_grab *grab)
{
	struct weston_drag *drag =
		container_of(grab, struct weston_drag, grab);
	struct weston_pointer *pointer = grab->pointer;
	struct weston_surface *surface;
	wl_fixed_t sx, sy;

	surface = weston_compositor_pick_surface(pointer->seat->compositor,
						 pointer->x, pointer->y,
						 &sx, &sy);
	if (drag->focus != surface)
		weston_drag_set_focus(drag, surface, sx, sy);
}

static void
drag_grab_motion(struct weston_pointer_grab *grab, uint32_t time)
{
	struct weston_drag *drag =
		container_of(grab, struct weston_drag, grab);
	struct weston_pointer *pointer = drag->grab.pointer;
	float fx, fy;
	wl_fixed_t sx, sy;

	if (drag->icon) {
		fx = wl_fixed_to_double(pointer->x) + drag->dx;
		fy = wl_fixed_to_double(pointer->y) + drag->dy;
		weston_surface_set_position(drag->icon, fx, fy);
		weston_surface_schedule_repaint(drag->icon);
	}

	if (drag->focus_resource) {
		weston_surface_from_global_fixed(drag->focus,
						 pointer->x, pointer->y,
						 &sx, &sy);

		wl_data_device_send_motion(drag->focus_resource, time, sx, sy);
	}
}

static void
data_device_end_drag_grab(struct weston_drag *drag)
{
	if (drag->icon) {
		if (weston_surface_is_mapped(drag->icon))
			weston_surface_unmap(drag->icon);

		drag->icon->configure = NULL;
		empty_region(&drag->icon->pending.input);
		wl_list_remove(&drag->icon_destroy_listener.link);
	}

	weston_drag_set_focus(drag, NULL, 0, 0);

	weston_pointer_end_grab(drag->grab.pointer);

	free(drag);
}

static void
drag_grab_button(struct weston_pointer_grab *grab,
		 uint32_t time, uint32_t button, uint32_t state_w)
{
	struct weston_drag *drag =
		container_of(grab, struct weston_drag, grab);
	struct weston_pointer *pointer = drag->grab.pointer;
	enum wl_pointer_button_state state = state_w;

	if (drag->focus_resource &&
	    pointer->grab_button == button &&
	    state == WL_POINTER_BUTTON_STATE_RELEASED)
		wl_data_device_send_drop(drag->focus_resource);

	if (pointer->button_count == 0 &&
	    state == WL_POINTER_BUTTON_STATE_RELEASED) {
		if (drag->data_source)
			wl_list_remove(&drag->data_source_listener.link);
		data_device_end_drag_grab(drag);
	}
}

static const struct weston_pointer_grab_interface drag_grab_interface = {
	drag_grab_focus,
	drag_grab_motion,
	drag_grab_button,
};

static void
destroy_data_device_source(struct wl_listener *listener, void *data)
{
	struct weston_drag *drag = container_of(listener, struct weston_drag,
						data_source_listener);

	data_device_end_drag_grab(drag);
}

static void
handle_drag_icon_destroy(struct wl_listener *listener, void *data)
{
	struct weston_drag *drag = container_of(listener, struct weston_drag,
						icon_destroy_listener);

	drag->icon = NULL;
}

static void
data_device_start_drag(struct wl_client *client, struct wl_resource *resource,
		       struct wl_resource *source_resource,
		       struct wl_resource *origin_resource,
		       struct wl_resource *icon_resource, uint32_t serial)
{
	struct weston_seat *seat = wl_resource_get_user_data(resource);
	struct weston_drag *drag;
	struct weston_surface *icon = NULL;

	if (seat->pointer->button_count == 0 ||
	    seat->pointer->grab_serial != serial ||
	    seat->pointer->focus != wl_resource_get_user_data(origin_resource))
		return;

	/* FIXME: Check that the data source type array isn't empty. */

	if (icon_resource)
		icon = wl_resource_get_user_data(icon_resource);
	if (icon && icon->configure) {
		wl_resource_post_error(icon_resource,
				       WL_DISPLAY_ERROR_INVALID_OBJECT,
				       "surface->configure already set");
		return;
	}

	drag = zalloc(sizeof *drag);
	if (drag == NULL) {
		wl_resource_post_no_memory(resource);
		return;
	}

	drag->grab.interface = &drag_grab_interface;
	drag->client = client;

	if (source_resource) {
		drag->data_source = wl_resource_get_user_data(source_resource);
		drag->data_source_listener.notify = destroy_data_device_source;
		wl_resource_add_destroy_listener(source_resource,
						 &drag->data_source_listener);
	}

	if (icon) {
		drag->icon = icon;
		drag->icon_destroy_listener.notify = handle_drag_icon_destroy;
		wl_signal_add(&icon->destroy_signal,
			      &drag->icon_destroy_listener);

		icon->configure = drag_surface_configure;
		icon->configure_private = drag;
	}

	weston_pointer_set_focus(seat->pointer, NULL,
				 wl_fixed_from_int(0), wl_fixed_from_int(0));
	weston_pointer_start_grab(seat->pointer, &drag->grab);
}

static void
destroy_selection_data_source(struct wl_listener *listener, void *data)
{
	struct weston_seat *seat = container_of(listener, struct weston_seat,
						selection_data_source_listener);
	struct wl_resource *data_device;
	struct wl_resource *focus = NULL;

	seat->selection_data_source = NULL;

	if (seat->keyboard)
		focus = seat->keyboard->focus_resource;
	if (focus) {
		data_device = wl_resource_find_for_client(&seat->drag_resource_list,
							  wl_resource_get_client(focus));
		if (data_device)
			wl_data_device_send_selection(data_device, NULL);
	}

	wl_signal_emit(&seat->selection_signal, seat);
}

WL_EXPORT void
weston_seat_set_selection(struct weston_seat *seat,
			  struct weston_data_source *source, uint32_t serial)
{
	struct wl_resource *data_device, *offer;
	struct wl_resource *focus = NULL;

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
		focus = seat->keyboard->focus_resource;
	if (focus) {
		data_device = wl_resource_find_for_client(&seat->drag_resource_list,
							  wl_resource_get_client(focus));
		if (data_device && source) {
			offer = weston_data_source_send_offer(seat->selection_data_source,
							      data_device);
			wl_data_device_send_selection(data_device, offer);
		} else if (data_device) {
			wl_data_device_send_selection(data_device, NULL);
		}
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

static const struct wl_data_device_interface data_device_interface = {
	data_device_start_drag,
	data_device_set_selection,
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
				      &wl_data_device_interface, 1, id);
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

	resource =
		wl_resource_create(client,
				   &wl_data_device_manager_interface, 1, id);
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
	struct wl_resource *data_device, *focus, *offer;
	struct weston_data_source *source;

	if (!seat->keyboard)
		return;

	focus = seat->keyboard->focus_resource;
	if (!focus)
		return;

	data_device = wl_resource_find_for_client(&seat->drag_resource_list,
						  wl_resource_get_client(focus));
	if (!data_device)
		return;

	source = seat->selection_data_source;
	if (source) {
		offer = weston_data_source_send_offer(source, data_device);
		wl_data_device_send_selection(data_device, offer);
	}
}

WL_EXPORT int
wl_data_device_manager_init(struct wl_display *display)
{
	if (wl_global_create(display,
			     &wl_data_device_manager_interface, 1,
			     NULL, bind_manager) == NULL)
		return -1;

	return 0;
}
