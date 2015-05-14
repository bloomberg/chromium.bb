/*
 * Copyright © 2015 Samsung Electronics Co., Ltd
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

#include <unistd.h>
#include <stdio.h>
#include <string.h> /* memcpy */
#include <cairo.h>

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
write_surface_as_png(const struct surface* weston_surface, const char *fname) {
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

TEST(internal_screenshot)
{
	struct client *client;
	struct surface *screenshot = NULL;
	struct surface *reference = NULL;
	struct rectangle clip;
	const char *fname;
	cairo_surface_t *reference_cairo_surface;
	cairo_status_t status;
	bool match = false;
	bool dump_all_images = true;

	printf("Starting test\n");

	/* Create the client */
	client = create_client_and_test_surface(100, 100, 100, 100);
	assert(client);
	printf("Client created\n");

	/* Create a surface to hold the screenshot */
	screenshot = xzalloc(sizeof *screenshot);
	assert(screenshot);

	/* Create and attach buffer to our surface */
	screenshot->wl_buffer = create_shm_buffer(client,
						  client->output->width,
						  client->output->height,
						  &screenshot->data);
	screenshot->height = client->output->height;
	screenshot->width = client->output->width;
	assert(screenshot->wl_buffer);
	printf("Screenshot buffer created and attached to surface\n");

	/* Take a snapshot.  Result will be in screenshot->wl_buffer. */
	client->test->buffer_copy_done = 0;
	weston_test_capture_screenshot(client->test->weston_test,
				       client->output->wl_output,
				       screenshot->wl_buffer);
	printf("Capture request sent\n");
	while (client->test->buffer_copy_done == 0)
		if (wl_display_dispatch(client->wl_display) < 0)
			break;
	printf("Roundtrip done\n");

	/* FIXME: Document somewhere the orientation the screenshot is taken
	 * and how the clip coords are interpreted, in case of scaling/transform.
	 * If we're using read_pixels() just make sure it is documented somewhere.
	 * Protocol docs in the XML, comparison function docs in Doxygen style.
	 */

	/* Load reference image */
	fname = screenshot_reference_filename("internal-screenshot", 0);
	printf("Loading reference image %s\n", fname);
	reference_cairo_surface = cairo_image_surface_create_from_png(fname);
	status = cairo_surface_status(reference_cairo_surface);
	if (status != CAIRO_STATUS_SUCCESS) {
		printf("Could not open %s: %s\n", fname, cairo_status_to_string(status));
		cairo_surface_destroy(reference_cairo_surface);
		assert(status != CAIRO_STATUS_SUCCESS);
	}

	/* Disguise the cairo surface in a weston test surface */
	reference =  xzalloc(sizeof *reference);
	reference->width = cairo_image_surface_get_width(reference_cairo_surface);
	reference->height = cairo_image_surface_get_height(reference_cairo_surface);
	reference->data = cairo_image_surface_get_data(reference_cairo_surface);

	/* Test check_surfaces_equal()
	 * We expect this to fail since the clock will differ from when we made the reference image
	 */
	match = check_surfaces_equal(screenshot, reference);
	printf("Screenshot %s reference image\n", match? "equal to" : "different from");
	assert(!match);

	/* Test check_surfaces_match_in_clip()
	 * Alpha-blending and other effects can cause irrelevant discrepancies, so look only
	 * at a small portion of the solid-colored background
	 */
	clip.x = 50;
	clip.y = 50;
	clip.width = 101;
	clip.height = 101;
	printf("Clip: %d,%d %d x %d\n", clip.x, clip.y, clip.width, clip.height);
	match = check_surfaces_match_in_clip(screenshot, reference, &clip);
	printf("Screenshot %s reference image in clipped area\n", match? "matches" : "doesn't match");
	cairo_surface_destroy(reference_cairo_surface);
	free(reference);

	/* Test dumping of non-matching images */
	if (!match || dump_all_images) {
		fname = screenshot_output_filename("internal-screenshot", 0);
		write_surface_as_png(screenshot, fname);
	}

	free(screenshot);

	printf("Test complete\n");
	assert(match);
}
