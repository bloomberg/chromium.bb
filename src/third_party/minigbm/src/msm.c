/*
 * Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifdef DRV_MSM

#include "drv_priv.h"
#include "helpers.h"
#include "util.h"

#define MESA_LLVMPIPE_TILE_ORDER 6
#define MESA_LLVMPIPE_TILE_SIZE (1 << MESA_LLVMPIPE_TILE_ORDER)

static const uint32_t render_target_formats[] = { DRM_FORMAT_ARGB8888, DRM_FORMAT_BGR888,
						  DRM_FORMAT_XRGB8888 };

static const uint32_t supported_formats[] = { DRM_FORMAT_NV12 };

static int msm_init(struct driver *drv)
{
	drv_add_combinations(drv, render_target_formats, ARRAY_SIZE(render_target_formats),
			     &LINEAR_METADATA, BO_USE_RENDER_MASK);

	drv_add_combinations(drv, supported_formats, ARRAY_SIZE(supported_formats),
			     &LINEAR_METADATA, BO_USE_TEXTURE_MASK | BO_USE_HW_VIDEO_DECODER);

	return drv_modify_linear_combinations(drv);
}

static int msm_bo_create(struct bo *bo, uint32_t width, uint32_t height, uint32_t format,
			 uint64_t flags)
{
	width = ALIGN(width, MESA_LLVMPIPE_TILE_SIZE);
	height = ALIGN(height, MESA_LLVMPIPE_TILE_SIZE);

	/* HAL_PIXEL_FORMAT_YV12 requires that the buffer's height not be aligned. */
	if (bo->format == DRM_FORMAT_YVU420_ANDROID)
		height = bo->height;

	/* Adjust the height of NV12 buffers to include room for the UV plane */
	/* The extra 12KB at the end are a requirement of the Venus codec driver */
	if (bo->format == DRM_FORMAT_NV12)
		height += DIV_ROUND_UP(height, 2) + DIV_ROUND_UP(0x3000, width);

	return drv_dumb_bo_create(bo, width, height, format, flags);
}

const struct backend backend_msm = {
	.name = "msm",
	.init = msm_init,
	.bo_create = msm_bo_create,
	.bo_destroy = drv_dumb_bo_destroy,
	.bo_import = drv_prime_bo_import,
	.bo_map = drv_dumb_bo_map,
	.bo_unmap = drv_bo_munmap,
};

#endif /* DRV_MSM */
