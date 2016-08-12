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

static int drv_rockchip_bo_create(struct bo *bo,
				  uint32_t width, uint32_t height,
				  uint32_t format, uint32_t flags)
{
	size_t plane;

	switch (format) {
		case DRV_FORMAT_NV12:
			width = ALIGN(width, 4);
			height = ALIGN(height, 4);
			bo->strides[0] = bo->strides[1] = width;
			bo->sizes[0] = height * bo->strides[0];
			bo->sizes[1] = height * bo->strides[1] / 2;
			bo->offsets[0] = 0;
			bo->offsets[1] = height * bo->strides[0];
			break;
		case DRV_FORMAT_XRGB8888:
		case DRV_FORMAT_ARGB8888:
		case DRV_FORMAT_ABGR8888:
			bo->strides[0] = drv_stride_from_format(format, width);
			bo->sizes[0] = height * bo->strides[0];
			bo->offsets[0] = 0;
			break;
		default:
			fprintf(stderr, "drv: rockchip: unsupported format %4.4s\n",
				(char*)&format);
			assert(0);
			return -EINVAL;
	}

	int ret;
	size_t size = 0;

	for (plane = 0; plane < bo->num_planes; plane++)
		size += bo->sizes[plane];

	struct drm_rockchip_gem_create gem_create;

	memset(&gem_create, 0, sizeof(gem_create));
	gem_create.size = size;

	ret = drmIoctl(bo->drv->fd, DRM_IOCTL_ROCKCHIP_GEM_CREATE,
			   &gem_create);

	if (ret) {
		fprintf(stderr, "drv: DRM_IOCTL_ROCKCHIP_GEM_CREATE failed "
				"(size=%zu)\n", size);
	}
	else {
		for (plane = 0; plane < bo->num_planes; plane++)
			bo->handles[plane].u32 = gem_create.handle;
	}

	return ret;
}

static void *drv_rockchip_bo_map(struct bo *bo)
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

	return mmap(0, bo->sizes[0], PROT_READ | PROT_WRITE, MAP_SHARED,
		    bo->drv->fd, gem_map.offset);
}

const struct backend backend_rockchip =
{
	.name = "rockchip",
	.bo_create = drv_rockchip_bo_create,
	.bo_destroy = drv_gem_bo_destroy,
	.bo_map = drv_rockchip_bo_map,
	.format_list = {
		{DRV_FORMAT_XRGB8888, DRV_BO_USE_SCANOUT | DRV_BO_USE_CURSOR | DRV_BO_USE_RENDERING},
		{DRV_FORMAT_XRGB8888, DRV_BO_USE_SCANOUT | DRV_BO_USE_CURSOR | DRV_BO_USE_LINEAR},
		{DRV_FORMAT_ARGB8888, DRV_BO_USE_SCANOUT | DRV_BO_USE_CURSOR | DRV_BO_USE_RENDERING},
		{DRV_FORMAT_ARGB8888, DRV_BO_USE_SCANOUT | DRV_BO_USE_CURSOR | DRV_BO_USE_LINEAR},
		{DRV_FORMAT_ABGR8888, DRV_BO_USE_SCANOUT | DRV_BO_USE_CURSOR | DRV_BO_USE_RENDERING},
		{DRV_FORMAT_NV12, DRV_BO_USE_SCANOUT | DRV_BO_USE_RENDERING},
		{DRV_FORMAT_NV12, DRV_BO_USE_SCANOUT | DRV_BO_USE_LINEAR},
	}
};

#endif
