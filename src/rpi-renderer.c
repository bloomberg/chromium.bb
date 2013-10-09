/*
 * Copyright © 2012-2013 Raspberry Pi Foundation
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

#include <stdlib.h>
#include <assert.h>
#include <string.h>

#ifdef HAVE_BCM_HOST
#  include <bcm_host.h>
#else
#  include "rpi-bcm-stubs.h"
#endif

#include "compositor.h"
#include "rpi-renderer.h"

#ifdef ENABLE_EGL
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include "weston-egl-ext.h"
#endif

/*
 * Dispmanx API offers alpha-blended overlays for hardware compositing.
 * The final composite consists of dispmanx elements, and their contents:
 * the dispmanx resource assigned to the element. The elements may be
 * scanned out directly, or composited to a temporary surface, depending on
 * how the firmware decides to handle the scene. Updates to multiple elements
 * may be queued in a single dispmanx update object, resulting in atomic and
 * vblank synchronized display updates.
 *
 * To avoid tearing and display artifacts, the current dispmanx resource in a
 * dispmanx element must not be touched. Therefore each element must be
 * double-buffered, using two resources, the front and the back. While a
 * dispmanx update is running, the both resources must be considered in use.
 *
 * A resource may be destroyed only, when the update removing the element has
 * completed. Otherwise you risk showing an incomplete composition.
 */

#ifndef ELEMENT_CHANGE_LAYER
/* copied from interface/vmcs_host/vc_vchi_dispmanx.h of userland.git */
#define ELEMENT_CHANGE_LAYER          (1<<0)
#define ELEMENT_CHANGE_OPACITY        (1<<1)
#define ELEMENT_CHANGE_DEST_RECT      (1<<2)
#define ELEMENT_CHANGE_SRC_RECT       (1<<3)
#define ELEMENT_CHANGE_MASK_RESOURCE  (1<<4)
#define ELEMENT_CHANGE_TRANSFORM      (1<<5)
#endif

#if 0
#define DBG(...) \
	weston_log(__VA_ARGS__)
#else
#define DBG(...) do {} while (0)
#endif

/* If we had a fully featured vc_dispmanx_resource_write_data()... */
/*#define HAVE_RESOURCE_WRITE_DATA_RECT 1*/

struct rpi_resource {
	DISPMANX_RESOURCE_HANDLE_T handle;
	int width;
	int height; /* height of the image (valid pixel data) */
	int stride; /* bytes */
	int buffer_height; /* height of the buffer */
	VC_IMAGE_TYPE_T ifmt;
};

struct rpir_output;

struct rpir_egl_buffer {
	struct weston_buffer_reference buffer_ref;
	DISPMANX_RESOURCE_HANDLE_T resource_handle;
};

enum buffer_type {
	BUFFER_TYPE_NULL,
	BUFFER_TYPE_SHM,
	BUFFER_TYPE_EGL
};

struct rpir_surface {
	struct weston_surface *surface;

	/* If link is empty, the surface is guaranteed to not be on screen,
	 * i.e. updates removing Elements have completed.
	 */
	struct wl_list link;

	DISPMANX_ELEMENT_HANDLE_T handle;
	int layer;
	int need_swap;
	int single_buffer;

	struct rpi_resource resources[2];
	struct rpi_resource *front;
	struct rpi_resource *back;
	pixman_region32_t prev_damage;

	struct rpir_egl_buffer *egl_front;
	struct rpir_egl_buffer *egl_back;
	struct rpir_egl_buffer *egl_old_front;

	struct weston_buffer_reference buffer_ref;
	enum buffer_type buffer_type;
};

struct rpir_output {
	DISPMANX_DISPLAY_HANDLE_T display;

	DISPMANX_UPDATE_HANDLE_T update;
	struct weston_matrix matrix;

	/* all Elements currently on screen */
	struct wl_list surface_list; /* struct rpir_surface::link */

	/* Elements just removed, waiting for update completion */
	struct wl_list surface_cleanup_list; /* struct rpir_surface::link */

	struct rpi_resource capture_buffer;
	uint8_t *capture_data;
};

struct rpi_renderer {
	struct weston_renderer base;

	int single_buffer;

#ifdef ENABLE_EGL
	EGLDisplay egl_display;

	PFNEGLBINDWAYLANDDISPLAYWL bind_display;
	PFNEGLUNBINDWAYLANDDISPLAYWL unbind_display;
	PFNEGLQUERYWAYLANDBUFFERWL query_buffer;
#endif
	int has_bind_display;
};

static inline struct rpir_surface *
to_rpir_surface(struct weston_surface *surface)
{
	return surface->renderer_state;
}

static inline struct rpir_output *
to_rpir_output(struct weston_output *output)
{
	return output->renderer_state;
}

static inline struct rpi_renderer *
to_rpi_renderer(struct weston_compositor *compositor)
{
	return container_of(compositor->renderer, struct rpi_renderer, base);
}

static inline int
int_max(int a, int b)
{
	return a > b ? a : b;
}

static inline void
int_swap(int *a, int *b)
{
	int tmp = *a;
	*a = *b;
	*b = tmp;
}

static uint8_t
float2uint8(float f)
{
	int v = roundf(f * 255.0f);

	return v < 0 ? 0 : (v > 255 ? 255 : v);
}

static void
rpi_resource_init(struct rpi_resource *resource)
{
	resource->handle = DISPMANX_NO_HANDLE;
}

static void
rpi_resource_release(struct rpi_resource *resource)
{
	if (resource->handle == DISPMANX_NO_HANDLE)
		return;

	vc_dispmanx_resource_delete(resource->handle);
	DBG("resource %p release\n", resource);
	resource->handle = DISPMANX_NO_HANDLE;
}

static int
rpi_resource_realloc(struct rpi_resource *resource, VC_IMAGE_TYPE_T ifmt,
		     int width, int height, int stride, int buffer_height)
{
	uint32_t dummy;

	if (resource->handle != DISPMANX_NO_HANDLE &&
	    resource->width == width &&
	    resource->height == height &&
	    resource->stride == stride &&
	    resource->buffer_height == buffer_height &&
	    resource->ifmt == ifmt)
		return 0;

	rpi_resource_release(resource);

	/* NOTE: if stride is not a multiple of 16 pixels in bytes,
	 * the vc_image_* functions may break. Dispmanx elements
	 * should be fine, though. Buffer_height probably has similar
	 * constraints, too.
	 */
	resource->handle =
		vc_dispmanx_resource_create(ifmt,
					    width | (stride << 16),
					    height | (buffer_height << 16),
					    &dummy);
	if (resource->handle == DISPMANX_NO_HANDLE)
		return -1;

	resource->width = width;
	resource->height = height;
	resource->stride = stride;
	resource->buffer_height = buffer_height;
	resource->ifmt = ifmt;
	DBG("resource %p alloc\n", resource);
	return 1;
}

