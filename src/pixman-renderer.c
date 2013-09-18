/*
 * Copyright © 2012 Intel Corporation
 * Copyright © 2013 Vasily Khoruzhick <anarsoul@gmail.com>
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

#include <errno.h>
#include <stdlib.h>

#include "pixman-renderer.h"

#include <linux/input.h>

struct pixman_output_state {
	void *shadow_buffer;
	pixman_image_t *shadow_image;
	pixman_image_t *hw_buffer;
};

struct pixman_surface_state {
	pixman_image_t *image;
	struct weston_buffer_reference buffer_ref;
};

struct pixman_renderer {
	struct weston_renderer base;
	int repaint_debug;
	pixman_image_t *debug_color;
};

static inline struct pixman_output_state *
get_output_state(struct weston_output *output)
{
	return (struct pixman_output_state *)output->renderer_state;
}

static inline struct pixman_surface_state *
get_surface_state(struct weston_surface *surface)
{
	return (struct pixman_surface_state *)surface->renderer_state;
}

static inline struct pixman_renderer *
get_renderer(struct weston_compositor *ec)
{
	return (struct pixman_renderer *)ec->renderer;
}

static int
pixman_renderer_read_pixels(struct weston_output *output,
			       pixman_format_code_t format, void *pixels,
			       uint32_t x, uint32_t y,
			       uint32_t width, uint32_t height)
{
	struct pixman_output_state *po = get_output_state(output);
	pixman_transform_t transform;
	pixman_image_t *out_buf;

	if (!po->hw_buffer) {
		errno = ENODEV;
		return -1;
	}

	out_buf = pixman_image_create_bits(format,
		width,
		height,
		pixels,
		(PIXMAN_FORMAT_BPP(format) / 8) * width);

	/* Caller expects vflipped source image */
	pixman_transform_init_translate(&transform,
					pixman_int_to_fixed (x),
					pixman_int_to_fixed (y - pixman_image_get_height (po->hw_buffer)));
	pixman_transform_scale(&transform, NULL,
			       pixman_fixed_1,
			       pixman_fixed_minus_1);
	pixman_image_set_transform(po->hw_buffer, &transform);

	pixman_image_composite32(PIXMAN_OP_SRC,
				 po->hw_buffer, /* src */
				 NULL /* mask */,
				 out_buf, /* dest */
				 0, 0, /* src_x, src_y */
				 0, 0, /* mask_x, mask_y */
				 0, 0, /* dest_x, dest_y */
				 pixman_image_get_width (po->hw_buffer), /* width */
				 pixman_image_get_height (po->hw_buffer) /* height */);
	pixman_image_set_transform(po->hw_buffer, NULL);

	pixman_image_unref(out_buf);

	return 0;
}

static void
box_scale(pixman_box32_t *dst, int scale)
{
	dst->x1 *= scale;
	dst->x2 *= scale;
	dst->y1 *= scale;
	dst->y2 *= scale;
}

static void
scale_region (pixman_region32_t *region, int scale)
{
	pixman_box32_t *rects, *scaled_rects;
	int nrects, i;

	if (scale != 1)	{
		rects = pixman_region32_rectangles(region, &nrects);
		scaled_rects = calloc(nrects, sizeof(pixman_box32_t));

		for (i = 0; i < nrects; i++) {
			scaled_rects[i] = rects[i];
			box_scale(&scaled_rects[i], scale);
		}
		pixman_region32_clear(region);

		pixman_region32_init_rects (region, scaled_rects, nrects);
		free (scaled_rects);
	}
}

