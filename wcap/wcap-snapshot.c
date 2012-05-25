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

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <sys/mman.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>

#include <cairo.h>

#include "wcap-decode.h"

static void
write_png(struct wcap_decoder *decoder, const char *filename)
{
	cairo_surface_t *surface;

	surface = cairo_image_surface_create_for_data((unsigned char *) decoder->frame,
						      CAIRO_FORMAT_ARGB32,
						      decoder->width,
						      decoder->height,
						      decoder->width * 4);
	cairo_surface_write_to_png(surface, filename);
	cairo_surface_destroy(surface);
}

int main(int argc, char *argv[])
{
	struct wcap_decoder *decoder;
	int i, output_frame;
	char filename[200];

	if (argc != 2 && argc != 3) {
		fprintf(stderr, "usage: wcap-snapshot WCAP_FILE [FRAME]\n");
		return 1;
	}

	decoder = wcap_decoder_create(argv[1]);
	output_frame = -1;
	if (argc == 3)
		output_frame = strtol(argv[2], NULL, 0);

	i = 0;
	while (wcap_decoder_get_frame(decoder)) {
		if (i == output_frame) {
			snprintf(filename, sizeof filename,
				 "wcap-frame-%d.png", i);
			write_png(decoder, filename);
			printf("wrote %s\n", filename);
		}
		i++;
	}

	printf("wcap file: size %dx%d, %d frames\n",
	       decoder->width, decoder->height, i);

	wcap_decoder_destroy(decoder);
}
