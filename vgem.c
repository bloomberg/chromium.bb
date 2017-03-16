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

static const uint32_t supported_formats[] = {
	DRM_FORMAT_ARGB8888, DRM_FORMAT_RGB565, DRM_FORMAT_XRGB8888,
	DRM_FORMAT_YVU420_ANDROID
};

static int vgem_init(struct driver *drv)
{
	return drv_add_linear_combinations(drv, supported_formats,
					   ARRAY_SIZE(supported_formats));
}

static int vgem_bo_create(struct bo *bo, uint32_t width, uint32_t height,
			  uint32_t format, uint32_t flags)
{
	int ret = drv_dumb_bo_create(bo, ALIGN(width, MESA_LLVMPIPE_TILE_SIZE),
				     ALIGN(height, MESA_LLVMPIPE_TILE_SIZE),
				     format, flags);
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

struct backend backend_vgem =
{
	.name = "vgem",
	.init = vgem_init,
	.bo_create = vgem_bo_create,
	.bo_destroy = drv_dumb_bo_destroy,
	.bo_import = drv_prime_bo_import,
	.bo_map = drv_dumb_bo_map,
	.resolve_format = vgem_resolve_format,
};

