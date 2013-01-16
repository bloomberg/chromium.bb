/*
 * Copyright © 2011 Intel Corporation
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
#include <string.h>
#include <stdio.h>
#include <math.h>

#include <unistd.h>
#include <fcntl.h>

#include "compositor.h"

WL_EXPORT void
weston_spring_init(struct weston_spring *spring,
		 double k, double current, double target)
{
	spring->k = k;
	spring->friction = 400.0;
	spring->current = current;
	spring->previous = current;
	spring->target = target;
}

WL_EXPORT void
weston_spring_update(struct weston_spring *spring, uint32_t msec)
{
	double force, v, current, step;

	/* Limit the number of executions of the loop below by ensuring that
	 * the timestamp for last update of the spring is no more than 1s ago.
	 * This handles the case where time moves backwards or forwards in
	 * large jumps.
	 */
	if (msec - spring->timestamp > 1000) {
		weston_log("unexpectedly large timestamp jump (from %u to %u)\n",
			   spring->timestamp, msec);
		spring->timestamp = msec - 1000;
	}

	step = 0.01;
	while (4 < msec - spring->timestamp) {
		current = spring->current;
		v = current - spring->previous;
		force = spring->k * (spring->target - current) / 10.0 +
			(spring->previous - current) - v * spring->friction;

		spring->current =
			current + (current - spring->previous) +
			force * step * step;
		spring->previous = current;

#if 0
		if (spring->current >= 1.0) {
#ifdef TWEENER_BOUNCE
			spring->current = 2.0 - spring->current;
			spring->previous = 2.0 - spring->previous;
#else
			spring->current = 1.0;
			spring->previous = 1.0;
#endif
		}

		if (spring->current <= 0.0) {
			spring->current = 0.0;
			spring->previous = 0.0;
		}
#endif
		spring->timestamp += 4;
	}
}

WL_EXPORT int
weston_spring_done(struct weston_spring *spring)
{
	return fabs(spring->previous - spring->target) < 0.0002 &&
		fabs(spring->current - spring->target) < 0.0002;
}

typedef	void (*weston_surface_animation_frame_func_t)(struct weston_surface_animation *animation);

struct weston_surface_animation {
	struct weston_surface *surface;
	struct weston_animation animation;
	struct weston_spring spring;
	struct weston_transform transform;
	struct wl_listener listener;
	float start, stop;
	weston_surface_animation_frame_func_t frame;
	weston_surface_animation_done_func_t done;
	void *data;
};

static void
weston_surface_animation_destroy(struct weston_surface_animation *animation)
{
	wl_list_remove(&animation->animation.link);
	wl_list_remove(&animation->listener.link);
	wl_list_remove(&animation->transform.link);
	animation->surface->geometry.dirty = 1;
	if (animation->done)
		animation->done(animation, animation->data);
	free(animation);
}

static void
handle_animation_surface_destroy(struct wl_listener *listener, void *data)
{
	struct weston_surface_animation *animation =
		container_of(listener,
			     struct weston_surface_animation, listener);

	weston_surface_animation_destroy(animation);
}

static void
weston_surface_animation_frame(struct weston_animation *base,
			       struct weston_output *output, uint32_t msecs)
{
	struct weston_surface_animation *animation =
		container_of(base,
			     struct weston_surface_animation, animation);

	if (base->frame_counter <= 1)
		animation->spring.timestamp = msecs;

	weston_spring_update(&animation->spring, msecs);

	if (weston_spring_done(&animation->spring)) {
		weston_surface_animation_destroy(animation);
		return;
	}

	if (animation->frame)
		animation->frame(animation);

	animation->surface->geometry.dirty = 1;
	weston_compositor_schedule_repaint(animation->surface->compositor);
}

static struct weston_surface_animation *
weston_surface_animation_run(struct weston_surface *surface,
			     float start, float stop,
			     weston_surface_animation_frame_func_t frame,
			     weston_surface_animation_done_func_t done,
			     void *data)
{
	struct weston_surface_animation *animation;

	animation = malloc(sizeof *animation);
	if (!animation)
		return NULL;

	animation->surface = surface;
	animation->frame = frame;
	animation->done = done;
	animation->data = data;
	animation->start = start;
	animation->stop = stop;
	weston_matrix_init(&animation->transform.matrix);
	wl_list_insert(&surface->geometry.transformation_list,
		       &animation->transform.link);
	weston_spring_init(&animation->spring, 200.0, 0.0, 1.0);
	animation->spring.friction = 700;
	animation->animation.frame_counter = 0;
	animation->animation.frame = weston_surface_animation_frame;
	weston_surface_animation_frame(&animation->animation, NULL, 0);

	animation->listener.notify = handle_animation_surface_destroy;
	wl_signal_add(&surface->surface.resource.destroy_signal,
		      &animation->listener);

	wl_list_insert(&surface->output->animation_list,
		       &animation->animation.link);

	return animation;
}

