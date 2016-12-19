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

static struct supported_combination combos[4] = {
	{DRM_FORMAT_ABGR8888, DRM_FORMAT_MOD_NONE,
		BO_USE_CURSOR | BO_USE_LINEAR | BO_USE_RENDERING | BO_USE_SW_READ_OFTEN |
		BO_USE_SW_WRITE_OFTEN | BO_USE_SW_READ_RARELY | BO_USE_SW_WRITE_RARELY},
	{DRM_FORMAT_RGB565, DRM_FORMAT_MOD_NONE,
		BO_USE_LINEAR | BO_USE_RENDERING | BO_USE_SW_READ_OFTEN | BO_USE_SW_WRITE_OFTEN |
		BO_USE_SW_READ_RARELY | BO_USE_SW_WRITE_RARELY},
	{DRM_FORMAT_XBGR8888, DRM_FORMAT_MOD_NONE,
		BO_USE_CURSOR | BO_USE_LINEAR | BO_USE_RENDERING | BO_USE_SW_READ_OFTEN |
		BO_USE_SW_WRITE_OFTEN | BO_USE_SW_READ_RARELY | BO_USE_SW_WRITE_RARELY},
	{DRM_FORMAT_YVU420, DRM_FORMAT_MOD_NONE,
		BO_USE_LINEAR | BO_USE_RENDERING | BO_USE_SW_READ_OFTEN | BO_USE_SW_WRITE_OFTEN |
		BO_USE_SW_READ_RARELY | BO_USE_SW_WRITE_RARELY},
};

static int vgem_init(struct driver *drv)
{
	drv_insert_combinations(drv, combos, ARRAY_SIZE(combos));
	return 0;
}

static int vgem_bo_create(struct bo *bo, uint32_t width, uint32_t height,
			  uint32_t format, uint32_t flags)
{
	int ret = drv_dumb_bo_create(bo, ALIGN(width, MESA_LLVMPIPE_TILE_SIZE),
				     ALIGN(height, MESA_LLVMPIPE_TILE_SIZE),
				     format, flags);
	bo->width = width;
	bo->height = height;

	return ret;
}

static uint32_t vgem_resolve_format(uint32_t format)
{
	switch (format) {
	case DRM_FORMAT_FLEX_IMPLEMENTATION_DEFINED:
		/*HACK: See b/28671744 */
		return DRM_FORMAT_XBGR8888;
	case DRM_FORMAT_FLEX_YCbCr_420_888:
		return DRM_FORMAT_YVU420;
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
	.bo_map = drv_dumb_bo_map,
	.resolve_format = vgem_resolve_format,
};

