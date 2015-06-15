/*
 * Copyright © 2010-2011 Benjamin Franzke
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

#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <stdbool.h>

#include "shared/helpers.h"
#include "compositor.h"
#include "pixman-renderer.h"
#include "presentation_timing-server-protocol.h"

struct headless_compositor {
	struct weston_compositor base;
	struct weston_seat fake_seat;
	bool use_pixman;
};

struct headless_output {
	struct weston_output base;
	struct weston_mode mode;
	struct wl_event_source *finish_frame_timer;
	uint32_t *image_buf;
	pixman_image_t *image;
};

struct headless_parameters {
	int width;
	int height;
	int use_pixman;
	uint32_t transform;
};

static void
headless_output_start_repaint_loop(struct weston_output *output)
{
	struct timespec ts;

	weston_compositor_read_presentation_clock(output->compositor, &ts);
	weston_output_finish_frame(output, &ts, PRESENTATION_FEEDBACK_INVALID);
}

static int
finish_frame_handler(void *data)
{
	struct headless_output *output = data;
	struct timespec ts;

	weston_compositor_read_presentation_clock(output->base.compositor, &ts);
	weston_output_finish_frame(&output->base, &ts, 0);

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
	struct headless_compositor *c =
			(struct headless_compositor *) output->base.compositor;

	wl_event_source_remove(output->finish_frame_timer);

	if (c->use_pixman) {
		pixman_renderer_output_destroy(&output->base);
		pixman_image_unref(output->image);
		free(output->image_buf);
	}

	weston_output_destroy(&output->base);

	free(output);

	return;
}

static int
headless_compositor_create_output(struct headless_compositor *c,
				  struct headless_parameters *param)
{
	struct headless_output *output;
	struct wl_event_loop *loop;

	output = zalloc(sizeof *output);
	if (output == NULL)
		return -1;

	output->mode.flags =
		WL_OUTPUT_MODE_CURRENT | WL_OUTPUT_MODE_PREFERRED;
	output->mode.width = param->width;
	output->mode.height = param->height;
	output->mode.refresh = 60000;
	wl_list_init(&output->base.mode_list);
	wl_list_insert(&output->base.mode_list, &output->mode.link);

	output->base.current_mode = &output->mode;
	weston_output_init(&output->base, &c->base, 0, 0, param->width,
			   param->height, param->transform, 1);

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

	if (c->use_pixman) {
		output->image_buf = malloc(param->width * param->height * 4);
		if (!output->image_buf)
			return -1;

		output->image = pixman_image_create_bits(PIXMAN_x8r8g8b8,
							 param->width,
							 param->height,
							 output->image_buf,
							 param->width * 4);

		if (pixman_renderer_output_create(&output->base) < 0)
			return -1;

		pixman_renderer_output_set_buffer(&output->base,
						  output->image);
	}

	weston_compositor_add_output(&c->base, &output->base);

	return 0;
}

static int
headless_input_create(struct headless_compositor *c)
{
	weston_seat_init(&c->fake_seat, &c->base, "default");

	weston_seat_init_pointer(&c->fake_seat);

	if (weston_seat_init_keyboard(&c->fake_seat, NULL) < 0)
		return -1;

	return 0;
}

static void
headless_input_destroy(struct headless_compositor *c)
{
	weston_seat_release(&c->fake_seat);
}

static void
headless_restore(struct weston_compositor *ec)
{
}

static void
headless_destroy(struct weston_compositor *ec)
{
	struct headless_compositor *c = (struct headless_compositor *) ec;

	headless_input_destroy(c);
	weston_compositor_shutdown(ec);

	free(ec);
}

static struct weston_compositor *
headless_compositor_create(struct wl_display *display,
			   struct headless_parameters *param,
			   const char *display_name,
			   int *argc, char *argv[],
			   struct weston_config *config)
{
	struct headless_compositor *c;

	c = zalloc(sizeof *c);
	if (c == NULL)
		return NULL;

	if (weston_compositor_init(&c->base, display, argc, argv, config) < 0)
		goto err_free;

	if (weston_compositor_set_presentation_clock_software(&c->base) < 0)
		goto err_compositor;

	if (headless_input_create(c) < 0)
		goto err_compositor;

	c->base.destroy = headless_destroy;
	c->base.restore = headless_restore;

	c->use_pixman = param->use_pixman;
	if (c->use_pixman) {
		pixman_renderer_init(&c->base);
	}
	if (headless_compositor_create_output(c, param) < 0)
		goto err_input;

	if (!c->use_pixman && noop_renderer_init(&c->base) < 0)
		goto err_input;

	return &c->base;

err_input:
	headless_input_destroy(c);
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
	struct headless_parameters param = { 0, };
	const char *transform = "normal";

	const struct weston_option headless_options[] = {
		{ WESTON_OPTION_INTEGER, "width", 0, &width },
		{ WESTON_OPTION_INTEGER, "height", 0, &height },
		{ WESTON_OPTION_BOOLEAN, "use-pixman", 0, &param.use_pixman },
		{ WESTON_OPTION_STRING, "transform", 0, &transform },
	};

	parse_options(headless_options,
		      ARRAY_LENGTH(headless_options), argc, argv);

	param.width = width;
	param.height = height;

	if (weston_parse_transform(transform, &param.transform) < 0)
		weston_log("Invalid transform \"%s\"\n", transform);

	return headless_compositor_create(display, &param, display_name,
					  argc, argv, config);
}