static void
transform_region (pixman_region32_t *region, int width, int height, enum wl_output_transform transform)
{
	pixman_box32_t *rects, *transformed_rects;
	int nrects, i;

	if (transform == WL_OUTPUT_TRANSFORM_NORMAL)
		return;

	rects = pixman_region32_rectangles(region, &nrects);
	transformed_rects = calloc(nrects, sizeof(pixman_box32_t));

	for (i = 0; i < nrects; i++) {
		switch (transform) {
		default:
		case WL_OUTPUT_TRANSFORM_NORMAL:
			transformed_rects[i].x1 = rects[i].x1;
			transformed_rects[i].y1 = rects[i].y1;
			transformed_rects[i].x2 = rects[i].x2;
			transformed_rects[i].y2 = rects[i].y2;
			break;
		case WL_OUTPUT_TRANSFORM_90:
			transformed_rects[i].x1 = height - rects[i].y2;
			transformed_rects[i].y1 = rects[i].x1;
			transformed_rects[i].x2 = height - rects[i].y1;
			transformed_rects[i].y2 = rects[i].x2;
			break;
		case WL_OUTPUT_TRANSFORM_180:
			transformed_rects[i].x1 = width - rects[i].x2;
			transformed_rects[i].y1 = height - rects[i].y2;
			transformed_rects[i].x2 = width - rects[i].x1;
			transformed_rects[i].y2 = height - rects[i].y1;
			break;
		case WL_OUTPUT_TRANSFORM_270:
			transformed_rects[i].x1 = rects[i].y1;
			transformed_rects[i].y1 = width - rects[i].x2;
			transformed_rects[i].x2 = rects[i].y2;
			transformed_rects[i].y2 = width - rects[i].x1;
			break;
		case WL_OUTPUT_TRANSFORM_FLIPPED:
			transformed_rects[i].x1 = width - rects[i].x2;
			transformed_rects[i].y1 = rects[i].y1;
			transformed_rects[i].x2 = width - rects[i].x1;
			transformed_rects[i].y2 = rects[i].y2;
			break;
		case WL_OUTPUT_TRANSFORM_FLIPPED_90:
			transformed_rects[i].x1 = height - rects[i].y2;
			transformed_rects[i].y1 = width - rects[i].x2;
			transformed_rects[i].x2 = height - rects[i].y1;
			transformed_rects[i].y2 = width - rects[i].x1;
			break;
		case WL_OUTPUT_TRANSFORM_FLIPPED_180:
			transformed_rects[i].x1 = rects[i].x1;
			transformed_rects[i].y1 = height - rects[i].y2;
			transformed_rects[i].x2 = rects[i].x2;
			transformed_rects[i].y2 = height - rects[i].y1;
			break;
		case WL_OUTPUT_TRANSFORM_FLIPPED_270:
			transformed_rects[i].x1 = rects[i].y1;
			transformed_rects[i].y1 = rects[i].x1;
			transformed_rects[i].x2 = rects[i].y2;
			transformed_rects[i].y2 = rects[i].x2;
			break;
		}
	}
	pixman_region32_clear(region);

	pixman_region32_init_rects (region, transformed_rects, nrects);
	free (transformed_rects);
}

static void
region_global_to_output(struct weston_output *output, pixman_region32_t *region)
{
	pixman_region32_translate(region, -output->x, -output->y);
	transform_region (region, output->width, output->height, output->transform);
	scale_region (region, output->current_scale);
}

#define D2F(v) pixman_double_to_fixed((double)v)

