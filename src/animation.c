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

#include "config.h"

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
	weston_surface_geometry_dirty(animation->surface);
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

	weston_surface_geometry_dirty(animation->surface);
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
	wl_signal_add(&surface->destroy_signal, &animation->listener);

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
	if (animation->spring.current > 0.999)
		animation->surface->alpha = 1;
	else if (animation->spring.current < 0.001 )
		animation->surface->alpha = 0;
	else
		animation->surface->alpha = animation->spring.current;
}

WL_EXPORT struct weston_surface_animation *
weston_fade_run(struct weston_surface *surface,
		float start, float end, float k,
		weston_surface_animation_done_func_t done, void *data)
{
	struct weston_surface_animation *fade;

	fade = weston_surface_animation_run(surface, 0, 0,
					    fade_frame, done, data);

	weston_spring_init(&fade->spring, k, start, end);
	surface->alpha = start;

	return fade;
}

WL_EXPORT void
weston_fade_update(struct weston_surface_animation *fade,
		   float start, float end, float k)
{
	weston_spring_init(&fade->spring, k, start, end);
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
