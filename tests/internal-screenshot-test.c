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

#include <unistd.h>
#include <stdio.h>
#include <string.h> /* memcpy */
#include <cairo.h>

#include "zalloc.h"
#include "weston-test-client-helper.h"

char *server_parameters="--use-pixman --width=320 --height=240";

/** write_surface_as_png()
 *
 * Writes out a given weston test surface to disk as a PNG image
 * using the provided filename (with path).
 *
 * @returns true if successfully saved file; false otherwise.
 */
static bool
write_surface_as_png(const struct surface* weston_surface, const char *fname)
{
	cairo_surface_t *cairo_surface;
	cairo_status_t status;
	int bpp = 4; /* Assume ARGB */
	int stride = bpp * weston_surface->width;

	cairo_surface = cairo_image_surface_create_for_data(weston_surface->data,
							    CAIRO_FORMAT_ARGB32,
							    weston_surface->width,
							    weston_surface->height,
							    stride);
	printf("Writing PNG to disk\n");
	status = cairo_surface_write_to_png(cairo_surface, fname);
	if (status != CAIRO_STATUS_SUCCESS) {
		printf("Failed to save screenshot: %s\n",
		       cairo_status_to_string(status));
		return false;
	}
	cairo_surface_destroy(cairo_surface);
	return true;
}

/** load_surface_from_png()
 *
 * Reads a PNG image from disk using the given filename (and path)
 * and returns as a freshly allocated weston test surface.
 *
 * @returns weston test surface with image, which should be free'd
 * when no longer used; or, NULL in case of error.
 */
static struct surface*
load_surface_from_png(const char *fname)
{
	struct surface *reference;
	cairo_surface_t *reference_cairo_surface;
	cairo_status_t status;
	size_t source_data_size;
	int bpp;
	int stride;

	reference_cairo_surface = cairo_image_surface_create_from_png(fname);
	status = cairo_surface_status(reference_cairo_surface);
	if (status != CAIRO_STATUS_SUCCESS) {
		printf("Could not open %s: %s\n", fname, cairo_status_to_string(status));
		cairo_surface_destroy(reference_cairo_surface);
		return NULL;
	}

	/* Disguise the cairo surface in a weston test surface */
	reference = zalloc(sizeof *reference);
	if (reference == NULL) {
		perror("zalloc reference");
		cairo_surface_destroy(reference_cairo_surface);
		return NULL;
	}
	reference->width = cairo_image_surface_get_width(reference_cairo_surface);
	reference->height = cairo_image_surface_get_height(reference_cairo_surface);
	stride = cairo_image_surface_get_stride(reference_cairo_surface);
	source_data_size = stride * reference->height;

	/* Check that the file's stride matches our assumption */
	bpp = 4;
	if (stride != bpp * reference->width) {
		printf("Mismatched stride for screenshot reference image %s\n", fname);
		cairo_surface_destroy(reference_cairo_surface);
		free(reference);
		return NULL;
	}

	/* Allocate new buffer for our weston reference, and copy the data from
	   the cairo surface so we can destroy it */
	reference->data = zalloc(source_data_size);
	if (reference->data == NULL) {
		perror("zalloc reference data");
		cairo_surface_destroy(reference_cairo_surface);
		free(reference);
		return NULL;
	}
	memcpy(reference->data,
	       cairo_image_surface_get_data(reference_cairo_surface),
	       source_data_size);

	cairo_surface_destroy(reference_cairo_surface);
	return reference;
}

/** create_screenshot_surface()
 *
 *  Allocates and initializes a weston test surface for use in
 *  storing a screenshot of the client's output.  Establishes a
 *  shm backed wl_buffer for retrieving screenshot image data
 *  from the server, sized to match the client's output display.
 *
 *  @returns stack allocated surface image, which should be
 *  free'd when done using it.
 */
static struct surface*
create_screenshot_surface(struct client *client)
{
	struct surface* screenshot;
	screenshot = zalloc(sizeof *screenshot);
	if (screenshot == NULL)
		return NULL;
	screenshot->wl_buffer = create_shm_buffer(client,
						  client->output->width,
						  client->output->height,
						  &screenshot->data);
	screenshot->height = client->output->height;
	screenshot->width = client->output->width;

	return screenshot;
}

/** capture_screenshot_of_output()
 *
 * Requests a screenshot from the server of the output that the
 * client appears on.  The image data returned from the server
 * can be accessed from the screenshot surface's data member.
 *
 * @returns a new surface object, which should be free'd when no
 * longer needed.
 */
static struct surface *
capture_screenshot_of_output(struct client *client)
{
	struct surface *screenshot;

	/* Create a surface to hold the screenshot */
	screenshot = create_screenshot_surface(client);

	client->test->buffer_copy_done = 0;
	weston_test_capture_screenshot(client->test->weston_test,
				       client->output->wl_output,
				       screenshot->wl_buffer);
	while (client->test->buffer_copy_done == 0)
		if (wl_display_dispatch(client->wl_display) < 0)
			break;

	/* FIXME: Document somewhere the orientation the screenshot is taken
	 * and how the clip coords are interpreted, in case of scaling/transform.
	 * If we're using read_pixels() just make sure it is documented somewhere.
	 * Protocol docs in the XML, comparison function docs in Doxygen style.
	 */

	return screenshot;
}

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