/* A firmware workaround for broken ALPHA_PREMULT + ALPHA_MIX hardware. */
#define PREMULT_ALPHA_FLAG (1 << 31)

static VC_IMAGE_TYPE_T
shm_buffer_get_vc_format(struct wl_shm_buffer *buffer)
{
	switch (wl_shm_buffer_get_format(buffer)) {
	case WL_SHM_FORMAT_XRGB8888:
		return VC_IMAGE_XRGB8888;
	case WL_SHM_FORMAT_ARGB8888:
		return VC_IMAGE_ARGB8888 | PREMULT_ALPHA_FLAG;
	case WL_SHM_FORMAT_RGB565:
		return VC_IMAGE_RGB565;
	default:
		/* invalid format */
		return VC_IMAGE_MIN;
	}
}

static int
rpi_resource_update(struct rpi_resource *resource, struct weston_buffer *buffer,
		    pixman_region32_t *region)
{
	pixman_region32_t write_region;
	pixman_box32_t *r;
	VC_RECT_T rect;
	VC_IMAGE_TYPE_T ifmt;
	uint32_t *pixels;
	int width;
	int height;
	int stride;
	int ret;
#ifdef HAVE_RESOURCE_WRITE_DATA_RECT
	int n;
#endif

	if (!buffer)
		return -1;

	ifmt = shm_buffer_get_vc_format(buffer->shm_buffer);
	width = wl_shm_buffer_get_width(buffer->shm_buffer);
	height = wl_shm_buffer_get_height(buffer->shm_buffer);
	stride = wl_shm_buffer_get_stride(buffer->shm_buffer);
	pixels = wl_shm_buffer_get_data(buffer->shm_buffer);

	ret = rpi_resource_realloc(resource, ifmt & ~PREMULT_ALPHA_FLAG,
				   width, height, stride, height);
	if (ret < 0)
		return -1;

	pixman_region32_init_rect(&write_region, 0, 0, width, height);
	if (ret == 0)
		pixman_region32_intersect(&write_region,
					  &write_region, region);

#ifdef HAVE_RESOURCE_WRITE_DATA_RECT
	/* XXX: Can this do a format conversion, so that scanout does not have to? */
	r = pixman_region32_rectangles(&write_region, &n);
	while (n--) {
		vc_dispmanx_rect_set(&rect, r[n].x1, r[n].y1,
				     r[n].x2 - r[n].x1, r[n].y2 - r[n].y1);

		ret = vc_dispmanx_resource_write_data_rect(resource->handle,
							   ifmt, stride,
							   pixels, &rect,
							   rect.x, rect.y);
		DBG("%s: %p %ux%u@%u,%u, ret %d\n", __func__, resource,
		    rect.width, rect.height, rect.x, rect.y, ret);
		if (ret)
			break;
	}
#else
	/* vc_dispmanx_resource_write_data() ignores ifmt,
	 * rect.x, rect.width, and uses stride only for computing
	 * the size of the transfer as rect.height * stride.
	 * Therefore we can only write rows starting at x=0.
	 * To be able to write more than one scanline at a time,
	 * the resource must have been created with the same stride
	 * as used here, and we must write full scanlines.
	 */

	r = pixman_region32_extents(&write_region);
	vc_dispmanx_rect_set(&rect, 0, r->y1, width, r->y2 - r->y1);
	ret = vc_dispmanx_resource_write_data(resource->handle,
					      ifmt, stride, pixels, &rect);
	DBG("%s: %p %ux%u@%u,%u, ret %d\n", __func__, resource,
	    width, r->y2 - r->y1, 0, r->y1, ret);
#endif

	pixman_region32_fini(&write_region);

	return ret ? -1 : 0;
}

static struct rpir_surface *
rpir_surface_create(struct rpi_renderer *renderer)
{
	struct rpir_surface *surface;

	surface = calloc(1, sizeof *surface);
	if (!surface)
		return NULL;

	wl_list_init(&surface->link);
	surface->single_buffer = renderer->single_buffer;
	surface->handle = DISPMANX_NO_HANDLE;
	rpi_resource_init(&surface->resources[0]);
	rpi_resource_init(&surface->resources[1]);
	surface->front = &surface->resources[0];
	if (surface->single_buffer)
		surface->back = &surface->resources[0];
	else
		surface->back = &surface->resources[1];
	surface->buffer_type = BUFFER_TYPE_NULL;

	pixman_region32_init(&surface->prev_damage);

	return surface;
}

static void
rpir_surface_destroy(struct rpir_surface *surface)
{
	wl_list_remove(&surface->link);

	if (surface->handle != DISPMANX_NO_HANDLE)
		weston_log("ERROR rpi: destroying on-screen element\n");

	pixman_region32_fini(&surface->prev_damage);
	rpi_resource_release(&surface->resources[0]);
	rpi_resource_release(&surface->resources[1]);
	DBG("rpir_surface %p destroyed (%u)\n", surface, surface->handle);

	if (surface->egl_back != NULL) {
		weston_buffer_reference(&surface->egl_back->buffer_ref, NULL);
		free(surface->egl_back);
		surface->egl_back = NULL;
	}

	if (surface->egl_front != NULL) {
		weston_buffer_reference(&surface->egl_front->buffer_ref, NULL);
		free(surface->egl_front);
		surface->egl_front = NULL;
	}

	if (surface->egl_old_front != NULL) {
		weston_buffer_reference(&surface->egl_old_front->buffer_ref, NULL);
		free(surface->egl_old_front);
		surface->egl_old_front = NULL;
	}

	free(surface);
}

static int
rpir_surface_damage(struct rpir_surface *surface, struct weston_buffer *buffer,
		    pixman_region32_t *damage)
{
	pixman_region32_t upload;
	int ret;

	if (!pixman_region32_not_empty(damage))
		return 0;

	DBG("rpir_surface %p update resource %p\n", surface, surface->back);

	/* XXX: todo: if no surface->handle, update front buffer directly
	 * to avoid creating a new back buffer */
	if (surface->single_buffer) {
		ret = rpi_resource_update(surface->front, buffer, damage);
	} else {
		pixman_region32_init(&upload);
		pixman_region32_union(&upload, &surface->prev_damage, damage);
		ret = rpi_resource_update(surface->back, buffer, &upload);
		pixman_region32_fini(&upload);
	}

	pixman_region32_copy(&surface->prev_damage, damage);
	surface->need_swap = 1;

	return ret;
}