static void
repaint_region(struct weston_surface *es, struct weston_output *output,
	       pixman_region32_t *region, pixman_region32_t *surf_region,
	       pixman_op_t pixman_op)
{
	struct pixman_renderer *pr =
		(struct pixman_renderer *) output->compositor->renderer;
	struct pixman_surface_state *ps = get_surface_state(es);
	struct pixman_output_state *po = get_output_state(output);
	pixman_region32_t final_region;
	float surface_x, surface_y;
	pixman_transform_t transform;
	pixman_fixed_t fw, fh;

	/* The final region to be painted is the intersection of
	 * 'region' and 'surf_region'. However, 'region' is in the global
	 * coordinates, and 'surf_region' is in the surface-local
	 * coordinates
	 */
	pixman_region32_init(&final_region);
	if (surf_region) {
		pixman_region32_copy(&final_region, surf_region);

		/* Convert from surface to global coordinates */
		if (!es->transform.enabled) {
			pixman_region32_translate(&final_region, es->geometry.x, es->geometry.y);
		} else {
			weston_surface_to_global_float(es, 0, 0, &surface_x, &surface_y);
			pixman_region32_translate(&final_region, (int)surface_x, (int)surface_y);
		}

		/* We need to paint the intersection */
		pixman_region32_intersect(&final_region, &final_region, region);
	} else {
		/* If there is no surface region, just use the global region */
		pixman_region32_copy(&final_region, region);
	}

	/* Convert from global to output coord */
	region_global_to_output(output, &final_region);

	/* And clip to it */
	pixman_image_set_clip_region32 (po->shadow_image, &final_region);

	/* Set up the source transformation based on the surface
	   position, the output position/transform/scale and the client
	   specified buffer transform/scale */
	pixman_transform_init_identity(&transform);
	pixman_transform_scale(&transform, NULL,
			       pixman_double_to_fixed ((double)1.0/output->current_scale),
			       pixman_double_to_fixed ((double)1.0/output->current_scale));

	fw = pixman_int_to_fixed(output->width);
	fh = pixman_int_to_fixed(output->height);
	switch (output->transform) {
	default:
	case WL_OUTPUT_TRANSFORM_NORMAL:
	case WL_OUTPUT_TRANSFORM_FLIPPED:
		break;
	case WL_OUTPUT_TRANSFORM_90:
	case WL_OUTPUT_TRANSFORM_FLIPPED_90:
		pixman_transform_rotate(&transform, NULL, 0, -pixman_fixed_1);
		pixman_transform_translate(&transform, NULL, 0, fh);
		break;
	case WL_OUTPUT_TRANSFORM_180:
	case WL_OUTPUT_TRANSFORM_FLIPPED_180:
		pixman_transform_rotate(&transform, NULL, -pixman_fixed_1, 0);
		pixman_transform_translate(&transform, NULL, fw, fh);
		break;
	case WL_OUTPUT_TRANSFORM_270:
	case WL_OUTPUT_TRANSFORM_FLIPPED_270:
		pixman_transform_rotate(&transform, NULL, 0, pixman_fixed_1);
		pixman_transform_translate(&transform, NULL, fw, 0);
		break;
	}

	switch (output->transform) {
	case WL_OUTPUT_TRANSFORM_FLIPPED:
	case WL_OUTPUT_TRANSFORM_FLIPPED_90:
	case WL_OUTPUT_TRANSFORM_FLIPPED_180:
	case WL_OUTPUT_TRANSFORM_FLIPPED_270:
		pixman_transform_scale(&transform, NULL,
				       pixman_int_to_fixed (-1),
				       pixman_int_to_fixed (1));
		pixman_transform_translate(&transform, NULL, fw, 0);
		break;
	}

        pixman_transform_translate(&transform, NULL,
				   pixman_double_to_fixed (output->x),
				   pixman_double_to_fixed (output->y));

	if (es->transform.enabled) {
		/* Pixman supports only 2D transform matrix, but Weston uses 3D,
		 * so we're omitting Z coordinate here
		 */
		pixman_transform_t surface_transform = {{
				{ D2F(es->transform.matrix.d[0]),
				  D2F(es->transform.matrix.d[4]),
				  D2F(es->transform.matrix.d[12]),
				},
				{ D2F(es->transform.matrix.d[1]),
				  D2F(es->transform.matrix.d[5]),
				  D2F(es->transform.matrix.d[13]),
				},
				{ D2F(es->transform.matrix.d[3]),
				  D2F(es->transform.matrix.d[7]),
				  D2F(es->transform.matrix.d[15]),
				}
			}};

		pixman_transform_invert(&surface_transform, &surface_transform);
		pixman_transform_multiply (&transform, &surface_transform, &transform);
	} else {
		pixman_transform_translate(&transform, NULL,
					   pixman_double_to_fixed ((double)-es->geometry.x),
					   pixman_double_to_fixed ((double)-es->geometry.y));
	}


	fw = pixman_int_to_fixed(es->geometry.width);
	fh = pixman_int_to_fixed(es->geometry.height);

	switch (es->buffer_transform) {
	case WL_OUTPUT_TRANSFORM_FLIPPED:
	case WL_OUTPUT_TRANSFORM_FLIPPED_90:
	case WL_OUTPUT_TRANSFORM_FLIPPED_180:
	case WL_OUTPUT_TRANSFORM_FLIPPED_270:
		pixman_transform_scale(&transform, NULL,
				       pixman_int_to_fixed (-1),
				       pixman_int_to_fixed (1));
		pixman_transform_translate(&transform, NULL, fw, 0);
		break;
	}

	switch (es->buffer_transform) {
	default:
	case WL_OUTPUT_TRANSFORM_NORMAL:
	case WL_OUTPUT_TRANSFORM_FLIPPED:
		break;
	case WL_OUTPUT_TRANSFORM_90:
	case WL_OUTPUT_TRANSFORM_FLIPPED_90:
		pixman_transform_rotate(&transform, NULL, 0, pixman_fixed_1);
		pixman_transform_translate(&transform, NULL, fh, 0);
		break;
	case WL_OUTPUT_TRANSFORM_180:
	case WL_OUTPUT_TRANSFORM_FLIPPED_180:
		pixman_transform_rotate(&transform, NULL, -pixman_fixed_1, 0);
		pixman_transform_translate(&transform, NULL, fw, fh);
		break;
	case WL_OUTPUT_TRANSFORM_270:
	case WL_OUTPUT_TRANSFORM_FLIPPED_270:
		pixman_transform_rotate(&transform, NULL, 0, -pixman_fixed_1);
		pixman_transform_translate(&transform, NULL, 0, fw);
		break;
	}

	pixman_transform_scale(&transform, NULL,
			       pixman_double_to_fixed ((double)es->buffer_scale),
			       pixman_double_to_fixed ((double)es->buffer_scale));

	pixman_image_set_transform(ps->image, &transform);

	if (es->transform.enabled || output->current_scale != es->buffer_scale)
		pixman_image_set_filter(ps->image, PIXMAN_FILTER_BILINEAR, NULL, 0);
	else
		pixman_image_set_filter(ps->image, PIXMAN_FILTER_NEAREST, NULL, 0);

	pixman_image_composite32(pixman_op,
				 ps->image, /* src */
				 NULL /* mask */,
				 po->shadow_image, /* dest */
				 0, 0, /* src_x, src_y */
				 0, 0, /* mask_x, mask_y */
				 0, 0, /* dest_x, dest_y */
				 pixman_image_get_width (po->shadow_image), /* width */
				 pixman_image_get_height (po->shadow_image) /* height */);

	if (pr->repaint_debug)
		pixman_image_composite32(PIXMAN_OP_OVER,
					 pr->debug_color, /* src */
					 NULL /* mask */,
					 po->shadow_image, /* dest */
					 0, 0, /* src_x, src_y */
					 0, 0, /* mask_x, mask_y */
					 0, 0, /* dest_x, dest_y */
					 pixman_image_get_width (po->shadow_image), /* width */
					 pixman_image_get_height (po->shadow_image) /* height */);

	pixman_image_set_clip_region32 (po->shadow_image, NULL);

	pixman_region32_fini(&final_region);
}

