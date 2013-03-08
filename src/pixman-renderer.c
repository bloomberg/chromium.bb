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

#define _GNU_SOURCE

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
	pixman_image_t *out_buf;
	uint32_t cur_y;

	if (!po->hw_buffer) {
		errno = ENODEV;
		return -1;
	}

	out_buf = pixman_image_create_bits(format,
		width,
		height,
		pixels,
		(PIXMAN_FORMAT_BPP(format) / 8) * width);

	/* Caller expects vflipped image */
	for (cur_y = y; cur_y < y + height; cur_y++) {
		pixman_image_composite32(PIXMAN_OP_SRC,
			po->hw_buffer, /* src */
			NULL /* mask */,
			out_buf, /* dest */
			x, cur_y, /* src_x, src_y */
			0, 0, /* mask_x, mask_y */
			0, height - (cur_y - y), /* dest_x, dest_y */
			width, /* width */
			1 /* height */);
	}

	pixman_image_unref(out_buf);

	return 0;
}

static void
box_translate(pixman_box32_t *dst, const pixman_box32_t *src, int x, int y)
{
	dst->x1 = src->x1 + x;
	dst->x2 = src->x2 + x;
	dst->y1 = src->y1 + y;
	dst->y2 = src->y2 + y;
}

#define D2F(v) pixman_double_to_fixed((double)v)

