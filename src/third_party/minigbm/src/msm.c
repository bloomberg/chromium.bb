/*
 * Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifdef DRV_MSM

#include <assert.h>
#include <drm_fourcc.h>
#include <errno.h>
#include <inttypes.h>
#include <msm_drm.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <xf86drm.h>

#include "drv_priv.h"
#include "helpers.h"
#include "util.h"

/* Alignment values are based on SDM845 Gfx IP */
#define DEFAULT_ALIGNMENT 64
#define BUFFER_SIZE_ALIGN 4096

#define VENUS_STRIDE_ALIGN 128
#define VENUS_SCANLINE_ALIGN 16
#define NV12_LINEAR_PADDING (12 * 1024)
#define NV12_UBWC_PADDING(y_stride) (MAX(16 * 1024, y_stride * 48))
#define MACROTILE_WIDTH_ALIGN 64
#define MACROTILE_HEIGHT_ALIGN 16
#define PLANE_SIZE_ALIGN 4096

#define MSM_UBWC_TILING 1

static const uint32_t render_target_formats[] = { DRM_FORMAT_ABGR8888, DRM_FORMAT_ARGB8888,
						  DRM_FORMAT_RGB565, DRM_FORMAT_XBGR8888,
						  DRM_FORMAT_XRGB8888 };

static const uint32_t texture_source_formats[] = { DRM_FORMAT_NV12, DRM_FORMAT_R8,
						   DRM_FORMAT_YVU420, DRM_FORMAT_YVU420_ANDROID };

/*
 * Each macrotile consists of m x n (mostly 4 x 4) tiles.
 * Pixel data pitch/stride is aligned with macrotile width.
 * Pixel data height is aligned with macrotile height.
 * Entire pixel data buffer is aligned with 4k(bytes).
 */
static uint32_t get_ubwc_meta_size(uint32_t width, uint32_t height, uint32_t tile_width,
				   uint32_t tile_height)
{
	uint32_t macrotile_width, macrotile_height;

	macrotile_width = DIV_ROUND_UP(width, tile_width);
	macrotile_height = DIV_ROUND_UP(height, tile_height);

	// Align meta buffer width to 64 blocks
	macrotile_width = ALIGN(macrotile_width, MACROTILE_WIDTH_ALIGN);

	// Align meta buffer height to 16 blocks
	macrotile_height = ALIGN(macrotile_height, MACROTILE_HEIGHT_ALIGN);

	return ALIGN(macrotile_width * macrotile_height, PLANE_SIZE_ALIGN);
}

static void msm_calculate_layout(struct bo *bo)
{
	uint32_t width, height;

	width = bo->meta.width;
	height = bo->meta.height;

	/* NV12 format requires extra padding with platform
	 * specific alignments for venus driver
	 */
	if (bo->meta.format == DRM_FORMAT_NV12) {
		uint32_t y_stride, uv_stride, y_scanline, uv_scanline, y_plane, uv_plane, size,
		    extra_padding;

		y_stride = ALIGN(width, VENUS_STRIDE_ALIGN);
		uv_stride = ALIGN(width, VENUS_STRIDE_ALIGN);
		y_scanline = ALIGN(height, VENUS_SCANLINE_ALIGN * 2);
		uv_scanline = ALIGN(DIV_ROUND_UP(height, 2), VENUS_SCANLINE_ALIGN);
		y_plane = y_stride * y_scanline;
		uv_plane = uv_stride * uv_scanline;

		if (bo->meta.tiling == MSM_UBWC_TILING) {
			y_plane += get_ubwc_meta_size(width, height, 32, 8);
			uv_plane += get_ubwc_meta_size(width >> 1, height >> 1, 16, 8);
			extra_padding = NV12_UBWC_PADDING(y_stride);
		} else {
			extra_padding = NV12_LINEAR_PADDING;
		}

		bo->meta.strides[0] = y_stride;
		bo->meta.sizes[0] = y_plane;
		bo->meta.offsets[1] = y_plane;
		bo->meta.strides[1] = uv_stride;
		size = y_plane + uv_plane + extra_padding;
		bo->meta.total_size = ALIGN(size, BUFFER_SIZE_ALIGN);
		bo->meta.sizes[1] = bo->meta.total_size - bo->meta.sizes[0];
	} else {
		uint32_t stride, alignw, alignh;

		alignw = ALIGN(width, DEFAULT_ALIGNMENT);
		/* HAL_PIXEL_FORMAT_YV12 requires that the buffer's height not be aligned.
			DRM_FORMAT_R8 of height one is used for JPEG camera output, so don't
			height align that. */
		if (bo->meta.format == DRM_FORMAT_YVU420_ANDROID ||
		    (bo->meta.format == DRM_FORMAT_R8 && height == 1)) {
			alignh = height;
		} else {
			alignh = ALIGN(height, DEFAULT_ALIGNMENT);
		}

		stride = drv_stride_from_format(bo->meta.format, alignw, 0);

		/* Calculate size and assign stride, size, offset to each plane based on format */
		drv_bo_from_format(bo, stride, alignh, bo->meta.format);

		/* For all RGB UBWC formats */
		if (bo->meta.tiling == MSM_UBWC_TILING) {
			bo->meta.sizes[0] += get_ubwc_meta_size(width, height, 16, 4);
			bo->meta.total_size = bo->meta.sizes[0];
			assert(IS_ALIGNED(bo->meta.total_size, BUFFER_SIZE_ALIGN));
		}
	}
}