static void
draw_surface(struct weston_surface *es, struct weston_output *output,
	     pixman_region32_t *damage) /* in global coordinates */
{
	struct pixman_surface_state *ps = get_surface_state(es);
	/* repaint bounding region in global coordinates: */
	pixman_region32_t repaint;
	/* non-opaque region in surface coordinates: */
	pixman_region32_t surface_blend;

	/* No buffer attached */
	if (!ps->image)
		return;

	pixman_region32_init(&repaint);
	pixman_region32_intersect(&repaint,
				  &es->transform.boundingbox, damage);
	pixman_region32_subtract(&repaint, &repaint, &es->clip);

	if (!pixman_region32_not_empty(&repaint))
		goto out;

	if (output->zoom.active) {
		weston_log("pixman renderer does not support zoom\n");
		goto out;
	}

	/* TODO: Implement repaint_region_complex() using pixman_composite_trapezoids() */
	if (es->transform.enabled &&
	    es->transform.matrix.type != WESTON_MATRIX_TRANSFORM_TRANSLATE) {
		repaint_region(es, output, &repaint, NULL, PIXMAN_OP_OVER);
	} else {
		/* blended region is whole surface minus opaque region: */
		pixman_region32_init_rect(&surface_blend, 0, 0,
					  es->geometry.width, es->geometry.height);
		pixman_region32_subtract(&surface_blend, &surface_blend, &es->opaque);

		if (pixman_region32_not_empty(&es->opaque)) {
			repaint_region(es, output, &repaint, &es->opaque, PIXMAN_OP_SRC);
		}

		if (pixman_region32_not_empty(&surface_blend)) {
			repaint_region(es, output, &repaint, &surface_blend, PIXMAN_OP_OVER);
		}
		pixman_region32_fini(&surface_blend);
	}


out:
	pixman_region32_fini(&repaint);
}
static void
repaint_surfaces(struct weston_output *output, pixman_region32_t *damage)
{
	struct weston_compositor *compositor = output->compositor;
	struct weston_surface *surface;

	wl_list_for_each_reverse(surface, &compositor->surface_list, link)
		if (surface->plane == &compositor->primary_plane)
			draw_surface(surface, output, damage);
}

