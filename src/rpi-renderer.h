/*
 * Copyright © 2013 Raspberry Pi Foundation
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

#ifndef RPI_RENDERER_H
#define RPI_RENDERER_H

struct rpi_renderer_parameters {
	int single_buffer;
	int opaque_regions;
};

int
rpi_renderer_create(struct weston_compositor *compositor,
		    const struct rpi_renderer_parameters *params);

int
rpi_renderer_output_create(struct weston_output *base,
			   DISPMANX_DISPLAY_HANDLE_T display);

void
rpi_renderer_output_destroy(struct weston_output *base);

void
rpi_renderer_set_update_handle(struct weston_output *base,
			       DISPMANX_UPDATE_HANDLE_T handle);

void
rpi_renderer_finish_frame(struct weston_output *base);

#endif /* RPI_RENDERER_H */