static void
matrix_type_str(struct weston_matrix *matrix, char *buf, int len)
{
	static const char types[33] = "TSRO";
	unsigned mask = matrix->type;
	int i = 0;

	while (mask && i < len - 1) {
		if (mask & (1u << i))
			*buf++ = types[i];
		mask &= ~(1u << i);
		i++;
	}
	*buf = '\0';
}

static void
log_print_matrix(struct weston_matrix *matrix)
{
	char typestr[6];
	float *d = matrix->d;

	matrix_type_str(matrix, typestr, sizeof typestr);
	weston_log_continue("%14.6e %14.6e %14.6e %14.6e\n",
			    d[0], d[4], d[8], d[12]);
	weston_log_continue("%14.6e %14.6e %14.6e %14.6e\n",
			    d[1], d[5], d[9], d[13]);
	weston_log_continue("%14.6e %14.6e %14.6e %14.6e\n",
			    d[2], d[6], d[10], d[14]);
	weston_log_continue("%14.6e %14.6e %14.6e %14.6e type: %s\n",
			    d[3], d[7], d[11], d[15], typestr);
}

static void
warn_bad_matrix(struct weston_matrix *total, struct weston_matrix *output,
		struct weston_matrix *surface)
{
	static int n_warn;
	char typestr[6];

	if (n_warn++ == 10)
		weston_log("%s: not showing more warnings\n", __func__);

	if (n_warn > 10)
		return;

	weston_log("%s: warning: total transformation is not renderable:\n",
		   __func__);
	log_print_matrix(total);

	matrix_type_str(surface, typestr, sizeof typestr);
	weston_log_continue("surface matrix type: %s\n", typestr);
	matrix_type_str(output, typestr, sizeof typestr);
	weston_log_continue("output matrix type: %s\n", typestr);
}

/*#define SURFACE_TRANSFORM */