static void
copy_to_hw_buffer(struct weston_output *output, pixman_region32_t *region)
{
	struct pixman_output_state *po = get_output_state(output);
	pixman_region32_t output_region;

	pixman_region32_init(&output_region);
	pixman_region32_copy(&output_region, region);

	region_global_to_output(output, &output_region);

	pixman_image_set_clip_region32 (po->hw_buffer, &output_region);

	pixman_image_composite32(PIXMAN_OP_SRC,
				 po->shadow_image, /* src */
				 NULL /* mask */,
				 po->hw_buffer, /* dest */
				 0, 0, /* src_x, src_y */
				 0, 0, /* mask_x, mask_y */
				 0, 0, /* dest_x, dest_y */
				 pixman_image_get_width (po->hw_buffer), /* width */
				 pixman_image_get_height (po->hw_buffer) /* height */);

	pixman_image_set_clip_region32 (po->hw_buffer, NULL);
}

static void
pixman_renderer_repaint_output(struct weston_output *output,
			     pixman_region32_t *output_damage)
{
	struct pixman_output_state *po = get_output_state(output);

	if (!po->hw_buffer)
		return;

	repaint_surfaces(output, output_damage);
	copy_to_hw_buffer(output, output_damage);

	pixman_region32_copy(&output->previous_damage, output_damage);
	wl_signal_emit(&output->frame_signal, output);

	/* Actual flip should be done by caller */
}

static void
pixman_renderer_flush_damage(struct weston_surface *surface)
{
	/* No-op for pixman renderer */
}

static void
pixman_renderer_attach(struct weston_surface *es, struct weston_buffer *buffer)
{
	struct pixman_surface_state *ps = get_surface_state(es);
	struct wl_shm_buffer *shm_buffer;
	pixman_format_code_t pixman_format;

	weston_buffer_reference(&ps->buffer_ref, buffer);

	if (ps->image) {
		pixman_image_unref(ps->image);
		ps->image = NULL;
	}

	if (!buffer)
		return;
	
	shm_buffer = wl_shm_buffer_get(buffer->resource);

	if (! shm_buffer) {
		weston_log("Pixman renderer supports only SHM buffers\n");
		weston_buffer_reference(&ps->buffer_ref, NULL);
		return;
	}

	switch (wl_shm_buffer_get_format(shm_buffer)) {
	case WL_SHM_FORMAT_XRGB8888:
		pixman_format = PIXMAN_x8r8g8b8;
		break;
	case WL_SHM_FORMAT_ARGB8888:
		pixman_format = PIXMAN_a8r8g8b8;
		break;
	case WL_SHM_FORMAT_RGB565:
		pixman_format = PIXMAN_r5g6b5;
		break;
	default:
		weston_log("Unsupported SHM buffer format\n");
		weston_buffer_reference(&ps->buffer_ref, NULL);
		return;
	break;
	}

	buffer->shm_buffer = shm_buffer;
	buffer->width = wl_shm_buffer_get_width(shm_buffer);
	buffer->height = wl_shm_buffer_get_height(shm_buffer);

	ps->image = pixman_image_create_bits(pixman_format,
		buffer->width, buffer->height,
		wl_shm_buffer_get_data(shm_buffer),
		wl_shm_buffer_get_stride(shm_buffer));
}

static int
pixman_renderer_create_surface(struct weston_surface *surface)
{
	struct pixman_surface_state *ps;

	ps = calloc(1, sizeof *ps);
	if (!ps)
		return -1;

	surface->renderer_state = ps;

	return 0;
}