static bool is_ubwc_fmt(uint32_t format)
{
	switch (format) {
	case DRM_FORMAT_XBGR8888:
	case DRM_FORMAT_ABGR8888:
	case DRM_FORMAT_XRGB8888:
	case DRM_FORMAT_ARGB8888:
	case DRM_FORMAT_NV12:
		return 1;
	default:
		return 0;
	}
}

static void msm_add_ubwc_combinations(struct driver *drv, const uint32_t *formats,
				      uint32_t num_formats, struct format_metadata *metadata,
				      uint64_t use_flags)
{
	for (uint32_t i = 0; i < num_formats; i++) {
		if (is_ubwc_fmt(formats[i])) {
			struct combination combo = { .format = formats[i],
						     .metadata = *metadata,
						     .use_flags = use_flags };
			drv_array_append(drv->combos, &combo);
		}
	}
}

static int msm_init(struct driver *drv)
{
	struct format_metadata metadata;
	uint64_t render_use_flags = BO_USE_RENDER_MASK | BO_USE_SCANOUT;
	uint64_t texture_use_flags = BO_USE_TEXTURE_MASK | BO_USE_HW_VIDEO_DECODER;
	uint64_t sw_flags = (BO_USE_RENDERSCRIPT | BO_USE_SW_WRITE_OFTEN | BO_USE_SW_READ_OFTEN |
			     BO_USE_LINEAR | BO_USE_PROTECTED);

	drv_add_combinations(drv, render_target_formats, ARRAY_SIZE(render_target_formats),
			     &LINEAR_METADATA, render_use_flags);

	drv_add_combinations(drv, texture_source_formats, ARRAY_SIZE(texture_source_formats),
			     &LINEAR_METADATA, texture_use_flags);

	/*
	 * Chrome uses DMA-buf mmap to write to YV12 buffers, which are then accessed by the
	 * Video Encoder Accelerator (VEA). It could also support NV12 potentially in the future.
	 */
	drv_modify_combination(drv, DRM_FORMAT_YVU420, &LINEAR_METADATA, BO_USE_HW_VIDEO_ENCODER);
	drv_modify_combination(drv, DRM_FORMAT_NV12, &LINEAR_METADATA, BO_USE_HW_VIDEO_ENCODER);

	/* The camera stack standardizes on NV12 for YUV buffers. */
	drv_modify_combination(drv, DRM_FORMAT_NV12, &LINEAR_METADATA,
			       BO_USE_CAMERA_READ | BO_USE_CAMERA_WRITE | BO_USE_SCANOUT);
	/*
	 * R8 format is used for Android's HAL_PIXEL_FORMAT_BLOB and is used for JPEG snapshots
	 * from camera.
	 */
	drv_modify_combination(drv, DRM_FORMAT_R8, &LINEAR_METADATA,
			       BO_USE_CAMERA_READ | BO_USE_CAMERA_WRITE);

	/* Android CTS tests require this. */
	drv_add_combination(drv, DRM_FORMAT_BGR888, &LINEAR_METADATA, BO_USE_SW_MASK);

	drv_modify_linear_combinations(drv);

	metadata.tiling = MSM_UBWC_TILING;
	metadata.priority = 2;
	metadata.modifier = DRM_FORMAT_MOD_QCOM_COMPRESSED;

	render_use_flags &= ~sw_flags;
	texture_use_flags &= ~sw_flags;

	msm_add_ubwc_combinations(drv, render_target_formats, ARRAY_SIZE(render_target_formats),
				  &metadata, render_use_flags);

	msm_add_ubwc_combinations(drv, texture_source_formats, ARRAY_SIZE(texture_source_formats),
				  &metadata, texture_use_flags);

	return 0;
}

