/*
 * Copyright © 2015 Samsung Electronics Co., Ltd
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

#include "weston-test-client-helper.h"

char *server_parameters="--use-pixman --width=320 --height=240";

static void
draw_stuff(void *pixels, int w, int h)
{
	int x, y;
	uint8_t r, g, b;
	uint32_t *pixel;

	for (x = 0; x < w; x++)
		for (y = 0; y < h; y++) {
			b = x;
			g = x + y;
			r = y;
			pixel = (uint32_t *)pixels + y * w + x;
			*pixel = (255 << 24) | (r << 16) | (g << 8) | b;
		}
}

TEST(internal_screenshot)
{
	struct wl_buffer *buf;
	struct client *client;
	struct wl_surface *surface;
	struct surface *screenshot = NULL;
	struct surface *reference_good = NULL;
	struct surface *reference_bad = NULL;
	struct rectangle clip;
	const char *fname;
	bool match = false;
	bool dump_all_images = true;
	void *pixels;

	/* Create the client */
	printf("Creating client for test\n");
	client = create_client_and_test_surface(100, 100, 100, 100);
	assert(client);
	surface = client->surface->wl_surface;

	buf = create_shm_buffer(client, 100, 100, &pixels);
	draw_stuff(pixels, 100, 100);
	wl_surface_attach(surface, buf, 0, 0);
	wl_surface_damage(surface, 0, 0, 100, 100);
	wl_surface_commit(surface);

	/* Take a snapshot.  Result will be in screenshot->wl_buffer. */
	printf("Taking a screenshot\n");
	screenshot = capture_screenshot_of_output(client);
	assert(screenshot);

	/* Load good reference image */
	fname = screenshot_reference_filename("internal-screenshot-good", 0);
	printf("Loading good reference image %s\n", fname);
	reference_good = load_surface_from_png(fname);
	assert(reference_good);

	/* Load bad reference image */
	fname = screenshot_reference_filename("internal-screenshot-bad", 0);
	printf("Loading bad reference image %s\n", fname);
	reference_bad = load_surface_from_png(fname);
	assert(reference_bad);

	/* Test check_surfaces_equal()
	 * We expect this to fail since we use a bad reference image
	 */
	match = check_surfaces_equal(screenshot, reference_bad);
	printf("Screenshot %s reference image\n", match? "equal to" : "different from");
	assert(!match);
	free(reference_bad->data);
	free(reference_bad);

	/* Test check_surfaces_match_in_clip()
	 * Alpha-blending and other effects can cause irrelevant discrepancies, so look only
	 * at a small portion of the solid-colored background
	 */
	clip.x = 100;
	clip.y = 100;
	clip.width = 100;
	clip.height = 100;
	printf("Clip: %d,%d %d x %d\n", clip.x, clip.y, clip.width, clip.height);
	match = check_surfaces_match_in_clip(screenshot, reference_good,
					     &clip);
	printf("Screenshot %s reference image in clipped area\n", match? "matches" : "doesn't match");
	free(reference_good->data);
	free(reference_good);

	/* Test dumping of non-matching images */
	if (!match || dump_all_images) {
		fname = screenshot_output_filename("internal-screenshot", 0);
		write_surface_as_png(screenshot, fname);
	}

	free(screenshot);

	printf("Test complete\n");
	assert(match);
}
