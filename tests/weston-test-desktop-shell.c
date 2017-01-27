/*
 * Copyright © 2010-2012 Intel Corporation
 * Copyright © 2011-2012 Collabora, Ltd.
 * Copyright © 2013 Raspberry Pi Foundation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#include "config.h"

#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <linux/input.h>
#include <assert.h>
#include <signal.h>
#include <math.h>
#include <sys/types.h>

#include "compositor/weston.h"
#include "shared/config-parser.h"
#include "shared/helpers.h"
#include "libweston-desktop/libweston-desktop.h"

#ifndef static_assert
#define static_assert(cond, msg)
#endif

static struct weston_desktop *desktop = NULL;
static struct weston_layer background_layer;
static struct weston_surface *background_surface = NULL;
static struct weston_view *background_view = NULL;
static struct weston_layer layer;
static struct weston_view *view = NULL;
/*
 * libweston-desktop
 */

static void
desktop_surface_added(struct weston_desktop_surface *desktop_surface,
		      void *shell)
{

	assert(!view);

	view = weston_desktop_surface_create_view(desktop_surface);

	assert(view);
}

static void
desktop_surface_removed(struct weston_desktop_surface *desktop_surface,
			void *shell)
{
	assert(view);

	weston_desktop_surface_unlink_view(view);
	weston_view_destroy(view);
	view = NULL;
}

static void
desktop_surface_committed(struct weston_desktop_surface *desktop_surface,
			  int32_t sx, int32_t sy, void *data)
{
	struct weston_surface *surface =
		weston_desktop_surface_get_surface(desktop_surface);
	struct weston_geometry geometry =
		weston_desktop_surface_get_geometry(desktop_surface);

	assert(view);

	if (weston_surface_is_mapped(surface))
		return;

	surface->is_mapped = true;
	weston_layer_entry_insert(&layer.view_list, &view->layer_link);
	weston_view_set_position(view, 0 - geometry.x, 0 - geometry.y);
	weston_view_update_transform(view);
	view->is_mapped = true;
}

static void
desktop_surface_move(struct weston_desktop_surface *desktop_surface,
		     struct weston_seat *seat, uint32_t serial, void *shell)
{
}

static void
desktop_surface_resize(struct weston_desktop_surface *desktop_surface,
		       struct weston_seat *seat, uint32_t serial,
		       enum weston_desktop_surface_edge edges, void *shell)
{
}

static void
desktop_surface_fullscreen_requested(struct weston_desktop_surface *desktop_surface,
				     bool fullscreen,
				     struct weston_output *output, void *shell)
{
}

static void
desktop_surface_maximized_requested(struct weston_desktop_surface *desktop_surface,
				    bool maximized, void *shell)
{
}

static void
desktop_surface_minimized_requested(struct weston_desktop_surface *desktop_surface,
				    void *shell)
{
}

static void
desktop_surface_ping_timeout(struct weston_desktop_client *desktop_client,
			     void *shell)
{
}

static void
desktop_surface_pong(struct weston_desktop_client *desktop_client,
		     void *shell)
{
}

static const struct weston_desktop_api shell_desktop_api = {
	.struct_size = sizeof(struct weston_desktop_api),
	.surface_added = desktop_surface_added,
	.surface_removed = desktop_surface_removed,
	.committed = desktop_surface_committed,
	.move = desktop_surface_move,
	.resize = desktop_surface_resize,
	.fullscreen_requested = desktop_surface_fullscreen_requested,
	.maximized_requested = desktop_surface_maximized_requested,
	.minimized_requested = desktop_surface_minimized_requested,
	.ping_timeout = desktop_surface_ping_timeout,
	.pong = desktop_surface_pong,
};

/* ************************ *
 * end of libweston-desktop *
 * ************************ */

WL_EXPORT int
wet_shell_init(struct weston_compositor *ec,
	       int *argc, char *argv[])
{
	weston_layer_init(&layer, ec);
	weston_layer_init(&background_layer, ec);

	weston_layer_set_position(&layer,
				  WESTON_LAYER_POSITION_NORMAL);
	weston_layer_set_position(&background_layer,
				  WESTON_LAYER_POSITION_BACKGROUND);

	background_surface = weston_surface_create(ec);
	if (background_surface == NULL)
		return -1;
	background_view = weston_view_create(background_surface);
	if (background_view == NULL) {
		weston_surface_destroy(background_surface);
		return -1;
	}

	weston_surface_set_color(background_surface, 0.0, 0.0, 0.0, 1);
	pixman_region32_fini(&background_surface->opaque);
	pixman_region32_init_rect(&background_surface->opaque, 0, 0, 2000, 2000);
	pixman_region32_fini(&background_surface->input);
	pixman_region32_init_rect(&background_surface->input, 0, 0, 2000, 2000);

	weston_surface_set_size(background_surface, 2000, 2000);
	weston_view_set_position(background_view, 0, 0);
	weston_layer_entry_insert(&background_layer.view_list, &background_view->layer_link);
	weston_view_update_transform(background_view);

	desktop = weston_desktop_create(ec, &shell_desktop_api, NULL);
	return desktop ? 0 : -1;
}