static int
rpir_surface_compute_rects(struct rpir_surface *surface,
			   VC_RECT_T *src_rect, VC_RECT_T *dst_rect,
			   VC_IMAGE_TRANSFORM_T *flipmask)
{
	struct weston_output *output_base = surface->surface->output;
	struct rpir_output *output = to_rpir_output(output_base);
	struct weston_matrix matrix = surface->surface->transform.matrix;
	VC_IMAGE_TRANSFORM_T flipt = 0;
	int src_x, src_y;
	int dst_x, dst_y;
	int src_width, src_height;
	int dst_width, dst_height;
	struct weston_vector p1 = {{ 0.0f, 0.0f, 0.0f, 1.0f }};
	struct weston_vector p2 = {{ 0.0f, 0.0f, 0.0f, 1.0f }};
	int t;
	int over;

	/* XXX: take buffer transform into account */

	/* src is in 16.16, dst is in 32.0 fixed point.
	 * Negative values are not allowed in VC_RECT_T.
	 * Clip size to output boundaries, firmware ignores
	 * huge elements like 8192x8192.
	 */

	src_x = 0 << 16;
	src_y = 0 << 16;

	if (surface->buffer_type == BUFFER_TYPE_EGL) {
		struct weston_buffer *buffer = surface->egl_front->buffer_ref.buffer;

		src_width = buffer->width << 16;
		src_height = buffer->height << 16;
	} else {
		src_width = surface->front->width << 16;
		src_height = surface->front->height << 16;
	}

	weston_matrix_multiply(&matrix, &output->matrix);

#ifdef SURFACE_TRANSFORM
	if (matrix.type >= WESTON_MATRIX_TRANSFORM_OTHER) {
#else
	if (matrix.type >= WESTON_MATRIX_TRANSFORM_ROTATE) {
#endif
		warn_bad_matrix(&matrix, &output->matrix,
				&surface->surface->transform.matrix);
	} else {
		if (matrix.type & WESTON_MATRIX_TRANSFORM_ROTATE) {
			if (fabsf(matrix.d[0]) < 1e-4f &&
			    fabsf(matrix.d[5]) < 1e-4f) {
				flipt |= TRANSFORM_TRANSPOSE;
			} else if (fabsf(matrix.d[1]) < 1e-4 &&
				   fabsf(matrix.d[4]) < 1e-4) {
				/* no transpose */
			} else {
				warn_bad_matrix(&matrix, &output->matrix,
					&surface->surface->transform.matrix);
			}
		}
	}

	p2.f[0] = surface->surface->geometry.width;
	p2.f[1] = surface->surface->geometry.height;

	/* transform top-left and bot-right corner into screen coordinates */
	weston_matrix_transform(&matrix, &p1);
	weston_matrix_transform(&matrix, &p2);

	/* Compute the destination rectangle on screen, converting
	 * negative dimensions to flips.
	 */

	dst_width = round(p2.f[0] - p1.f[0]);
	if (dst_width < 0) {
		dst_x = round(p2.f[0]);
		dst_width = -dst_width;

		if (!(flipt & TRANSFORM_TRANSPOSE))
			flipt |= TRANSFORM_HFLIP;
		else
			flipt |= TRANSFORM_VFLIP;
	} else {
		dst_x = round(p1.f[0]);
	}

	dst_height = round(p2.f[1] - p1.f[1]);
	if (dst_height < 0) {
		dst_y = round(p2.f[1]);
		dst_height = -dst_height;

		if (!(flipt & TRANSFORM_TRANSPOSE))
			flipt |= TRANSFORM_VFLIP;
		else
			flipt |= TRANSFORM_HFLIP;
	} else {
		dst_y = round(p1.f[1]);
	}

	if (dst_width == 0 || dst_height == 0) {
		DBG("ignored, zero surface area before clipping\n");
		return -1;
	}

#ifdef SURFACE_TRANSFORM
	/* Dispmanx works as if you flipped the whole screen, when
	 * you flip an element. But, we want to flip an element in place.
	 * XXX: fixme
	 */
	if (flipt & TRANSFORM_HFLIP)
		dst_x = output_base->width - dst_x;
	if (flipt & TRANSFORM_VFLIP)
		dst_y = output_base->height - dst_y;
	if (flipt & TRANSFORM_TRANSPOSE) {
		int_swap(&dst_x, &dst_y);
		int_swap(&dst_width, &dst_height);
	}
#else
	switch (output_base->transform) {
	case WL_OUTPUT_TRANSFORM_FLIPPED:
		flipt = TRANSFORM_HFLIP;
		break;
	case WL_OUTPUT_TRANSFORM_NORMAL:
		flipt = 0;
		break;

	case WL_OUTPUT_TRANSFORM_FLIPPED_90:
		flipt = TRANSFORM_HFLIP | TRANSFORM_VFLIP | TRANSFORM_TRANSPOSE;
		break;
	case WL_OUTPUT_TRANSFORM_90:
		flipt = TRANSFORM_VFLIP | TRANSFORM_TRANSPOSE;
		break;
	case WL_OUTPUT_TRANSFORM_FLIPPED_180:
		flipt = TRANSFORM_VFLIP;
		break;
	case WL_OUTPUT_TRANSFORM_180:
		flipt = TRANSFORM_HFLIP | TRANSFORM_VFLIP;
		break;

	case WL_OUTPUT_TRANSFORM_FLIPPED_270:
		flipt = TRANSFORM_TRANSPOSE;
		break;
	case WL_OUTPUT_TRANSFORM_270:
		flipt = TRANSFORM_HFLIP | TRANSFORM_TRANSPOSE;
		break;
	default:
		break;
	}
#endif

	/* clip destination rectangle to screen dimensions */

	if (dst_x < 0) {
		t = (int64_t)dst_x * src_width / dst_width;
		src_width += t;
		dst_width += dst_x;
		src_x -= t;
		dst_x = 0;
	}

	if (dst_y < 0) {
		t = (int64_t)dst_y * src_height / dst_height;
		src_height += t;
		dst_height += dst_y;
		src_y -= t;
		dst_y = 0;
	}

	over = dst_x + dst_width - output_base->width;
	if (over > 0) {
		t = (int64_t)over * src_width / dst_width;
		src_width -= t;
		dst_width -= over;
	}

	over = dst_y + dst_height - output_base->height;
	if (over > 0) {
		t = (int64_t)over * src_height / dst_height;
		src_height -= t;
		dst_height -= over;
	}

	src_width = int_max(src_width, 0);
	src_height = int_max(src_height, 0);

	DBG("rpir_surface %p %dx%d: p1 %f, %f; p2 %f, %f\n", surface,
	    surface->surface->geometry.width,
	    surface->surface->geometry.height,
	    p1.f[0], p1.f[1], p2.f[0], p2.f[1]);
	DBG("src rect %d;%d, %d;%d, %d;%dx%d;%d\n",
	    src_x >> 16, src_x & 0xffff,
	    src_y >> 16, src_y & 0xffff,
	    src_width >> 16, src_width & 0xffff,
	    src_height >> 16, src_height & 0xffff);
	DBG("dest rect %d, %d, %dx%d%s%s%s\n",
	    dst_x, dst_y, dst_width, dst_height,
	    (flipt & TRANSFORM_HFLIP) ? " hflip" : "",
	    (flipt & TRANSFORM_VFLIP) ? " vflip" : "",
	    (flipt & TRANSFORM_TRANSPOSE) ? " transp" : "");

	assert(src_x >= 0);
	assert(src_y >= 0);
	assert(dst_x >= 0);
	assert(dst_y >= 0);

	if (dst_width < 1 || dst_height < 1) {
		DBG("ignored, zero surface area after clipping\n");
		return -1;
	}

	/* EGL buffers will be upside-down related to what DispmanX expects */
	if (surface->buffer_type == BUFFER_TYPE_EGL)
		flipt ^= TRANSFORM_VFLIP;

	vc_dispmanx_rect_set(src_rect, src_x, src_y, src_width, src_height);
	vc_dispmanx_rect_set(dst_rect, dst_x, dst_y, dst_width, dst_height);
	*flipmask = flipt;

	return 0;
}

static DISPMANX_TRANSFORM_T
vc_image2dispmanx_transform(VC_IMAGE_TRANSFORM_T t)
{
	/* XXX: uhh, are these right? */
	switch (t) {
	case VC_IMAGE_ROT0:
		return DISPMANX_NO_ROTATE;
	case VC_IMAGE_MIRROR_ROT0:
		return DISPMANX_FLIP_HRIZ;
	case VC_IMAGE_MIRROR_ROT180:
		return DISPMANX_FLIP_VERT;
	case VC_IMAGE_ROT180:
		return DISPMANX_ROTATE_180;
	case VC_IMAGE_MIRROR_ROT90:
		return DISPMANX_ROTATE_90 | DISPMANX_FLIP_HRIZ;
	case VC_IMAGE_ROT270:
		return DISPMANX_ROTATE_270;
	case VC_IMAGE_ROT90:
		return DISPMANX_ROTATE_90;
	case VC_IMAGE_MIRROR_ROT270:
		return DISPMANX_ROTATE_270 | DISPMANX_FLIP_VERT;
	default:
		assert(0 && "bad VC_IMAGE_TRANSFORM_T");
		return DISPMANX_NO_ROTATE;
	}
}


static DISPMANX_RESOURCE_HANDLE_T
rpir_surface_get_resource(struct rpir_surface *surface)
{
	switch (surface->buffer_type) {
	case BUFFER_TYPE_SHM:
	case BUFFER_TYPE_NULL:
		return surface->front->handle;
	case BUFFER_TYPE_EGL:
		if (surface->egl_front != NULL)
			return surface->egl_front->resource_handle;
	default:
		return DISPMANX_NO_HANDLE;
	}
}

static int
rpir_surface_dmx_add(struct rpir_surface *surface, struct rpir_output *output,
		    DISPMANX_UPDATE_HANDLE_T update, int layer)
{
	/* Do not use DISPMANX_FLAGS_ALPHA_PREMULT here.
	 * If you define PREMULT and ALPHA_MIX, the hardware will not
	 * multiply the source color with the element alpha, leading to
	 * bad colors. Instead, we define PREMULT during pixel data upload.
	 */
	VC_DISPMANX_ALPHA_T alphasetup = {
		DISPMANX_FLAGS_ALPHA_FROM_SOURCE |
		DISPMANX_FLAGS_ALPHA_MIX,
		float2uint8(surface->surface->alpha), /* opacity 0-255 */
		0 /* mask resource handle */
	};
	VC_RECT_T dst_rect;
	VC_RECT_T src_rect;
	VC_IMAGE_TRANSFORM_T flipmask;
	int ret;
	DISPMANX_RESOURCE_HANDLE_T resource_handle;

	resource_handle = rpir_surface_get_resource(surface);
	if (resource_handle == DISPMANX_NO_HANDLE) {
		weston_log("%s: no buffer yet, aborting\n", __func__);
		return 0;
	}

	ret = rpir_surface_compute_rects(surface, &src_rect, &dst_rect,
					 &flipmask);
	if (ret < 0)
		return 0;

	surface->handle = vc_dispmanx_element_add(
		update,
		output->display,
		layer,
		&dst_rect,
		resource_handle,
		&src_rect,
		DISPMANX_PROTECTION_NONE,
		&alphasetup,
		NULL /* clamp */,
		vc_image2dispmanx_transform(flipmask));
	DBG("rpir_surface %p add %u, alpha %f resource %d\n", surface,
	    surface->handle, surface->surface->alpha, resource_handle);

	return 1;
}

static void
rpir_surface_dmx_swap(struct rpir_surface *surface,
		      DISPMANX_UPDATE_HANDLE_T update)
{
	VC_RECT_T rect;
	pixman_box32_t *r;

	/* XXX: skip, iff resource was not reallocated, and single-buffering */
	vc_dispmanx_element_change_source(update, surface->handle,
					  surface->front->handle);

	/* This is current damage now, after rpir_surface_damage() */
	r = pixman_region32_extents(&surface->prev_damage);

	vc_dispmanx_rect_set(&rect, r->x1, r->y1,
			     r->x2 - r->x1, r->y2 - r->y1);
	vc_dispmanx_element_modified(update, surface->handle, &rect);
	DBG("rpir_surface %p swap\n", surface);
}

static int
rpir_surface_dmx_move(struct rpir_surface *surface,
		      DISPMANX_UPDATE_HANDLE_T update, int layer)
{
	uint8_t alpha = float2uint8(surface->surface->alpha);
	VC_RECT_T dst_rect;
	VC_RECT_T src_rect;
	VC_IMAGE_TRANSFORM_T flipmask;
	int ret;

	/* XXX: return early, if all attributes stay the same */

	if (surface->buffer_type == BUFFER_TYPE_EGL) {
		DISPMANX_RESOURCE_HANDLE_T resource_handle;

		resource_handle = rpir_surface_get_resource(surface);
		if (resource_handle == DISPMANX_NO_HANDLE) {
			weston_log("%s: no buffer yet, aborting\n", __func__);
			return 0;
		}

		vc_dispmanx_element_change_source(update,
						  surface->handle,
						  resource_handle);
	}

	ret = rpir_surface_compute_rects(surface, &src_rect, &dst_rect,
					 &flipmask);
	if (ret < 0)
		return 0;

	ret = vc_dispmanx_element_change_attributes(
		update,
		surface->handle,
		ELEMENT_CHANGE_LAYER |
			ELEMENT_CHANGE_OPACITY |
			ELEMENT_CHANGE_TRANSFORM |
			ELEMENT_CHANGE_DEST_RECT |
			ELEMENT_CHANGE_SRC_RECT,
		layer,
		alpha,
		&dst_rect,
		&src_rect,
		DISPMANX_NO_HANDLE,
		/* This really is DISPMANX_TRANSFORM_T, no matter
		 * what the header says. */
		vc_image2dispmanx_transform(flipmask));
	DBG("rpir_surface %p move\n", surface);

	if (ret)
		return -1;

	return 1;
}

static void
rpir_surface_dmx_remove(struct rpir_surface *surface,
			DISPMANX_UPDATE_HANDLE_T update)
{
	if (surface->handle == DISPMANX_NO_HANDLE)
		return;

	vc_dispmanx_element_remove(update, surface->handle);
	DBG("rpir_surface %p remove %u\n", surface, surface->handle);
	surface->handle = DISPMANX_NO_HANDLE;
}

static void
rpir_surface_swap_pointers(struct rpir_surface *surface)
{
	struct rpi_resource *tmp;

	tmp = surface->front;
	surface->front = surface->back;
	surface->back = tmp;
	surface->need_swap = 0;
	DBG("new back %p, new front %p\n", surface->back, surface->front);
}

static int
is_surface_not_visible(struct weston_surface *surface)
{
	/* Return true, if surface is guaranteed to be totally obscured. */
	int ret;
	pixman_region32_t unocc;

	pixman_region32_init(&unocc);
	pixman_region32_subtract(&unocc, &surface->transform.boundingbox,
				 &surface->clip);
	ret = !pixman_region32_not_empty(&unocc);
	pixman_region32_fini(&unocc);

	return ret;
}

static void
rpir_surface_update(struct rpir_surface *surface, struct rpir_output *output,
		    DISPMANX_UPDATE_HANDLE_T update, int layer)
{
	int need_swap = surface->need_swap;
	int ret;
	int obscured;

	if (surface->buffer_type == BUFFER_TYPE_EGL) {
		if (surface->egl_back != NULL) {
			assert(surface->egl_old_front == NULL);
			surface->egl_old_front = surface->egl_front;
			surface->egl_front = surface->egl_back;
			surface->egl_back = NULL;
		}
		if (surface->egl_front->buffer_ref.buffer == NULL) {
			weston_log("warning: client unreffed current front buffer\n");

			wl_list_remove(&surface->link);
			if (surface->handle == DISPMANX_NO_HANDLE) {
				wl_list_init(&surface->link);
			} else {
				rpir_surface_dmx_remove(surface, update);
				wl_list_insert(&output->surface_cleanup_list,
						   &surface->link);
			}

			goto out;
		}
	} else {
		if (need_swap)
			rpir_surface_swap_pointers(surface);
	}

	obscured = is_surface_not_visible(surface->surface);
	if (obscured) {
		DBG("rpir_surface %p totally obscured.\n", surface);

		wl_list_remove(&surface->link);
		if (surface->handle == DISPMANX_NO_HANDLE) {
			wl_list_init(&surface->link);
		} else {
			rpir_surface_dmx_remove(surface, update);
			wl_list_insert(&output->surface_cleanup_list,
				       &surface->link);
		}

		goto out;
	}

	if (surface->handle == DISPMANX_NO_HANDLE) {
		ret = rpir_surface_dmx_add(surface, output, update, layer);
		if (ret == 0) {
			wl_list_remove(&surface->link);
			wl_list_init(&surface->link);
		} else if (ret < 0) {
			weston_log("ERROR rpir_surface_dmx_add() failed.\n");
		}
	} else {
		if (need_swap)
			rpir_surface_dmx_swap(surface, update);

		ret = rpir_surface_dmx_move(surface, update, layer);
		if (ret == 0) {
			rpir_surface_dmx_remove(surface, update);

			wl_list_remove(&surface->link);
			wl_list_insert(&output->surface_cleanup_list,
				       &surface->link);
		} else if (ret < 0) {
			weston_log("ERROR rpir_surface_dmx_move() failed.\n");
		}
	}

out:
	surface->layer = layer;
}

static int
rpi_renderer_read_pixels(struct weston_output *base,
			 pixman_format_code_t format, void *pixels,
			 uint32_t x, uint32_t y,
			 uint32_t width, uint32_t height)
{
	struct rpir_output *output = to_rpir_output(base);
	struct rpi_resource *buffer = &output->capture_buffer;
	VC_RECT_T rect;
	uint32_t fb_width, fb_height;
	uint32_t dst_pitch;
	uint32_t i;
	int ret;

	fb_width = base->current_mode->width;
	fb_height = base->current_mode->height;

	DBG("%s(%u, %u, %u, %u), resource %p\n", __func__,
	    x, y, width, height, buffer);

	if (format != PIXMAN_a8r8g8b8) {
		weston_log("rpi-renderer error: bad read_format\n");
		return -1;
	}

	dst_pitch = fb_width * 4;

	if (buffer->handle == DISPMANX_NO_HANDLE) {
		free(output->capture_data);
		output->capture_data = NULL;

		ret = rpi_resource_realloc(buffer, VC_IMAGE_ARGB8888,
					   fb_width, fb_height,
					   dst_pitch, fb_height);
		if (ret < 0) {
			weston_log("rpi-renderer error: "
				   "allocating read buffer failed\n");
			return -1;
		}

		ret = vc_dispmanx_snapshot(output->display, buffer->handle,
					   VC_IMAGE_ROT0);
		if (ret) {
			weston_log("rpi-renderer error: "
				   "vc_dispmanx_snapshot returned %d\n", ret);
			return -1;
		}
		DBG("%s: snapshot done.\n", __func__);
	}

	/*
	 * If vc_dispmanx_resource_read_data was able to read sub-rectangles,
	 * we could read directly into 'pixels'. But it cannot, it does not
	 * use rect.x or rect.width, and does this:
	 * host_start = (uint8_t *)dst_address + (dst_pitch * p_rect->y);
	 * In other words, it is only good for reading the full buffer in
	 * one go.
	 */
	vc_dispmanx_rect_set(&rect, 0, 0, fb_width, fb_height);

	if (x == 0 && y == 0 && width == fb_width && height == fb_height) {
		ret = vc_dispmanx_resource_read_data(buffer->handle, &rect,
						     pixels, dst_pitch);
		if (ret) {
			weston_log("rpi-renderer error: "
				   "resource_read_data returned %d\n", ret);
			return -1;
		}
		DBG("%s: full frame done.\n", __func__);
		return 0;
	}

	if (!output->capture_data) {
		output->capture_data = malloc(fb_height * dst_pitch);
		if (!output->capture_data) {
			weston_log("rpi-renderer error: "
				   "out of memory\n");
			return -1;
		}

		ret = vc_dispmanx_resource_read_data(buffer->handle, &rect,
						     output->capture_data,
						     dst_pitch);
		if (ret) {
			weston_log("rpi-renderer error: "
				   "resource_read_data returned %d\n", ret);
			return -1;
		}
	}

	for (i = 0; i < height; i++) {
		uint8_t *src = output->capture_data +
				(y + i) * dst_pitch + x * 4;
		uint8_t *dst = (uint8_t *)pixels + i * width * 4;
		memcpy(dst, src, width * 4);
	}

	return 0;
}

static void
rpir_output_dmx_remove_all(struct rpir_output *output,
			   DISPMANX_UPDATE_HANDLE_T update)
{
	struct rpir_surface *surface;

	while (!wl_list_empty(&output->surface_list)) {
		surface = container_of(output->surface_list.next,
				       struct rpir_surface, link);
		rpir_surface_dmx_remove(surface, update);

		wl_list_remove(&surface->link);
		wl_list_insert(&output->surface_cleanup_list, &surface->link);
	}
}

static void
output_compute_matrix(struct weston_output *base)
{
	struct rpir_output *output = to_rpir_output(base);
	struct weston_matrix *matrix = &output->matrix;
	const float half_w = 0.5f * base->width;
	const float half_h = 0.5f * base->height;
	float mag;
	float dx, dy;

	weston_matrix_init(matrix);
	weston_matrix_translate(matrix, -base->x, -base->y, 0.0f);

#ifdef SURFACE_TRANSFORM
	weston_matrix_translate(matrix, -half_w, -half_h, 0.0f);
	switch (base->transform) {
	case WL_OUTPUT_TRANSFORM_FLIPPED:
		weston_matrix_scale(matrix, -1.0f, 1.0f, 1.0f);
	case WL_OUTPUT_TRANSFORM_NORMAL:
		/* weston_matrix_rotate_xy(matrix, 1.0f, 0.0f); no-op */
		weston_matrix_translate(matrix, half_w, half_h, 0.0f);
		break;

	case WL_OUTPUT_TRANSFORM_FLIPPED_90:
		weston_matrix_scale(matrix, -1.0f, 1.0f, 1.0f);
	case WL_OUTPUT_TRANSFORM_90:
		weston_matrix_rotate_xy(matrix, 0.0f, 1.0f);
		weston_matrix_translate(matrix, half_h, half_w, 0.0f);
		break;

	case WL_OUTPUT_TRANSFORM_FLIPPED_180:
		weston_matrix_scale(matrix, -1.0f, 1.0f, 1.0f);
	case WL_OUTPUT_TRANSFORM_180:
		weston_matrix_rotate_xy(matrix, -1.0f, 0.0f);
		weston_matrix_translate(matrix, half_w, half_h, 0.0f);
		break;

	case WL_OUTPUT_TRANSFORM_FLIPPED_270:
		weston_matrix_scale(matrix, -1.0f, 1.0f, 1.0f);
	case WL_OUTPUT_TRANSFORM_270:
		weston_matrix_rotate_xy(matrix, 0.0f, -1.0f);
		weston_matrix_translate(matrix, half_h, half_w, 0.0f);
		break;

	default:
		break;
	}
#endif

	if (base->zoom.active) {
		/* The base->zoom stuff is in GL coordinate system */
		mag = 1.0f / (1.0f - base->zoom.spring_z.current);
		dx = -(base->zoom.trans_x + 1.0f) * half_w;
		dy = -(base->zoom.trans_y + 1.0f) * half_h;
		weston_matrix_translate(matrix, dx, dy, 0.0f);
		weston_matrix_scale(matrix, mag, mag, 1.0f);
		weston_matrix_translate(matrix, half_w, half_h, 0.0f);
	}
}

/* Note: this won't work right for multiple outputs. A DispmanX Element
 * is tied to one DispmanX Display, i.e. output.
 */
static void
rpi_renderer_repaint_output(struct weston_output *base,
			    pixman_region32_t *output_damage)
{
	struct weston_compositor *compositor = base->compositor;
	struct rpir_output *output = to_rpir_output(base);
	struct weston_surface *ws;
	struct rpir_surface *surface;
	struct wl_list done_list;
	int layer = 1;

	assert(output->update != DISPMANX_NO_HANDLE);

	output_compute_matrix(base);

	rpi_resource_release(&output->capture_buffer);
	free(output->capture_data);
	output->capture_data = NULL;

	/* update all renderable surfaces */
	wl_list_init(&done_list);
	wl_list_for_each_reverse(ws, &compositor->surface_list, link) {
		if (ws->plane != &compositor->primary_plane)
			continue;

		surface = to_rpir_surface(ws);
		assert(!wl_list_empty(&surface->link) ||
		       surface->handle == DISPMANX_NO_HANDLE);

		wl_list_remove(&surface->link);
		wl_list_insert(&done_list, &surface->link);
		rpir_surface_update(surface, output, output->update, layer++);
	}

	/* Remove all surfaces that are still on screen, but were
	 * not rendered this time.
	 */
	rpir_output_dmx_remove_all(output, output->update);

	wl_list_insert_list(&output->surface_list, &done_list);
	output->update = DISPMANX_NO_HANDLE;

	/* The frame_signal is emitted in rpi_renderer_finish_frame(),
	 * so that the firmware can capture the up-to-date contents.
	 */
}

static void
rpi_renderer_flush_damage(struct weston_surface *base)
{
	/* Called for every surface just before repainting it, if
	 * having an shm buffer.
	 */
	struct rpir_surface *surface = to_rpir_surface(base);
	struct weston_buffer *buffer = surface->buffer_ref.buffer;
	int ret;

	assert(buffer);
	assert(wl_shm_buffer_get(buffer->resource));

	ret = rpir_surface_damage(surface, buffer, &base->damage);
	if (ret)
		weston_log("%s error: updating Dispmanx resource failed.\n",
			   __func__);

	weston_buffer_reference(&surface->buffer_ref, NULL);
}

static void
rpi_renderer_attach(struct weston_surface *base, struct weston_buffer *buffer)
{
	/* Called every time a client commits an attach. */
	struct rpir_surface *surface = to_rpir_surface(base);

	assert(surface);
	if (!surface)
		return;

	if (surface->buffer_type == BUFFER_TYPE_SHM) {
		if (!surface->single_buffer)
			/* XXX: need to check if in middle of update */
			rpi_resource_release(surface->back);

		if (surface->handle == DISPMANX_NO_HANDLE)
			/* XXX: cannot do this, if middle of an update */
			rpi_resource_release(surface->front);

		weston_buffer_reference(&surface->buffer_ref, NULL);
	}

	/* If buffer is NULL, Weston core unmaps the surface, the surface
	 * will not appear in repaint list, and so rpi_renderer_repaint_output
	 * will remove the DispmanX element. Later, for SHM, also the front
	 * buffer will be released in the cleanup_list processing.
	 */
	if (!buffer)
		return;

	if (wl_shm_buffer_get(buffer->resource)) {
		surface->buffer_type = BUFFER_TYPE_SHM;
		buffer->shm_buffer = wl_shm_buffer_get(buffer->resource);
		buffer->width = wl_shm_buffer_get_width(buffer->shm_buffer);
		buffer->height = wl_shm_buffer_get_height(buffer->shm_buffer);

		weston_buffer_reference(&surface->buffer_ref, buffer);
	} else {
#if ENABLE_EGL
		struct rpi_renderer *renderer = to_rpi_renderer(base->compositor);
		struct wl_resource *wl_resource = buffer->resource;

		if (!renderer->has_bind_display ||
		    !renderer->query_buffer(renderer->egl_display,
					    wl_resource,
					    EGL_WIDTH, &buffer->width)) {
			weston_log("unhandled buffer type!\n");
			weston_buffer_reference(&surface->buffer_ref, NULL);
			surface->buffer_type = BUFFER_TYPE_NULL;
		}

		renderer->query_buffer(renderer->egl_display,
				       wl_resource,
				       EGL_HEIGHT, &buffer->height);

		surface->buffer_type = BUFFER_TYPE_EGL;

		if(surface->egl_back == NULL)
			surface->egl_back = calloc(1, sizeof *surface->egl_back);

		weston_buffer_reference(&surface->egl_back->buffer_ref, buffer);
		surface->egl_back->resource_handle =
			vc_dispmanx_get_handle_from_wl_buffer(wl_resource);
#else
		weston_log("unhandled buffer type!\n");
		weston_buffer_reference(&surface->buffer_ref, NULL);
		surface->buffer_type = BUFFER_TYPE_NULL;
#endif
	}
}

static int
rpi_renderer_create_surface(struct weston_surface *base)
{
	struct rpi_renderer *renderer = to_rpi_renderer(base->compositor);
	struct rpir_surface *surface;

	assert(base->renderer_state == NULL);

	surface = rpir_surface_create(renderer);
	if (!surface)
		return -1;

	surface->surface = base;
	base->renderer_state = surface;
	return 0;
}

static void
rpi_renderer_surface_set_color(struct weston_surface *base,
			       float red, float green, float blue, float alpha)
{
	struct rpir_surface *surface = to_rpir_surface(base);
	uint8_t color[4];
	VC_RECT_T rect;
	int ret;

	assert(surface);

	ret = rpi_resource_realloc(surface->back, VC_IMAGE_ARGB8888,
				   1, 1, 4, 1);
	if (ret < 0) {
		weston_log("Error: %s: rpi_resource_realloc failed.\n",
			   __func__);
		return;
	}

	color[0] = float2uint8(blue);
	color[1] = float2uint8(green);
	color[2] = float2uint8(red);
	color[3] = float2uint8(alpha);

	vc_dispmanx_rect_set(&rect, 0, 0, 1, 1);
	ret = vc_dispmanx_resource_write_data(surface->back->handle,
					      VC_IMAGE_ARGB8888,
					      4, color, &rect);
	if (ret) {
		weston_log("Error: %s: resource_write_data failed.\n",
			   __func__);
		return;
	}

	DBG("%s: resource %p solid color BGRA %u,%u,%u,%u\n", __func__,
	    surface->back, color[0], color[1], color[2], color[3]);

	/*pixman_region32_copy(&surface->prev_damage, damage);*/
	surface->need_swap = 1;
}

static void
rpi_renderer_destroy_surface(struct weston_surface *base)
{
	struct rpir_surface *surface = to_rpir_surface(base);

	assert(surface);
	assert(surface->surface == base);
	if (!surface)
		return;

	surface->surface = NULL;
	base->renderer_state = NULL;

	/* If guaranteed to not be on screen, just detroy it. */
	if (wl_list_empty(&surface->link))
		rpir_surface_destroy(surface);

	/* Otherwise, the surface is either on screen and needs
	 * to be removed by a repaint update, or it is in the
	 * surface_cleanup_list, and will be destroyed by
	 * rpi_renderer_finish_frame().
	 */
}

static void
rpi_renderer_destroy(struct weston_compositor *compositor)
{
	struct rpi_renderer *renderer = to_rpi_renderer(compositor);

#if ENABLE_EGL
	if (renderer->has_bind_display)
		renderer->unbind_display(renderer->egl_display,
		                         compositor->wl_display);
#endif

	free(renderer);
	compositor->renderer = NULL;
}

WL_EXPORT int
rpi_renderer_create(struct weston_compositor *compositor,
		    const struct rpi_renderer_parameters *params)
{
	struct rpi_renderer *renderer;
#if ENABLE_EGL
	const char *extensions;
	EGLBoolean ret;
	EGLint major, minor;
#endif

	weston_log("Initializing the DispmanX compositing renderer\n");

	renderer = calloc(1, sizeof *renderer);
	if (renderer == NULL)
		return -1;

	renderer->single_buffer = params->single_buffer;

	renderer->base.read_pixels = rpi_renderer_read_pixels;
	renderer->base.repaint_output = rpi_renderer_repaint_output;
	renderer->base.flush_damage = rpi_renderer_flush_damage;
	renderer->base.attach = rpi_renderer_attach;
	renderer->base.create_surface = rpi_renderer_create_surface;
	renderer->base.surface_set_color = rpi_renderer_surface_set_color;
	renderer->base.destroy_surface = rpi_renderer_destroy_surface;
	renderer->base.destroy = rpi_renderer_destroy;

#ifdef ENABLE_EGL
	renderer->egl_display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
	if (renderer->egl_display == EGL_NO_DISPLAY) {
		weston_log("failed to create EGL display\n");
		return -1;
	}

	if (!eglInitialize(renderer->egl_display, &major, &minor)) {
		weston_log("failed to initialize EGL display\n");
		return -1;
	}

	renderer->bind_display =
		(void *) eglGetProcAddress("eglBindWaylandDisplayWL");
	renderer->unbind_display =
		(void *) eglGetProcAddress("eglUnbindWaylandDisplayWL");
	renderer->query_buffer =
		(void *) eglGetProcAddress("eglQueryWaylandBufferWL");

	extensions = (const char *) eglQueryString(renderer->egl_display,
						   EGL_EXTENSIONS);
	if (!extensions) {
		weston_log("Retrieving EGL extension string failed.\n");
		return -1;
	}

	if (strstr(extensions, "EGL_WL_bind_wayland_display"))
		renderer->has_bind_display = 1;

	if (renderer->has_bind_display) {
		ret = renderer->bind_display(renderer->egl_display,
					     compositor->wl_display);
		if (!ret)
			renderer->has_bind_display = 0;
	}
#endif

	compositor->renderer = &renderer->base;
	compositor->read_format = PIXMAN_a8r8g8b8;
	/* WESTON_CAP_ROTATION_ANY not supported */

	wl_display_add_shm_format(compositor->wl_display, WL_SHM_FORMAT_RGB565);

	return 0;
}

WL_EXPORT int
rpi_renderer_output_create(struct weston_output *base,
			   DISPMANX_DISPLAY_HANDLE_T display)
{
	struct rpir_output *output;

	assert(base->renderer_state == NULL);

	output = calloc(1, sizeof *output);
	if (!output)
		return -1;

	output->display = display;
	output->update = DISPMANX_NO_HANDLE;
	wl_list_init(&output->surface_list);
	wl_list_init(&output->surface_cleanup_list);
	rpi_resource_init(&output->capture_buffer);
	base->renderer_state = output;

	return 0;
}

WL_EXPORT void
rpi_renderer_output_destroy(struct weston_output *base)
{
	struct rpir_output *output = to_rpir_output(base);
	struct rpir_surface *surface;
	DISPMANX_UPDATE_HANDLE_T update;

	rpi_resource_release(&output->capture_buffer);
	free(output->capture_data);
	output->capture_data = NULL;

	update = vc_dispmanx_update_start(0);
	rpir_output_dmx_remove_all(output, update);
	vc_dispmanx_update_submit_sync(update);

	while (!wl_list_empty(&output->surface_cleanup_list)) {
		surface = container_of(output->surface_cleanup_list.next,
				       struct rpir_surface, link);
		if (surface->surface)
			surface->surface->renderer_state = NULL;
		rpir_surface_destroy(surface);
	}

	free(output);
	base->renderer_state = NULL;
}

WL_EXPORT void
rpi_renderer_set_update_handle(struct weston_output *base,
			       DISPMANX_UPDATE_HANDLE_T handle)
{
	struct rpir_output *output = to_rpir_output(base);

	output->update = handle;
}

WL_EXPORT void
rpi_renderer_finish_frame(struct weston_output *base)
{
	struct rpir_output *output = to_rpir_output(base);
	struct weston_compositor *compositor = base->compositor;
	struct weston_surface *ws;
	struct rpir_surface *surface;

	while (!wl_list_empty(&output->surface_cleanup_list)) {
		surface = container_of(output->surface_cleanup_list.next,
				       struct rpir_surface, link);

		if (surface->surface) {
			/* The weston_surface still exists, but is
			 * temporarily not visible, and hence its Element
			 * was removed. The current front buffer contents
			 * must be preserved.
			 */
			if (!surface->single_buffer)
				rpi_resource_release(surface->back);

			wl_list_remove(&surface->link);
			wl_list_init(&surface->link);
		} else {
			rpir_surface_destroy(surface);
		}
	}

	wl_list_for_each(ws, &compositor->surface_list, link) {
		surface = to_rpir_surface(ws);

		if (surface->buffer_type != BUFFER_TYPE_EGL)
			continue;

		if(surface->egl_old_front == NULL)
			continue;

		weston_buffer_reference(&surface->egl_old_front->buffer_ref, NULL);
		free(surface->egl_old_front);
		surface->egl_old_front = NULL;
	}

	wl_signal_emit(&base->frame_signal, base);
}
