/*
 * Copyright © 2015 Samsung Electronics Co., Ltd
 * Copyright © 2016 Collabora, Ltd.
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
#include <string.h>
#include <sys/mman.h>

#include "weston-test-client-helper.h"

char *server_parameters = "--use-pixman --width=320 --height=240"
	" --shell=weston-test-desktop-shell.so";

static struct wl_subcompositor *
get_subcompositor(struct client *client)
{
	struct global *g;
	struct global *global_sub = NULL;
	struct wl_subcompositor *sub;

	wl_list_for_each(g, &client->global_list, link) {
		if (strcmp(g->interface, "wl_subcompositor"))
			continue;

		if (global_sub)
			assert(0 && "multiple wl_subcompositor objects");

		global_sub = g;
	}

	assert(global_sub && "no wl_subcompositor found");

	assert(global_sub->version == 1);

	sub = wl_registry_bind(client->wl_registry, global_sub->name,
			       &wl_subcompositor_interface, 1);
	assert(sub);

	return sub;
}

static void
fill_color(pixman_image_t *image, pixman_color_t *color)
{
	pixman_image_t *solid;
	int width;
	int height;

	width = pixman_image_get_width(image);
	height = pixman_image_get_height(image);

	solid = pixman_image_create_solid_fill(color);
	pixman_image_composite32(PIXMAN_OP_SRC,
				 solid, /* src */
				 NULL, /* mask */
				 image, /* dst */
				 0, 0, /* src x,y */
				 0, 0, /* mask x,y */
				 0, 0, /* dst x,y */
				 width, height);
	pixman_image_unref(solid);
}

static pixman_color_t *
color(pixman_color_t *tmp, uint8_t r, uint8_t g, uint8_t b)
{
	tmp->alpha = 65535;
	tmp->red = (r << 8) + r;
	tmp->green = (g << 8) + g;
	tmp->blue = (b << 8) + b;

	return tmp;
}

static void
write_visual_diff(pixman_image_t *ref_image,
		  struct buffer *shot,
		  const struct rectangle *clip,
		  const char *test_name,
		  int seq_no)
{
	char *fname;
	char *ext_test_name;
	pixman_image_t *diff;
	int ret;

	ret = asprintf(&ext_test_name, "%s-diff", test_name);
	assert(ret >= 0);

	fname = screenshot_output_filename(ext_test_name, seq_no);
	diff = visualize_image_difference(shot->image, ref_image, clip);
	write_image_as_png(diff, fname);

	pixman_image_unref(diff);
	free(fname);
	free(ext_test_name);
}

static int
check_screen(struct client *client,
	     const char *ref_image,
	     int ref_seq_no,
	     const struct rectangle *clip,
	     int seq_no)
{
	const char *test_name = get_test_name();
	struct buffer *shot;
	pixman_image_t *ref;
	char *ref_fname;
	char *shot_fname;
	bool match;

	ref_fname = screenshot_reference_filename(ref_image, ref_seq_no);
	shot_fname = screenshot_output_filename(test_name, seq_no);

	ref = load_image_from_png(ref_fname);
	assert(ref);

	shot = capture_screenshot_of_output(client);
	assert(shot);

	match = check_images_match(shot->image, ref, clip);
	printf("ref %s vs. shot %s: %s\n", ref_fname, shot_fname,
	       match ? "PASS" : "FAIL");

	write_image_as_png(shot->image, shot_fname);
	if (!match)
		write_visual_diff(ref, shot, clip, test_name, seq_no);

	buffer_destroy(shot);
	pixman_image_unref(ref);
	free(ref_fname);
	free(shot_fname);

	return match ? 0 : -1;
}

static struct buffer *
surface_commit_color(struct client *client, struct wl_surface *surface,
		     pixman_color_t *color, int width, int height)
{
	struct buffer *buf;

	buf = create_shm_buffer_a8r8g8b8(client, width, height);
	fill_color(buf->image, color);
	wl_surface_attach(surface, buf->proxy, 0, 0);
	wl_surface_damage(surface, 0, 0, width, height);
	wl_surface_commit(surface);

	return buf;
}

TEST(subsurface_z_order)
{
	const char *test_name = get_test_name();
	struct client *client;
	struct wl_subcompositor *subco;
	struct buffer *bufs[5] = { 0 };
	struct wl_surface *surf[5] = { 0 };
	struct wl_subsurface *sub[5] = { 0 };
	struct rectangle clip = { 40, 40, 280, 200 };
	int fail = 0;
	unsigned i;
	pixman_color_t red;
	pixman_color_t blue;
	pixman_color_t cyan;
	pixman_color_t green;

	color(&red, 255, 0, 0);
	color(&blue, 0, 0, 255);
	color(&cyan, 0, 255, 255);
	color(&green, 0, 255, 0);

	client = create_client_and_test_surface(100, 50, 100, 100);
	assert(client);
	subco = get_subcompositor(client);

	/* move the pointer clearly away from our screenshooting area */
	weston_test_move_pointer(client->test->weston_test, 2, 30);

	/* make the parent surface red */
	surf[0] = client->surface->wl_surface;
	bufs[0] = surface_commit_color(client, surf[0], &red, 100, 100);
	/* sub[0] is not used */

	fail += check_screen(client, test_name, 0, &clip, 0);

	/* create a blue sub-surface above red */
	surf[1] = wl_compositor_create_surface(client->wl_compositor);
	sub[1] = wl_subcompositor_get_subsurface(subco, surf[1], surf[0]);
	bufs[1] = surface_commit_color(client, surf[1], &blue, 100, 100);

	wl_subsurface_set_position(sub[1], 20, 20);
	wl_surface_commit(surf[0]);

	fail += check_screen(client, test_name, 1, &clip, 1);

	/* create a cyan sub-surface above blue */
	surf[2] = wl_compositor_create_surface(client->wl_compositor);
	sub[2] = wl_subcompositor_get_subsurface(subco, surf[2], surf[1]);
	bufs[2] = surface_commit_color(client, surf[2], &cyan, 100, 100);

	wl_subsurface_set_position(sub[2], 20, 20);
	wl_surface_commit(surf[1]);
	wl_surface_commit(surf[0]);

	fail += check_screen(client, test_name, 2, &clip, 2);

	/* create a green sub-surface above blue, sibling to cyan */
	surf[3] = wl_compositor_create_surface(client->wl_compositor);
	sub[3] = wl_subcompositor_get_subsurface(subco, surf[3], surf[1]);
	bufs[3] = surface_commit_color(client, surf[3], &green, 100, 100);

	wl_subsurface_set_position(sub[3], -40, 10);
	wl_surface_commit(surf[1]);
	wl_surface_commit(surf[0]);

	fail += check_screen(client, test_name, 3, &clip, 3);

	/* stack blue below red, which brings also cyan and green below red */
	wl_subsurface_place_below(sub[1], surf[0]);
	wl_surface_commit(surf[0]);

	fail += check_screen(client, test_name, 4, &clip, 4);

	assert(fail == 0);

	for (i = 0; i < ARRAY_LENGTH(sub); i++)
		if (sub[i])
			wl_subsurface_destroy(sub[i]);

	for (i = 0; i < ARRAY_LENGTH(surf); i++)
		if (surf[i])
			wl_surface_destroy(surf[i]);

	for (i = 0; i < ARRAY_LENGTH(bufs); i++)
		if (bufs[i])
			buffer_destroy(bufs[i]);
}
