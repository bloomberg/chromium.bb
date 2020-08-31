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

#include <stdint.h>
#include <stdio.h>

#include "weston-test-client-helper.h"

char *server_parameters="--use-pixman --width=320 --height=240";

static void
draw_stuff(pixman_image_t *image)
{
	int w, h;
	int stride; /* bytes */
	int x, y;
	uint8_t r, g, b;
	uint32_t *pixels;
	uint32_t *pixel;
	pixman_format_code_t fmt;

	fmt = pixman_image_get_format(image);
	w = pixman_image_get_width(image);
	h = pixman_image_get_height(image);
	stride = pixman_image_get_stride(image);
	pixels = pixman_image_get_data(image);

	assert(PIXMAN_FORMAT_BPP(fmt) == 32);

	for (x = 0; x < w; x++)
		for (y = 0; y < h; y++) {
			b = x;
			g = x + y;
			r = y;
			pixel = pixels + (y * stride / 4) + x;
			*pixel = (255 << 24) | (r << 16) | (g << 8) | b;
		}
}

TEST(internal_screenshot)
{
	struct buffer *buf;
	struct client *client;
	struct wl_surface *surface;
	struct buffer *screenshot = NULL;
	pixman_image_t *reference_good = NULL;
	pixman_image_t *reference_bad = NULL;
	pixman_image_t *diffimg;
	struct rectangle clip;
	const char *fname;
	bool match = false;
	bool dump_all_images = true;

	/* Create the client */
	printf("Creating client for test\n");
	client = create_client_and_test_surface(100, 100, 100, 100);
	assert(client);
	surface = client->surface->wl_surface;

	/*
	 * We are racing our screenshooting against weston-desktop-shell
	 * setting the cursor. If w-d-s wins, our screenshot will have a cursor
	 * shown, which makes the image comparison fail. Our window and the
	 * default pointer position are accidentally causing an overlap that
	 * intersects our test clip rectangle.
	 *
	 * w-d-s wins very rarely though, so the race is easy to miss. You can
	 * make it happen by putting a delay before the call to
	 * create_client_and_test_surface().
	 *
	 * The weston_test_move_pointer() below makes the race irrelevant, as
	 * the cursor won't overlap with anything we care about.
	 */

	/* Move the pointer away from the screenshot area. */
	weston_test_move_pointer(client->test->weston_test, 0, 1, 0, 0, 0);

	buf = create_shm_buffer_a8r8g8b8(client, 100, 100);
	draw_stuff(buf->image);
	wl_surface_attach(surface, buf->proxy, 0, 0);
	wl_surface_damage(surface, 0, 0, 100, 100);
	wl_surface_commit(surface);

	/* Take a snapshot.  Result will be in screenshot->wl_buffer. */
	printf("Taking a screenshot\n");
	screenshot = capture_screenshot_of_output(client);
	assert(screenshot);

	/* Load good reference image */
	fname = screenshot_reference_filename("internal-screenshot-good", 0);
	printf("Loading good reference image %s\n", fname);
	reference_good = load_image_from_png(fname);
	assert(reference_good);

	/* Load bad reference image */
	fname = screenshot_reference_filename("internal-screenshot-bad", 0);
	printf("Loading bad reference image %s\n", fname);
	reference_bad = load_image_from_png(fname);
	assert(reference_bad);

	/* Test check_images_match() without a clip.
	 * We expect this to fail since we use a bad reference image
	 */
	match = check_images_match(screenshot->image, reference_bad, NULL);
	printf("Screenshot %s reference image\n", match? "equal to" : "different from");
	assert(!match);
	pixman_image_unref(reference_bad);

	/* Test check_images_match() with clip.
	 * Alpha-blending and other effects can cause irrelevant discrepancies, so look only
	 * at a small portion of the solid-colored background
	 */
	clip.x = 100;
	clip.y = 100;
	clip.width = 100;
	clip.height = 100;
	printf("Clip: %d,%d %d x %d\n", clip.x, clip.y, clip.width, clip.height);
	match = check_images_match(screenshot->image, reference_good, &clip);
	printf("Screenshot %s reference image in clipped area\n", match? "matches" : "doesn't match");
	if (!match) {
		diffimg = visualize_image_difference(screenshot->image, reference_good, &clip);
		fname = screenshot_output_filename("internal-screenshot-error", 0);
		write_image_as_png(diffimg, fname);
		pixman_image_unref(diffimg);
	}
	pixman_image_unref(reference_good);

	/* Test dumping of non-matching images */
	if (!match || dump_all_images) {
		fname = screenshot_output_filename("internal-screenshot", 0);
		write_image_as_png(screenshot->image, fname);
	}

	buffer_destroy(screenshot);

	printf("Test complete\n");
	assert(match);
}
