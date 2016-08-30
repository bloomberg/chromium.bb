/*
 * Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifdef DRV_ROCKCHIP

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <xf86drm.h>
#include <rockchip_drm.h>

#include "drv_priv.h"
#include "helpers.h"
#include "util.h"

static int rockchip_bo_create(struct bo *bo, uint32_t width, uint32_t height,
			      uint32_t format, uint32_t flags)
{
	int ret;
	size_t plane;
	struct drm_rockchip_gem_create gem_create;

	if (format == DRV_FORMAT_NV12) {
		width = ALIGN(width, 4);
		height = ALIGN(height, 4);
	}

	drv_bo_from_format(bo, width, height, format);

	memset(&gem_create, 0, sizeof(gem_create));
	gem_create.size = bo->total_size;

	ret = drmIoctl(bo->drv->fd, DRM_IOCTL_ROCKCHIP_GEM_CREATE,
			   &gem_create);

	if (ret) {
		fprintf(stderr, "drv: DRM_IOCTL_ROCKCHIP_GEM_CREATE failed "
				"(size=%llu)\n", gem_create.size);
		return ret;
	}

	for (plane = 0; plane < bo->num_planes; plane++)
		bo->handles[plane].u32 = gem_create.handle;

	return 0;
}

static void *rockchip_bo_map(struct bo *bo)
{
	int ret;
	struct drm_rockchip_gem_map_off gem_map;

	memset(&gem_map, 0, sizeof(gem_map));
	gem_map.handle = bo->handles[0].u32;

	ret = drmIoctl(bo->drv->fd, DRM_IOCTL_ROCKCHIP_GEM_MAP_OFFSET,
		       &gem_map);
	if (ret) {
		fprintf(stderr,
			"drv: DRM_IOCTL_ROCKCHIP_GEM_MAP_OFFSET failed\n");
		return MAP_FAILED;
	}

	return mmap(0, bo->total_size, PROT_READ | PROT_WRITE, MAP_SHARED,
		    bo->drv->fd, gem_map.offset);
}

static drv_format_t rockchip_resolve_format(drv_format_t format)
{
	switch (format) {
	case DRV_FORMAT_FLEX_IMPLEMENTATION_DEFINED:
		/*HACK: See b/28671744 */
		return DRV_FORMAT_XBGR8888;
	case DRV_FORMAT_FLEX_YCbCr_420_888:
		return DRV_FORMAT_NV12;
	default:
		return format;
	}
}

const struct backend backend_rockchip =
{
	.name = "rockchip",
	.bo_create = rockchip_bo_create,
	.bo_destroy = drv_gem_bo_destroy,
	.bo_map = rockchip_bo_map,
	.resolve_format = rockchip_resolve_format,
	.format_list = {
		{DRV_FORMAT_XRGB8888, DRV_BO_USE_SCANOUT | DRV_BO_USE_CURSOR |
				      DRV_BO_USE_RENDERING | DRV_BO_USE_HW_TEXTURE |
				      DRV_BO_USE_HW_RENDER | DRV_BO_USE_HW_2D |
				      DRV_BO_USE_SW_READ_RARELY | DRV_BO_USE_SW_WRITE_RARELY},
		{DRV_FORMAT_XRGB8888, DRV_BO_USE_SCANOUT | DRV_BO_USE_CURSOR | DRV_BO_USE_LINEAR |
				      DRV_BO_USE_SW_READ_OFTEN | DRV_BO_USE_SW_WRITE_OFTEN},
		{DRV_FORMAT_XBGR8888, DRV_BO_USE_SCANOUT | DRV_BO_USE_CURSOR |
				      DRV_BO_USE_RENDERING | DRV_BO_USE_HW_TEXTURE |
				      DRV_BO_USE_HW_RENDER | DRV_BO_USE_HW_2D |
				      DRV_BO_USE_SW_READ_RARELY | DRV_BO_USE_SW_WRITE_RARELY},
		{DRV_FORMAT_XBGR8888, DRV_BO_USE_SCANOUT | DRV_BO_USE_CURSOR | DRV_BO_USE_LINEAR |
				      DRV_BO_USE_SW_READ_OFTEN | DRV_BO_USE_SW_WRITE_OFTEN},
		{DRV_FORMAT_ARGB8888, DRV_BO_USE_SCANOUT | DRV_BO_USE_CURSOR |
				      DRV_BO_USE_RENDERING | DRV_BO_USE_HW_TEXTURE |
				      DRV_BO_USE_HW_RENDER | DRV_BO_USE_HW_2D |
				      DRV_BO_USE_SW_READ_RARELY | DRV_BO_USE_SW_WRITE_RARELY},
		{DRV_FORMAT_ARGB8888, DRV_BO_USE_SCANOUT | DRV_BO_USE_CURSOR | DRV_BO_USE_LINEAR |
				      DRV_BO_USE_SW_READ_OFTEN | DRV_BO_USE_SW_WRITE_OFTEN},
		{DRV_FORMAT_ABGR8888, DRV_BO_USE_SCANOUT | DRV_BO_USE_CURSOR |
				      DRV_BO_USE_RENDERING | DRV_BO_USE_HW_TEXTURE |
				      DRV_BO_USE_HW_RENDER | DRV_BO_USE_HW_2D |
				      DRV_BO_USE_SW_READ_RARELY | DRV_BO_USE_SW_WRITE_RARELY},
		{DRV_FORMAT_NV12,     DRV_BO_USE_SCANOUT | DRV_BO_USE_RENDERING |
				      DRV_BO_USE_HW_TEXTURE | DRV_BO_USE_HW_RENDER |
				      DRV_BO_USE_HW_2D | DRV_BO_USE_SW_READ_RARELY |
				      DRV_BO_USE_SW_WRITE_RARELY},
		{DRV_FORMAT_NV12,     DRV_BO_USE_SCANOUT | DRV_BO_USE_LINEAR |
				      DRV_BO_USE_SW_READ_OFTEN | DRV_BO_USE_SW_WRITE_OFTEN},
		{DRV_FORMAT_YVU420,   DRV_BO_USE_LINEAR | DRV_BO_USE_SW_READ_OFTEN |
				      DRV_BO_USE_SW_WRITE_OFTEN},
	}
};

#endif