static void
pixman_renderer_surface_set_color(struct weston_surface *es,
		 float red, float green, float blue, float alpha)
{
	struct pixman_surface_state *ps = get_surface_state(es);
	pixman_color_t color;

	color.red = red * 0xffff;
	color.green = green * 0xffff;
	color.blue = blue * 0xffff;
	color.alpha = alpha * 0xffff;
	
	if (ps->image) {
		pixman_image_unref(ps->image);
		ps->image = NULL;
	}

	ps->image = pixman_image_create_solid_fill(&color);
}

static void
pixman_renderer_destroy_surface(struct weston_surface *surface)
{
	struct pixman_surface_state *ps = get_surface_state(surface);

	if (ps->image) {
		pixman_image_unref(ps->image);
		ps->image = NULL;
	}
	weston_buffer_reference(&ps->buffer_ref, NULL);
	free(ps);
}

static void
pixman_renderer_destroy(struct weston_compositor *ec)
{
	free(ec->renderer);
	ec->renderer = NULL;
}

static void
debug_binding(struct weston_seat *seat, uint32_t time, uint32_t key,
	      void *data)
{
	struct weston_compositor *ec = data;
	struct pixman_renderer *pr = (struct pixman_renderer *) ec->renderer;

	pr->repaint_debug ^= 1;

	if (pr->repaint_debug) {
		pixman_color_t red = {
			0x3fff, 0x0000, 0x0000, 0x3fff
		};

		pr->debug_color = pixman_image_create_solid_fill(&red);
	} else {
		pixman_image_unref(pr->debug_color);
		weston_compositor_damage_all(ec);
	}
}

WL_EXPORT int
pixman_renderer_init(struct weston_compositor *ec)
{
	struct pixman_renderer *renderer;

	renderer = malloc(sizeof *renderer);
	if (renderer == NULL)
		return -1;

	renderer->repaint_debug = 0;
	renderer->debug_color = NULL;
	renderer->base.read_pixels = pixman_renderer_read_pixels;
	renderer->base.repaint_output = pixman_renderer_repaint_output;
	renderer->base.flush_damage = pixman_renderer_flush_damage;
	renderer->base.attach = pixman_renderer_attach;
	renderer->base.create_surface = pixman_renderer_create_surface;
	renderer->base.surface_set_color = pixman_renderer_surface_set_color;
	renderer->base.destroy_surface = pixman_renderer_destroy_surface;
	renderer->base.destroy = pixman_renderer_destroy;
	ec->renderer = &renderer->base;
	ec->capabilities |= WESTON_CAP_ROTATION_ANY;
	ec->capabilities |= WESTON_CAP_CAPTURE_YFLIP;

	weston_compositor_add_debug_binding(ec, KEY_R,
					    debug_binding, ec);

	wl_display_add_shm_format(ec->wl_display, WL_SHM_FORMAT_RGB565);

	return 0;
}

WL_EXPORT void
pixman_renderer_output_set_buffer(struct weston_output *output, pixman_image_t *buffer)
{
	struct pixman_output_state *po = get_output_state(output);

	if (po->hw_buffer)
		pixman_image_unref(po->hw_buffer);
	po->hw_buffer = buffer;

	if (po->hw_buffer) {
		output->compositor->read_format = pixman_image_get_format(po->hw_buffer);
		pixman_image_ref(po->hw_buffer);
	}
}

WL_EXPORT int
pixman_renderer_output_create(struct weston_output *output)
{
	struct pixman_output_state *po = calloc(1, sizeof *po);
	int w, h;

	if (!po)
		return -1;

	/* set shadow image transformation */
	w = output->current_mode->width;
	h = output->current_mode->height;

	po->shadow_buffer = malloc(w * h * 4);

	if (!po->shadow_buffer) {
		free(po);
		return -1;
	}

	po->shadow_image =
		pixman_image_create_bits(PIXMAN_x8r8g8b8, w, h,
					 po->shadow_buffer, w * 4);

	if (!po->shadow_image) {
		free(po->shadow_buffer);
		free(po);
		return -1;
	}

	output->renderer_state = po;

	return 0;
}

WL_EXPORT void
pixman_renderer_output_destroy(struct weston_output *output)
{
	struct pixman_output_state *po = get_output_state(output);

	pixman_image_unref(po->shadow_image);

	if (po->hw_buffer)
		pixman_image_unref(po->hw_buffer);

	po->shadow_image = NULL;
	po->hw_buffer = NULL;

	free(po);
}
