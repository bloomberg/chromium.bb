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

#include "compositor.h"

WL_EXPORT void
weston_matrix_init(struct weston_matrix *matrix)
{
	static const struct weston_matrix identity = {
		{ 1, 0, 0, 0,  0, 1, 0, 0,  0, 0, 1, 0,  0, 0, 0, 1 }
	};

	memcpy(matrix, &identity, sizeof identity);
}

static void
weston_matrix_multiply(struct weston_matrix *m, const struct weston_matrix *n)
{
	struct weston_matrix tmp;
	const GLfloat *row, *column;
	div_t d;
	int i, j;

	for (i = 0; i < 16; i++) {
		tmp.d[i] = 0;
		d = div(i, 4);
		row = m->d + d.quot * 4;
		column = n->d + d.rem;
		for (j = 0; j < 4; j++)
			tmp.d[i] += row[j] * column[j * 4];
	}
	memcpy(m, &tmp, sizeof tmp);
}

WL_EXPORT void
weston_matrix_translate(struct weston_matrix *matrix, GLfloat x, GLfloat y, GLfloat z)
{
	struct weston_matrix translate = {
		{ 1, 0, 0, 0,  0, 1, 0, 0,  0, 0, 1, 0,  x, y, z, 1 }
	};

	weston_matrix_multiply(matrix, &translate);
}

WL_EXPORT void
weston_matrix_scale(struct weston_matrix *matrix, GLfloat x, GLfloat y, GLfloat z)
{
	struct weston_matrix scale = {
		{ x, 0, 0, 0,  0, y, 0, 0,  0, 0, z, 0,  0, 0, 0, 1 }
	};

	weston_matrix_multiply(matrix, &scale);
}

WL_EXPORT void
weston_matrix_transform(struct weston_matrix *matrix, struct weston_vector *v)
{
	int i, j;
	struct weston_vector t;

	for (i = 0; i < 4; i++) {
		t.f[i] = 0;
		for (j = 0; j < 4; j++)
			t.f[i] += v->f[j] * matrix->d[i + j * 4];
	}

	*v = t;
}

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

struct weston_zoom {
	struct weston_surface *surface;
	struct weston_animation animation;
	struct weston_spring spring;
	struct weston_transform transform;
	struct wl_listener listener;
	GLfloat start, stop;
	void (*done)(struct weston_zoom *zoom, void *data);
	void *data;
};

static void
weston_zoom_destroy(struct weston_zoom *zoom)
{
	wl_list_remove(&zoom->animation.link);
	wl_list_remove(&zoom->listener.link);
	zoom->surface->transform = NULL;
	if (zoom->done)
		zoom->done(zoom, zoom->data);
	free(zoom);
}

static void
handle_zoom_surface_destroy(struct wl_listener *listener,
			    struct wl_resource *resource, uint32_t time)
{
	struct weston_zoom *zoom =
		container_of(listener, struct weston_zoom, listener);

	weston_zoom_destroy(zoom);
}

static void
weston_zoom_frame(struct weston_animation *animation,
		struct weston_output *output, uint32_t msecs)
{
	struct weston_zoom *zoom =
		container_of(animation, struct weston_zoom, animation);
	struct weston_surface *es = zoom->surface;
	GLfloat scale;

	weston_spring_update(&zoom->spring, msecs);

	if (weston_spring_done(&zoom->spring)) {
		weston_zoom_destroy(zoom);
		return;
	}

	scale = zoom->start +
		(zoom->stop - zoom->start) * zoom->spring.current;
	weston_matrix_init(&zoom->transform.matrix);
	weston_matrix_translate(&zoom->transform.matrix,
			      -(es->x + es->width / 2.0),
			      -(es->y + es->height / 2.0), 0);
	weston_matrix_scale(&zoom->transform.matrix, scale, scale, scale);
	weston_matrix_translate(&zoom->transform.matrix,
			      es->x + es->width / 2.0,
			      es->y + es->height / 2.0, 0);

	es->alpha = zoom->spring.current * 255;
	if (es->alpha > 255)
		es->alpha = 255;
	scale = 1.0 / zoom->spring.current;
	weston_matrix_init(&zoom->transform.inverse);
	weston_matrix_scale(&zoom->transform.inverse, scale, scale, scale);

	weston_compositor_damage_all(es->compositor);
}

WL_EXPORT struct weston_zoom *
weston_zoom_run(struct weston_surface *surface, GLfloat start, GLfloat stop,
	      weston_zoom_done_func_t done, void *data)
{
	struct weston_zoom *zoom;

	zoom = malloc(sizeof *zoom);
	if (!zoom)
		return NULL;

	zoom->surface = surface;
	zoom->done = done;
	zoom->data = data;
	zoom->start = start;
	zoom->stop = stop;
	surface->transform = &zoom->transform;
	weston_spring_init(&zoom->spring, 200.0, 0.0, 1.0);
	zoom->spring.friction = 700;
	zoom->spring.timestamp = weston_compositor_get_time();
	zoom->animation.frame = weston_zoom_frame;
	weston_zoom_frame(&zoom->animation, NULL, zoom->spring.timestamp);

	zoom->listener.func = handle_zoom_surface_destroy;
	wl_list_insert(surface->surface.resource.destroy_listener_list.prev,
		       &zoom->listener.link);

	wl_list_insert(surface->compositor->animation_list.prev,
		       &zoom->animation.link);

	return zoom;
}

struct weston_binding {
	uint32_t key;
	uint32_t button;
	uint32_t modifier;
	weston_binding_handler_t handler;
	void *data;
	struct wl_list link;
};

WL_EXPORT struct weston_binding *
weston_compositor_add_binding(struct weston_compositor *compositor,
			    uint32_t key, uint32_t button, uint32_t modifier,
			    weston_binding_handler_t handler, void *data)
{
	struct weston_binding *binding;

	binding = malloc(sizeof *binding);
	if (binding == NULL)
		return NULL;

	binding->key = key;
	binding->button = button;
	binding->modifier = modifier;
	binding->handler = handler;
	binding->data = data;
	wl_list_insert(compositor->binding_list.prev, &binding->link);

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

WL_EXPORT void
weston_compositor_run_binding(struct weston_compositor *compositor,
			      struct weston_input_device *device,
			      uint32_t time,
			      uint32_t key, uint32_t button, int32_t state)
{
	struct weston_binding *b;

	wl_list_for_each(b, &compositor->binding_list, link) {
		if (b->key == key && b->button == button &&
		    b->modifier == device->modifier_state && state) {
			b->handler(&device->input_device,
				   time, key, button, state, b->data);
			break;
		}
	}
}
