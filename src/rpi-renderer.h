/*
 * Copyright © 2013 Raspberry Pi Foundation
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