static int msm_bo_create_for_modifier(struct bo *bo, uint32_t width, uint32_t height,
				      uint32_t format, const uint64_t modifier)
{
	struct drm_msm_gem_new req;
	int ret;
	size_t i;

	bo->meta.tiling = (modifier == DRM_FORMAT_MOD_QCOM_COMPRESSED) ? MSM_UBWC_TILING : 0;

	msm_calculate_layout(bo);

	memset(&req, 0, sizeof(req));
	req.flags = MSM_BO_WC | MSM_BO_SCANOUT;
	req.size = bo->meta.total_size;

	ret = drmIoctl(bo->drv->fd, DRM_IOCTL_MSM_GEM_NEW, &req);
	if (ret) {
		drv_log("DRM_IOCTL_MSM_GEM_NEW failed with %s\n", strerror(errno));
		return -errno;
	}

	/*
	 * Though we use only one plane, we need to set handle for
	 * all planes to pass kernel checks
	 */
	for (i = 0; i < bo->meta.num_planes; i++) {
		bo->handles[i].u32 = req.handle;
		bo->meta.format_modifiers[i] = modifier;
	}

	return 0;
}

static int msm_bo_create_with_modifiers(struct bo *bo, uint32_t width, uint32_t height,
					uint32_t format, const uint64_t *modifiers, uint32_t count)
{
	static const uint64_t modifier_order[] = {
		DRM_FORMAT_MOD_QCOM_COMPRESSED,
		DRM_FORMAT_MOD_LINEAR,
	};

	uint64_t modifier =
	    drv_pick_modifier(modifiers, count, modifier_order, ARRAY_SIZE(modifier_order));

	return msm_bo_create_for_modifier(bo, width, height, format, modifier);
}

/* msm_bo_create will create linear buffers for now */
static int msm_bo_create(struct bo *bo, uint32_t width, uint32_t height, uint32_t format,
			 uint64_t flags)
{
	struct combination *combo = drv_get_combination(bo->drv, format, flags);

	if (!combo) {
		drv_log("invalid format = %d, flags = %" PRIx64 " combination\n", format, flags);
		return -EINVAL;
	}

	return msm_bo_create_for_modifier(bo, width, height, format, combo->metadata.modifier);
}

static void *msm_bo_map(struct bo *bo, struct vma *vma, size_t plane, uint32_t map_flags)
{
	int ret;
	struct drm_msm_gem_info req;

	memset(&req, 0, sizeof(req));
	req.handle = bo->handles[0].u32;

	ret = drmIoctl(bo->drv->fd, DRM_IOCTL_MSM_GEM_INFO, &req);
	if (ret) {
		drv_log("DRM_IOCLT_MSM_GEM_INFO failed with %s\n", strerror(errno));
		return MAP_FAILED;
	}
	vma->length = bo->meta.total_size;

	return mmap(0, bo->meta.total_size, drv_get_prot(map_flags), MAP_SHARED, bo->drv->fd,
		    req.offset);
}

static uint32_t msm_resolve_format(struct driver *drv, uint32_t format, uint64_t use_flags)
{
	switch (format) {
	case DRM_FORMAT_FLEX_YCbCr_420_888:
		return DRM_FORMAT_NV12;
	default:
		return format;
	}
}

const struct backend backend_msm = {
	.name = "msm",
	.init = msm_init,
	.bo_create = msm_bo_create,
	.bo_create_with_modifiers = msm_bo_create_with_modifiers,
	.bo_destroy = drv_gem_bo_destroy,
	.bo_import = drv_prime_bo_import,
	.bo_map = msm_bo_map,
	.bo_unmap = drv_bo_munmap,
	.resolve_format = msm_resolve_format,
};
#endif /* DRV_MSM */
