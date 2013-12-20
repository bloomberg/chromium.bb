/*
 * Copyright © 2010-2011 Benjamin Franzke
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
#include <string.h>
#include <sys/time.h>

#include "compositor.h"

struct headless_compositor {
	struct weston_compositor base;
	struct weston_seat fake_seat;
};

struct headless_output {
	struct weston_output base;
	struct weston_mode mode;
	struct wl_event_source *finish_frame_timer;
};


static void
headless_output_start_repaint_loop(struct weston_output *output)
{
	uint32_t msec;
	struct timeval tv;

	gettimeofday(&tv, NULL);
	msec = tv.tv_sec * 1000 + tv.tv_usec / 1000;
	weston_output_finish_frame(output, msec);
}

static int
finish_frame_handler(void *data)
{
	headless_output_start_repaint_loop(data);

	return 1;
}

static int
headless_output_repaint(struct weston_output *output_base,
		       pixman_region32_t *damage)
{
	struct headless_output *output = (struct headless_output *) output_base;
	struct weston_compositor *ec = output->base.compositor;

	ec->renderer->repaint_output(&output->base, damage);

	pixman_region32_subtract(&ec->primary_plane.damage,
				 &ec->primary_plane.damage, damage);

	wl_event_source_timer_update(output->finish_frame_timer, 16);

	return 0;
}

static void
headless_output_destroy(struct weston_output *output_base)
{
	struct headless_output *output = (struct headless_output *) output_base;

	wl_event_source_remove(output->finish_frame_timer);
	free(output);

	return;
}

static int
headless_compositor_create_output(struct headless_compositor *c,
				 int width, int height)
{
	struct headless_output *output;
	struct wl_event_loop *loop;

	output = zalloc(sizeof *output);
	if (output == NULL)
		return -1;

	output->mode.flags =
		WL_OUTPUT_MODE_CURRENT | WL_OUTPUT_MODE_PREFERRED;
	output->mode.width = width;
	output->mode.height = height;
	output->mode.refresh = 60;
	wl_list_init(&output->base.mode_list);
	wl_list_insert(&output->base.mode_list, &output->mode.link);

	output->base.current_mode = &output->mode;
	weston_output_init(&output->base, &c->base, 0, 0, width, height,
			   WL_OUTPUT_TRANSFORM_NORMAL, 1);

	output->base.make = "weston";
	output->base.model = "headless";

	loop = wl_display_get_event_loop(c->base.wl_display);
	output->finish_frame_timer =
		wl_event_loop_add_timer(loop, finish_frame_handler, output);

	output->base.start_repaint_loop = headless_output_start_repaint_loop;
	output->base.repaint = headless_output_repaint;
	output->base.destroy = headless_output_destroy;
	output->base.assign_planes = NULL;
	output->base.set_backlight = NULL;
	output->base.set_dpms = NULL;
	output->base.switch_mode = NULL;

	wl_list_insert(c->base.output_list.prev, &output->base.link);

	return 0;
}

static void
headless_restore(struct weston_compositor *ec)
{
}

static void
headless_destroy(struct weston_compositor *ec)
{
	struct headless_compositor *c = (struct headless_compositor *) ec;

	weston_seat_release(&c->fake_seat);
	weston_compositor_shutdown(ec);

	free(ec);
}

static struct weston_compositor *
headless_compositor_create(struct wl_display *display,
			   int width, int height, const char *display_name,
			   int *argc, char *argv[],
			   struct weston_config *config)
{
	struct headless_compositor *c;

	c = zalloc(sizeof *c);
	if (c == NULL)
		return NULL;

	if (weston_compositor_init(&c->base, display, argc, argv, config) < 0)
		goto err_free;

	weston_seat_init(&c->fake_seat, &c->base, "default");

	c->base.destroy = headless_destroy;
	c->base.restore = headless_restore;

	if (headless_compositor_create_output(c, width, height) < 0)
		goto err_compositor;

	if (noop_renderer_init(&c->base) < 0)
		goto err_compositor;

	return &c->base;

err_compositor:
	weston_compositor_shutdown(&c->base);
err_free:
	free(c);
	return NULL;
}

WL_EXPORT struct weston_compositor *
backend_init(struct wl_display *display, int *argc, char *argv[],
	     struct weston_config *config)
{
	int width = 1024, height = 640;
	char *display_name = NULL;

	const struct weston_option headless_options[] = {
		{ WESTON_OPTION_INTEGER, "width", 0, &width },
		{ WESTON_OPTION_INTEGER, "height", 0, &height },
	};

	parse_options(headless_options,
		      ARRAY_LENGTH(headless_options), argc, argv);

	return headless_compositor_create(display, width, height, display_name,
					  argc, argv, config);
}
