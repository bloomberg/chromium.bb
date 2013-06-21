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

#include "config.h"

#include <stdlib.h>

#include "compositor.h"

static int
noop_renderer_read_pixels(struct weston_output *output,
			       pixman_format_code_t format, void *pixels,
			       uint32_t x, uint32_t y,
			       uint32_t width, uint32_t height)
{
	return 0;
}

static void
noop_renderer_repaint_output(struct weston_output *output,
			     pixman_region32_t *output_damage)
{
}

static void
noop_renderer_flush_damage(struct weston_surface *surface)
{
}

static void
noop_renderer_attach(struct weston_surface *es, struct weston_buffer *buffer)
{
}

static int
noop_renderer_create_surface(struct weston_surface *surface)
{
	return 0;
}

static void
noop_renderer_surface_set_color(struct weston_surface *surface,
		 float red, float green, float blue, float alpha)
{
}

static void
noop_renderer_destroy_surface(struct weston_surface *surface)
{
}

static void
noop_renderer_destroy(struct weston_compositor *ec)
{
	free(ec->renderer);
	ec->renderer = NULL;
}

WL_EXPORT int
noop_renderer_init(struct weston_compositor *ec)
{
	struct weston_renderer *renderer;

	renderer = malloc(sizeof *renderer);
	if (renderer == NULL)
		return -1;

	renderer->read_pixels = noop_renderer_read_pixels;
	renderer->repaint_output = noop_renderer_repaint_output;
	renderer->flush_damage = noop_renderer_flush_damage;
	renderer->attach = noop_renderer_attach;
	renderer->create_surface = noop_renderer_create_surface;
	renderer->surface_set_color = noop_renderer_surface_set_color;
	renderer->destroy_surface = noop_renderer_destroy_surface;
	renderer->destroy = noop_renderer_destroy;
	ec->renderer = renderer;

	return 0;
}
