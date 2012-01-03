/*
 * Copyright © 2011 Intel Corporation
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

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <errno.h>
#include <png.h>

#include "compositor.h"

static inline int
multiply_alpha(int alpha, int color)
{
    int temp = (alpha * color) + 0x80;

    return ((temp + (temp >> 8)) >> 8);
}

static void
premultiply_data(png_structp   png,
		 png_row_infop row_info,
		 png_bytep     data)
{
    unsigned int i;
    png_bytep p;

    for (i = 0, p = data; i < row_info->rowbytes; i += 4, p += 4) {
	png_byte  alpha = p[3];
	uint32_t w;

	if (alpha == 0) {
		w = 0;
	} else {
		png_byte red   = p[0];
		png_byte green = p[1];
		png_byte blue  = p[2];

		if (alpha != 0xff) {
			red   = multiply_alpha(alpha, red);
			green = multiply_alpha(alpha, green);
			blue  = multiply_alpha(alpha, blue);
		}
		w = (alpha << 24) | (red << 16) | (green << 8) | (blue << 0);
	}

	* (uint32_t *) p = w;
    }
}

static void
read_func(png_structp png, png_bytep data, png_size_t size)
{
	FILE *fp = png_get_io_ptr(png);

	if (fread(data, 1, size, fp) < 0)
		png_error(png, NULL);
}

WL_EXPORT uint32_t *
wlsc_load_image(const char *filename,
		int32_t *width_arg, int32_t *height_arg, uint32_t *stride_arg)
{
	png_struct *png;
	png_info *info;
	png_byte *data;
	png_byte **row_pointers = NULL;
	png_uint_32 width, height;
	int depth, color_type, interlace, stride;
	unsigned int i;
	FILE *fp;

	fp = fopen(filename, "rb");
	if (fp == NULL)
		return NULL;

	png = png_create_read_struct(PNG_LIBPNG_VER_STRING,
				     NULL, NULL, NULL);
	if (!png) {
		fclose(fp);
		return NULL;
	}

	info = png_create_info_struct(png);
	if (!info) {
		png_destroy_read_struct(&png, &info, NULL);
		fclose(fp);
		return NULL;
	}

	png_set_read_fn(png, fp, read_func);
	png_read_info(png, info);
	png_get_IHDR(png, info,
		     &width, &height, &depth,
		     &color_type, &interlace, NULL, NULL);

	if (color_type == PNG_COLOR_TYPE_PALETTE)
		png_set_palette_to_rgb(png);

	if (color_type == PNG_COLOR_TYPE_GRAY)
		png_set_expand_gray_1_2_4_to_8(png);

	if (png_get_valid(png, info, PNG_INFO_tRNS))
		png_set_tRNS_to_alpha(png);

	if (depth == 16)
		png_set_strip_16(png);

	if (depth < 8)
		png_set_packing(png);

	if (color_type == PNG_COLOR_TYPE_GRAY ||
	    color_type == PNG_COLOR_TYPE_GRAY_ALPHA)
		png_set_gray_to_rgb(png);

	if (interlace != PNG_INTERLACE_NONE)
		png_set_interlace_handling(png);

	png_set_filler(png, 0xff, PNG_FILLER_AFTER);
	png_set_read_user_transform_fn(png, premultiply_data);
	png_read_update_info(png, info);
	png_get_IHDR(png, info,
		     &width, &height, &depth,
		     &color_type, &interlace, NULL, NULL);

	stride = width * 4;
	data = malloc(stride * height);
	if (!data) {
		png_destroy_read_struct(&png, &info, NULL);
		fclose(fp);
		return NULL;
	}

	row_pointers = malloc(height * sizeof row_pointers[0]);
	if (row_pointers == NULL) {
		free(data);
		png_destroy_read_struct(&png, &info, NULL);
		fclose(fp);
		return NULL;
	}

	for (i = 0; i < height; i++)
		row_pointers[i] = &data[i * stride];

	png_read_image(png, row_pointers);
	png_read_end(png, info);

	free(row_pointers);
	png_destroy_read_struct(&png, &info, NULL);
	fclose(fp);

	*width_arg = width;
	*height_arg = height;
	*stride_arg = stride;

	return (uint32_t *) data;
}
