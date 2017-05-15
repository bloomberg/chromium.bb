/*
 * Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "drv_priv.h"
#include "helpers.h"
#include "util.h"

#define MESA_LLVMPIPE_TILE_ORDER 6
#define MESA_LLVMPIPE_TILE_SIZE (1 << MESA_LLVMPIPE_TILE_ORDER)

static const uint32_t render_target_formats[] = { DRM_FORMAT_ABGR8888, DRM_FORMAT_ARGB8888,
						  DRM_FORMAT_RGB565, DRM_FORMAT_XBGR8888,
						  DRM_FORMAT_XRGB8888 };

static const uint32_t texture_source_formats[] = { DRM_FORMAT_R8, DRM_FORMAT_YVU420,
						   DRM_FORMAT_YVU420_ANDROID };

static int vgem_init(struct driver *drv)
{
	int ret;
	ret = drv_add_combinations(drv, render_target_formats, ARRAY_SIZE(render_target_formats),
				   &LINEAR_METADATA, BO_USE_RENDER_MASK);
	if (ret)
		return ret;

	ret = drv_add_combinations(drv, texture_source_formats, ARRAY_SIZE(texture_source_formats),
				   &LINEAR_METADATA, BO_USE_TEXTURE_MASK);
	if (ret)
		return ret;

	return drv_modify_linear_combinations(drv);
}

static int vgem_bo_create(struct bo *bo, uint32_t width, uint32_t height, uint32_t format,
			  uint32_t flags)
{
	int ret = drv_dumb_bo_create(bo, ALIGN(width, MESA_LLVMPIPE_TILE_SIZE),
				     ALIGN(height, MESA_LLVMPIPE_TILE_SIZE), format, flags);
	return ret;
}

static uint32_t vgem_resolve_format(uint32_t format)
{
	switch (format) {
	case DRM_FORMAT_FLEX_IMPLEMENTATION_DEFINED:
		/*HACK: See b/28671744 */
		return DRM_FORMAT_XBGR8888;
	case DRM_FORMAT_FLEX_YCbCr_420_888:
		return DRM_FORMAT_YVU420_ANDROID;
	default:
		return format;
	}
}

struct backend backend_vgem = {
	.name = "vgem",
	.init = vgem_init,
	.bo_create = vgem_bo_create,
	.bo_destroy = drv_dumb_bo_destroy,
	.bo_import = drv_prime_bo_import,
	.bo_map = drv_dumb_bo_map,
	.resolve_format = vgem_resolve_format,
};