static void
repaint_region_complex(struct weston_surface *es, struct weston_output *output,
		pixman_region32_t *region)
{
	struct pixman_renderer *pr =
		(struct pixman_renderer *) output->compositor->renderer;
	struct pixman_surface_state *ps = get_surface_state(es);
	struct pixman_output_state *po = get_output_state(output);
	int nrects, i;
	pixman_box32_t *rects, rect;

	/* Pixman supports only 2D transform matrix, but Weston uses 3D,
	 * so we're omitting Z coordinate here
	 */
	pixman_transform_t transform = {{
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

	pixman_transform_invert(&transform, &transform);

	pixman_image_set_transform(ps->image, &transform);
	pixman_image_set_filter(ps->image, PIXMAN_FILTER_BILINEAR, NULL, 0);

	rects = pixman_region32_rectangles(region, &nrects);
	for (i = 0; i < nrects; i++) {
		box_translate(&rect, &rects[i], -output->x, -output->y);
		pixman_image_composite32(PIXMAN_OP_OVER,
			ps->image, /* src */
			NULL /* mask */,
			po->shadow_image, /* dest */
			rects[i].x1, rects[i].y1, /* src_x, src_y */
			0, 0, /* mask_x, mask_y */
			rect.x1, rect.y1, /* dst_x, dst_y */
			rect.x2 - rect.x1, /* width */
			rect.y2 - rect.y1 /* height */
			);

		if (!pr->repaint_debug)
			continue;

		pixman_image_composite32(PIXMAN_OP_OVER,
			pr->debug_color, /* src */
			NULL /* mask */,
			po->shadow_image, /* dest */
			0, 0, /* src_x, src_y */
			0, 0, /* mask_x, mask_y */
			rect.x1, rect.y1, /* dest_x, dest_y */
			rect.x2 - rect.x1, /* width */
			rect.y2 - rect.y1 /* height */);
	}
}

static void
repaint_region_simple(struct weston_surface *es, struct weston_output *output,
		pixman_region32_t *region, pixman_region32_t *surf_region,
		pixman_op_t pixman_op)
{
	struct pixman_renderer *pr =
		(struct pixman_renderer *) output->compositor->renderer;
	struct pixman_surface_state *ps = get_surface_state(es);
	struct pixman_output_state *po = get_output_state(output);
	pixman_region32_t final_region;
	pixman_box32_t *rects, rect;
	int nrects, i, src_x, src_y;
	float surface_x, surface_y;

	/* The final region to be painted is the intersection of
	 * 'region' and 'surf_region'. However, 'region' is in the global
	 * coordinates, and 'surf_region' is in the surface-local
	 * coordinates
	 */
	pixman_region32_init(&final_region);
	pixman_region32_copy(&final_region, surf_region);

	pixman_image_set_filter(ps->image, PIXMAN_FILTER_NEAREST, NULL, 0);
	pixman_image_set_transform(ps->image, NULL);

	if (!es->transform.enabled) {
		pixman_region32_translate(&final_region, es->geometry.x, es->geometry.y);
	} else {
		weston_surface_to_global_float(es, 0, 0, &surface_x, &surface_y);
		pixman_region32_translate(&final_region, (int)surface_x, (int)surface_y);
	}

	/* That's what we need to paint */
	pixman_region32_intersect(&final_region, &final_region, region);
	rects = pixman_region32_rectangles(&final_region, &nrects);

	for (i = 0; i < nrects; i++) {
		weston_surface_from_global(es, rects[i].x1, rects[i].y1, &src_x, &src_y);
		box_translate(&rect, &rects[i], -output->x, -output->y);
		pixman_image_composite32(pixman_op,
			ps->image, /* src */
			NULL /* mask */,
			po->shadow_image, /* dest */
			src_x, src_y, /* src_x, src_y */
			0, 0, /* mask_x, mask_y */
			rect.x1, rect.y1, /* dest_x, dest_y */
			rect.x2 - rect.x1, /* width */
			rect.y2 - rect.y1 /* height */);

		if (!pr->repaint_debug)
			continue;

		pixman_image_composite32(PIXMAN_OP_OVER,
			pr->debug_color, /* src */
			NULL /* mask */,
			po->shadow_image, /* dest */
			src_x, src_y, /* src_x, src_y */
			0, 0, /* mask_x, mask_y */
			rect.x1, rect.y1, /* dest_x, dest_y */
			rect.x2 - rect.x1, /* width */
			rect.y2 - rect.y1 /* height */);
	}
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
		repaint_region_complex(es, output, &repaint);
	} else {
		/* blended region is whole surface minus opaque region: */
		pixman_region32_init_rect(&surface_blend, 0, 0,
					  es->geometry.width, es->geometry.height);
		pixman_region32_subtract(&surface_blend, &surface_blend, &es->opaque);

		if (pixman_region32_not_empty(&es->opaque)) {
			repaint_region_simple(es, output, &repaint, &es->opaque, PIXMAN_OP_SRC);
		}

		if (pixman_region32_not_empty(&surface_blend)) {
			repaint_region_simple(es, output, &repaint, &surface_blend, PIXMAN_OP_OVER);
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
	int nrects, i, width, height;
	pixman_box32_t *rects;
	pixman_box32_t b, rect;

	width = pixman_image_get_width(po->shadow_image);
	height = pixman_image_get_height(po->shadow_image);

	rects = pixman_region32_rectangles(region, &nrects);
	for (i = 0; i < nrects; i++) {
		box_translate(&rect, &rects[i], -output->x, -output->y);
		b = weston_transformed_rect(width, height,
					    output->transform, rect);

		pixman_image_composite32(PIXMAN_OP_SRC,
			po->shadow_image, /* src */
			NULL /* mask */,
			po->hw_buffer, /* dest */
			b.x1, b.y1, /* src_x, src_y */
			0, 0, /* mask_x, mask_y */
			b.x1, b.y1, /* dest_x, dest_y */
			b.x2 - b.x1, /* width */
			b.y2 - b.y1 /* height */);
	}

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
pixman_renderer_attach(struct weston_surface *es, struct wl_buffer *buffer)
{
	struct pixman_surface_state *ps = get_surface_state(es);
	pixman_format_code_t pixman_format;

	weston_buffer_reference(&ps->buffer_ref, buffer);

	if (ps->image) {
		pixman_image_unref(ps->image);
		ps->image = NULL;
	}

	if (!buffer)
		return;

	if (!wl_buffer_is_shm(buffer)) {
		weston_log("Pixman renderer supports only SHM buffers\n");
		weston_buffer_reference(&ps->buffer_ref, NULL);
		return;
	}

	switch (wl_shm_buffer_get_format(buffer)) {
	case WL_SHM_FORMAT_XRGB8888:
		pixman_format = PIXMAN_x8r8g8b8;
		break;
	case WL_SHM_FORMAT_ARGB8888:
		pixman_format = PIXMAN_a8r8g8b8;
		break;
	default:
		weston_log("Unsupported SHM buffer format\n");
		weston_buffer_reference(&ps->buffer_ref, NULL);
		return;
	break;
	}
	ps->image = pixman_image_create_bits(pixman_format,
		wl_shm_buffer_get_width(buffer),
		wl_shm_buffer_get_height(buffer),
		wl_shm_buffer_get_data(buffer),
		wl_shm_buffer_get_stride(buffer));
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
debug_binding(struct wl_seat *seat, uint32_t time, uint32_t key,
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

	weston_compositor_add_debug_binding(ec, KEY_R,
					    debug_binding, ec);
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
	pixman_transform_t transform;
	pixman_fixed_t fw, fh;
	int w, h;
	int rotated = 0;

	if (!po)
		return -1;

	/* set shadow image transformation */
	w = output->current->width;
	h = output->current->height;

	fw = pixman_int_to_fixed(w);
	fh = pixman_int_to_fixed(h);

	pixman_transform_init_identity(&transform);

	switch (output->transform) {
	default:
	case WL_OUTPUT_TRANSFORM_NORMAL:
		break;
	case WL_OUTPUT_TRANSFORM_180:
		pixman_transform_rotate(&transform, NULL, -pixman_fixed_1, 0);
		pixman_transform_translate(NULL, &transform, fw, fh);
		break;
	case WL_OUTPUT_TRANSFORM_270:
		rotated = 1;
		pixman_transform_rotate(&transform, NULL, 0, pixman_fixed_1);
		pixman_transform_translate(&transform, NULL, fh, 0);
		break;
	case WL_OUTPUT_TRANSFORM_90:
		rotated = 1;
		pixman_transform_rotate(&transform, NULL, 0, -pixman_fixed_1);
		pixman_transform_translate(&transform, NULL, 0, fw);
		break;
	}

	if (rotated) {
		int tmp = w;
		w = h;
		h = tmp;
	}

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

	pixman_image_set_transform(po->shadow_image, &transform);

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
