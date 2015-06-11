/*
 * Copyright © 2012 Scott Moreau
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

#include "compositor.h"
#include "text-cursor-position-server-protocol.h"

static void
weston_zoom_frame_z(struct weston_animation *animation,
		struct weston_output *output, uint32_t msecs)
{
	if (animation->frame_counter <= 1)
		output->zoom.spring_z.timestamp = msecs;

	weston_spring_update(&output->zoom.spring_z, msecs);

	if (output->zoom.spring_z.current > output->zoom.max_level)
		output->zoom.spring_z.current = output->zoom.max_level;
	else if (output->zoom.spring_z.current < 0.0)
		output->zoom.spring_z.current = 0.0;

	if (weston_spring_done(&output->zoom.spring_z)) {
		if (output->zoom.active && output->zoom.level <= 0.0) {
			output->zoom.active = 0;
			output->disable_planes--;
			wl_list_remove(&output->zoom.motion_listener.link);
		}
		output->zoom.spring_z.current = output->zoom.level;
		wl_list_remove(&animation->link);
		wl_list_init(&animation->link);
	}

	output->dirty = 1;
	weston_output_damage(output);
}

static struct weston_seat *
weston_zoom_pick_seat(struct weston_compositor *compositor)
{
	return container_of(compositor->seat_list.next,
			    struct weston_seat, link);
}

static void
zoom_area_center_from_pointer(struct weston_output *output,
				wl_fixed_t *x, wl_fixed_t *y)
{
	float level = output->zoom.spring_z.current;
	wl_fixed_t offset_x = wl_fixed_from_int(output->x);
	wl_fixed_t offset_y = wl_fixed_from_int(output->y);
	wl_fixed_t w = wl_fixed_from_int(output->width);
	wl_fixed_t h = wl_fixed_from_int(output->height);

	*x = (*x - offset_x) * level + w / 2;
	*y = (*y - offset_y) * level + h / 2;
}

static void
weston_output_update_zoom_transform(struct weston_output *output)
{
	float global_x, global_y;
	wl_fixed_t x = output->zoom.current.x; /* global pointer coords */
	wl_fixed_t y = output->zoom.current.y;
	float level;

	level = output->zoom.spring_z.current;

	if (!output->zoom.active || level > output->zoom.max_level ||
	    level == 0.0f)
		return;

	zoom_area_center_from_pointer(output, &x, &y);

	global_x = wl_fixed_to_double(x);
	global_y = wl_fixed_to_double(y);

	output->zoom.trans_x = global_x - output->width / 2;
	output->zoom.trans_y = global_y - output->height / 2;

	if (output->zoom.trans_x < 0)
		output->zoom.trans_x = 0;
	if (output->zoom.trans_y < 0)
		output->zoom.trans_y = 0;
	if (output->zoom.trans_x > level * output->width)
		output->zoom.trans_x = level * output->width;
	if (output->zoom.trans_y > level * output->height)
		output->zoom.trans_y = level * output->height;
}

static void
weston_zoom_transition(struct weston_output *output)
{
	if (output->zoom.level != output->zoom.spring_z.current) {
		output->zoom.spring_z.target = output->zoom.level;
		if (wl_list_empty(&output->zoom.animation_z.link)) {
			output->zoom.animation_z.frame_counter = 0;
			wl_list_insert(output->animation_list.prev,
				&output->zoom.animation_z.link);
		}
	}

	output->dirty = 1;
	weston_output_damage(output);
}

WL_EXPORT void
weston_output_update_zoom(struct weston_output *output)
{
	struct weston_seat *seat = weston_zoom_pick_seat(output->compositor);

	output->zoom.current.x = seat->pointer->x;
	output->zoom.current.y = seat->pointer->y;

	weston_zoom_transition(output);
	weston_output_update_zoom_transform(output);
}

static void
motion(struct wl_listener *listener, void *data)
{
	struct weston_output_zoom *zoom =
		container_of(listener, struct weston_output_zoom, motion_listener);
	struct weston_output *output =
		container_of(zoom, struct weston_output, zoom);

	weston_output_update_zoom(output);
}

WL_EXPORT void
weston_output_activate_zoom(struct weston_output *output)
{
	struct weston_seat *seat = weston_zoom_pick_seat(output->compositor);

	if (output->zoom.active)
		return;

	output->zoom.active = 1;
	output->disable_planes++;
	wl_signal_add(&seat->pointer->motion_signal,
		      &output->zoom.motion_listener);
}

WL_EXPORT void
weston_output_init_zoom(struct weston_output *output)
{
	output->zoom.active = 0;
	output->zoom.increment = 0.07;
	output->zoom.max_level = 0.95;
	output->zoom.level = 0.0;
	output->zoom.trans_x = 0.0;
	output->zoom.trans_y = 0.0;
	weston_spring_init(&output->zoom.spring_z, 250.0, 0.0, 0.0);
	output->zoom.spring_z.friction = 1000;
	output->zoom.animation_z.frame = weston_zoom_frame_z;
	wl_list_init(&output->zoom.animation_z.link);
	output->zoom.motion_listener.notify = motion;
}