static void
zoom_frame(struct weston_surface_animation *animation)
{
	struct weston_surface *es = animation->surface;
	float scale;

	scale = animation->start +
		(animation->stop - animation->start) *
		animation->spring.current;
	weston_matrix_init(&animation->transform.matrix);
	weston_matrix_translate(&animation->transform.matrix,
				-0.5f * es->geometry.width,
				-0.5f * es->geometry.height, 0);
	weston_matrix_scale(&animation->transform.matrix, scale, scale, scale);
	weston_matrix_translate(&animation->transform.matrix,
				0.5f * es->geometry.width,
				0.5f * es->geometry.height, 0);

	es->alpha = animation->spring.current;
	if (es->alpha > 1.0)
		es->alpha = 1.0;
}

WL_EXPORT struct weston_surface_animation *
weston_zoom_run(struct weston_surface *surface, float start, float stop,
		weston_surface_animation_done_func_t done, void *data)
{
	return weston_surface_animation_run(surface, start, stop,
					    zoom_frame, done, data);
}

static void
fade_frame(struct weston_surface_animation *animation)
{
	if (animation->spring.current > 1)
		animation->surface->alpha = 1;
	else if (animation->spring.current < 0 )
		animation->surface->alpha = 0;
	else
		animation->surface->alpha = animation->spring.current;
}

WL_EXPORT struct weston_surface_animation *
weston_fade_run(struct weston_surface *surface,
		weston_surface_animation_done_func_t done, void *data)
{
	return weston_surface_animation_run(surface, 0, 0,
					    fade_frame, done, data);
}

static void
slide_frame(struct weston_surface_animation *animation)
{
	float scale;

	scale = animation->start +
		(animation->stop - animation->start) *
		animation->spring.current;
	weston_matrix_init(&animation->transform.matrix);
	weston_matrix_translate(&animation->transform.matrix, 0, scale, 0);
}

WL_EXPORT struct weston_surface_animation *
weston_slide_run(struct weston_surface *surface, float start, float stop,
		weston_surface_animation_done_func_t done, void *data)
{
	struct weston_surface_animation *animation;

	animation = weston_surface_animation_run(surface, start, stop,
						 slide_frame, done, data);
	if (!animation)
		return NULL;

	animation->spring.friction = 900;
	animation->spring.k = 300;

	return animation;
}

struct weston_binding {
	uint32_t key;
	uint32_t button;
	uint32_t axis;
	uint32_t modifier;
	void *handler;
	void *data;
	struct wl_list link;
};

static struct weston_binding *
weston_compositor_add_binding(struct weston_compositor *compositor,
			      uint32_t key, uint32_t button, uint32_t axis,
			      uint32_t modifier, void *handler, void *data)
{
	struct weston_binding *binding;

	binding = malloc(sizeof *binding);
	if (binding == NULL)
		return NULL;

	binding->key = key;
	binding->button = button;
	binding->axis = axis;
	binding->modifier = modifier;
	binding->handler = handler;
	binding->data = data;

	return binding;
}

WL_EXPORT struct weston_binding *
weston_compositor_add_key_binding(struct weston_compositor *compositor,
				  uint32_t key, uint32_t modifier,
				  weston_key_binding_handler_t handler,
				  void *data)
{
	struct weston_binding *binding;

	binding = weston_compositor_add_binding(compositor, key, 0, 0,
						modifier, handler, data);
	if (binding == NULL)
		return NULL;

	wl_list_insert(compositor->key_binding_list.prev, &binding->link);

	return binding;
}

