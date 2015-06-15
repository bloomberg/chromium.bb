/*
 * Copyright © 2012 Intel Corporation
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

#include <stdio.h>
#include <assert.h>

#include "src/compositor.h"

static void
surface_transform(void *data)
{
	struct weston_compositor *compositor = data;
	struct weston_surface *surface;
	struct weston_view *view;
	float x, y;

	surface = weston_surface_create(compositor);
	assert(surface);
	view = weston_view_create(surface);
	assert(view);
	surface->width = 200;
	surface->height = 200;
	weston_view_set_position(view, 100, 100);
	weston_view_update_transform(view);
	weston_view_to_global_float(view, 20, 20, &x, &y);

	fprintf(stderr, "20,20 maps to %f, %f\n", x, y);
	assert(x == 120 && y == 120);

	weston_view_set_position(view, 150, 300);
	weston_view_update_transform(view);
	weston_view_to_global_float(view, 50, 40, &x, &y);
	assert(x == 200 && y == 340);

	wl_display_terminate(compositor->wl_display);
}

WL_EXPORT int
module_init(struct weston_compositor *compositor, int *argc, char *argv[])
{
	struct wl_event_loop *loop;

	loop = wl_display_get_event_loop(compositor->wl_display);

	wl_event_loop_add_idle(loop, surface_transform, compositor);

	return 0;
}
