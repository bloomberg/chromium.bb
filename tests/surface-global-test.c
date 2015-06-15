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

#include <assert.h>

#include "src/compositor.h"

static void
surface_to_from_global(void *data)
{
	struct weston_compositor *compositor = data;
	struct weston_surface *surface;
	struct weston_view *view;
	float x, y;
	wl_fixed_t fx, fy;
	int32_t ix, iy;

	surface = weston_surface_create(compositor);
	assert(surface);
	view = weston_view_create(surface);
	assert(view);
	surface->width = 50;
	surface->height = 50;
	weston_view_set_position(view, 5, 10);
	weston_view_update_transform(view);

	weston_view_to_global_float(view, 33, 22, &x, &y);
	assert(x == 38 && y == 32);

	weston_view_to_global_float(view, -8, -2, &x, &y);
	assert(x == -3 && y == 8);

	weston_view_to_global_fixed(view, wl_fixed_from_int(12),
				       wl_fixed_from_int(5), &fx, &fy);
	assert(fx == wl_fixed_from_int(17) && fy == wl_fixed_from_int(15));

	weston_view_from_global_float(view, 38, 32, &x, &y);
	assert(x == 33 && y == 22);

	weston_view_from_global_float(view, 42, 5, &x, &y);
	assert(x == 37 && y == -5);

	weston_view_from_global_fixed(view, wl_fixed_from_int(21),
					 wl_fixed_from_int(100), &fx, &fy);
	assert(fx == wl_fixed_from_int(16) && fy == wl_fixed_from_int(90));

	weston_view_from_global(view, 0, 0, &ix, &iy);
	assert(ix == -5 && iy == -10);

	weston_view_from_global(view, 5, 10, &ix, &iy);
	assert(ix == 0 && iy == 0);

	wl_display_terminate(compositor->wl_display);
}

WL_EXPORT int
module_init(struct weston_compositor *compositor, int *argc, char *argv[])
{
	struct wl_event_loop *loop;

	loop = wl_display_get_event_loop(compositor->wl_display);

	wl_event_loop_add_idle(loop, surface_to_from_global, compositor);

	return 0;
}