WL_EXPORT struct weston_binding *
weston_compositor_add_button_binding(struct weston_compositor *compositor,
				     uint32_t button, uint32_t modifier,
				     weston_button_binding_handler_t handler,
				     void *data)
{
	struct weston_binding *binding;

	binding = weston_compositor_add_binding(compositor, 0, button, 0,
						modifier, handler, data);
	if (binding == NULL)
		return NULL;

	wl_list_insert(compositor->button_binding_list.prev, &binding->link);

	return binding;
}

WL_EXPORT struct weston_binding *
weston_compositor_add_axis_binding(struct weston_compositor *compositor,
				   uint32_t axis, uint32_t modifier,
				   weston_axis_binding_handler_t handler,
				   void *data)
{
	struct weston_binding *binding;

	binding = weston_compositor_add_binding(compositor, 0, 0, axis,
						modifier, handler, data);
	if (binding == NULL)
		return NULL;

	wl_list_insert(compositor->axis_binding_list.prev, &binding->link);

	return binding;
}

WL_EXPORT struct weston_binding *
weston_compositor_add_debug_binding(struct weston_compositor *compositor,
				    uint32_t key,
				    weston_key_binding_handler_t handler,
				    void *data)
{
	struct weston_binding *binding;

	binding = weston_compositor_add_binding(compositor, key, 0, 0, 0,
						handler, data);

	wl_list_insert(compositor->debug_binding_list.prev, &binding->link);

	return binding;
}

WL_EXPORT void
weston_binding_destroy(struct weston_binding *binding)
{
	wl_list_remove(&binding->link);
	free(binding);
}

WL_EXPORT void
weston_binding_list_destroy_all(struct wl_list *list)
{
	struct weston_binding *binding, *tmp;

	wl_list_for_each_safe(binding, tmp, list, link)
		weston_binding_destroy(binding);
}

struct binding_keyboard_grab {
	uint32_t key;
	struct wl_keyboard_grab grab;
};

static void
binding_key(struct wl_keyboard_grab *grab,
	    uint32_t time, uint32_t key, uint32_t state_w)
{
	struct binding_keyboard_grab *b =
		container_of(grab, struct binding_keyboard_grab, grab);
	struct wl_resource *resource;
	struct wl_display *display;
	enum wl_keyboard_key_state state = state_w;
	uint32_t serial;
	struct weston_keyboard *keyboard = (struct weston_keyboard *)grab->keyboard;

	resource = grab->keyboard->focus_resource;
	if (key == b->key) {
		if (state == WL_KEYBOARD_KEY_STATE_RELEASED) {
			wl_keyboard_end_grab(grab->keyboard);
			if (keyboard->input_method_resource)
				keyboard->keyboard.grab = &keyboard->input_method_grab;
			free(b);
		}
	} else if (resource) {
		display = wl_client_get_display(resource->client);
		serial = wl_display_next_serial(display);
		wl_keyboard_send_key(resource, serial, time, key, state);
	}
}

static void
binding_modifiers(struct wl_keyboard_grab *grab, uint32_t serial,
		  uint32_t mods_depressed, uint32_t mods_latched,
		  uint32_t mods_locked, uint32_t group)
{
	struct wl_resource *resource;

	resource = grab->keyboard->focus_resource;
	if (!resource)
		return;

	wl_keyboard_send_modifiers(resource, serial, mods_depressed,
				   mods_latched, mods_locked, group);
}

static const struct wl_keyboard_grab_interface binding_grab = {
	binding_key,
	binding_modifiers,
};

static void
install_binding_grab(struct wl_seat *seat,
		     uint32_t time, uint32_t key)
{
	struct binding_keyboard_grab *grab;

	grab = malloc(sizeof *grab);
	grab->key = key;
	grab->grab.interface = &binding_grab;
	wl_keyboard_start_grab(seat->keyboard, &grab->grab);
}

WL_EXPORT void
weston_compositor_run_key_binding(struct weston_compositor *compositor,
				  struct weston_seat *seat,
				  uint32_t time, uint32_t key,
				  enum wl_keyboard_key_state state)
{
	struct weston_binding *b;

	if (state == WL_KEYBOARD_KEY_STATE_RELEASED)
		return;

	wl_list_for_each(b, &compositor->key_binding_list, link) {
		if (b->key == key && b->modifier == seat->modifier_state) {
			weston_key_binding_handler_t handler = b->handler;
			handler(&seat->seat, time, key, b->data);

			/* If this was a key binding and it didn't
			 * install a keyboard grab, install one now to
			 * swallow the key release. */
			if (seat->seat.keyboard->grab ==
			    &seat->seat.keyboard->default_grab)
				install_binding_grab(&seat->seat, time, key);
		}
	}
}

