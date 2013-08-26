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

#include <config.h>

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
wcap_decoder_decode_rectangle(struct wcap_decoder *decoder,
			      struct wcap_rectangle *rect)
{
	uint32_t v, *p = decoder->p, *d;
	int width = rect->x2 - rect->x1, height = rect->y2 - rect->y1;
	int x, i, j, k, l, count = width * height;
	unsigned char r, g, b, dr, dg, db;

	d = decoder->frame + (rect->y2 - 1) * decoder->width;
	x = rect->x1;
	i = 0;
	while (i < count) {
		v = *p++;
		l = v >> 24;
		if (l < 0xe0) {
			j = l + 1;
		} else {
			j = 1 << (l - 0xe0 + 7);
		}

		dr = (v >> 16);
		dg = (v >>  8);
		db = (v >>  0);
		for (k = 0; k < j; k++) {
			r = (d[x] >> 16) + dr;
			g = (d[x] >>  8) + dg;
			b = (d[x] >>  0) + db;
			d[x] = 0xff000000 | (r << 16) | (g << 8) | b;
			x++;
			if (x == rect->x2) {
				x = rect->x1;
				d -= decoder->width;
			}
		}
		i += j;
	}

	if (i != count)
		printf("rle encoding longer than expected (%d expected %d)\n",
		       i, count);

	decoder->p = p;
}

int
wcap_decoder_get_frame(struct wcap_decoder *decoder)
{
	struct wcap_rectangle *rects;
	struct wcap_frame_header *header;
	uint32_t i;

	if (decoder->p == decoder->end)
		return 0;

	header = decoder->p;
	decoder->msecs = header->msecs;
	decoder->count++;

	rects = (void *) (header + 1);
	decoder->p = (uint32_t *) (rects + header->nrects);
	for (i = 0; i < header->nrects; i++)
		wcap_decoder_decode_rectangle(decoder, &rects[i]);

	return 1;
}

struct wcap_decoder *
wcap_decoder_create(const char *filename)
{
	struct wcap_decoder *decoder;
	struct wcap_header *header;
	int frame_size;
	struct stat buf;

	decoder = malloc(sizeof *decoder);
	if (decoder == NULL)
		return NULL;

	decoder->fd = open(filename, O_RDONLY);
	if (decoder->fd == -1) {
		free(decoder);
		return NULL;
	}

	fstat(decoder->fd, &buf);
	decoder->size = buf.st_size;
	decoder->map = mmap(NULL, decoder->size,
			    PROT_READ, MAP_PRIVATE, decoder->fd, 0);
		
	header = decoder->map;
	decoder->format = header->format;
	decoder->count = 0;
	decoder->width = header->width;
	decoder->height = header->height;
	decoder->p = header + 1;
	decoder->end = decoder->map + decoder->size;

	frame_size = header->width * header->height * 4;
	decoder->frame = malloc(frame_size);
	memset(decoder->frame, 0, frame_size);

	return decoder;
}

void
wcap_decoder_destroy(struct wcap_decoder *decoder)
{
	munmap(decoder->map, decoder->size);
	close(decoder->fd);
	free(decoder->frame);
	free(decoder);
}
