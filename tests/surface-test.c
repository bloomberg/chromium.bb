/*
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
#include <stdio.h>
#include <assert.h>

#include "../src/compositor.h"

static void
surface_transform(void *data)
{
	struct weston_compositor *compositor = data;
	struct weston_surface *surface;
	float x, y;

	surface = weston_surface_create(compositor);
	assert(surface);
	weston_surface_configure(surface, 100, 100, 200, 200);
	weston_surface_update_transform(surface);
	weston_surface_to_global_float(surface, 20, 20, &x, &y);

	fprintf(stderr, "20,20 maps to %f, %f\n", x, y);
	assert(x == 120 && y == 120);

	weston_surface_set_position(surface, 150, 300);
	weston_surface_update_transform(surface);
	weston_surface_to_global_float(surface, 50, 40, &x, &y);
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