WL_EXPORT void
weston_compositor_run_button_binding(struct weston_compositor *compositor,
				     struct weston_seat *seat,
				     uint32_t time, uint32_t button,
				     enum wl_pointer_button_state state)
{
	struct weston_binding *b;

	if (state == WL_POINTER_BUTTON_STATE_RELEASED)
		return;

	wl_list_for_each(b, &compositor->button_binding_list, link) {
		if (b->button == button && b->modifier == seat->modifier_state) {
			weston_button_binding_handler_t handler = b->handler;
			handler(&seat->seat, time, button, b->data);
		}
	}
}

WL_EXPORT void
weston_compositor_run_axis_binding(struct weston_compositor *compositor,
				   struct weston_seat *seat,
				   uint32_t time, uint32_t axis,
				   wl_fixed_t value)
{
	struct weston_binding *b;

	wl_list_for_each(b, &compositor->axis_binding_list, link) {
		if (b->axis == axis && b->modifier == seat->modifier_state) {
			weston_axis_binding_handler_t handler = b->handler;
			handler(&seat->seat, time, axis, value, b->data);
		}
	}
}

WL_EXPORT int
weston_compositor_run_debug_binding(struct weston_compositor *compositor,
				    struct weston_seat *seat,
				    uint32_t time, uint32_t key,
				    enum wl_keyboard_key_state state)
{
	weston_key_binding_handler_t handler;
	struct weston_binding *binding;
	int count = 0;

	wl_list_for_each(binding, &compositor->debug_binding_list, link) {
		if (key != binding->key)
			continue;

		count++;
		handler = binding->handler;
		handler(&seat->seat, time, key, binding->data);
	}

	return count;
}

WL_EXPORT int
weston_environment_get_fd(const char *env)
{
	char *e, *end;
	int fd, flags;

	e = getenv(env);
	if (!e)
		return -1;
	fd = strtol(e, &end, 0);
	if (*end != '\0')
		return -1;

	flags = fcntl(fd, F_GETFD);
	if (flags == -1)
		return -1;

	fcntl(fd, F_SETFD, flags | FD_CLOEXEC);
	unsetenv(env);

	return fd;
}

WL_EXPORT void
weston_transformed_coord(int width, int height,
			 enum wl_output_transform transform,
			 float sx, float sy, float *bx, float *by)
{
	switch (transform) {
	case WL_OUTPUT_TRANSFORM_NORMAL:
	default:
		*bx = sx;
		*by = sy;
		break;
	case WL_OUTPUT_TRANSFORM_FLIPPED:
		*bx = width - sx;
		*by = sy;
		break;
	case WL_OUTPUT_TRANSFORM_90:
		*bx = height - sy;
		*by = sx;
		break;
	case WL_OUTPUT_TRANSFORM_FLIPPED_90:
		*bx = height - sy;
		*by = width - sx;
		break;
	case WL_OUTPUT_TRANSFORM_180:
		*bx = width - sx;
		*by = height - sy;
		break;
	case WL_OUTPUT_TRANSFORM_FLIPPED_180:
		*bx = sx;
		*by = height - sy;
		break;
	case WL_OUTPUT_TRANSFORM_270:
		*bx = sy;
		*by = width - sx;
		break;
	case WL_OUTPUT_TRANSFORM_FLIPPED_270:
		*bx = sy;
		*by = sx;
		break;
	}
}

WL_EXPORT pixman_box32_t
weston_transformed_rect(int width, int height,
			enum wl_output_transform transform,
			pixman_box32_t rect)
{
	float x1, x2, y1, y2;

	pixman_box32_t ret;

	weston_transformed_coord(width, height, transform,
				 rect.x1, rect.y1, &x1, &y1);
	weston_transformed_coord(width, height, transform,
				 rect.x2, rect.y2, &x2, &y2);

	if (x1 <= x2) {
		ret.x1 = x1;
		ret.x2 = x2;
	} else {
		ret.x1 = x2;
		ret.x2 = x1;
	}

	if (y1 <= y2) {
		ret.y1 = y1;
		ret.y2 = y2;
	} else {
		ret.y1 = y2;
		ret.y2 = y1;
	}

	return ret;
}
