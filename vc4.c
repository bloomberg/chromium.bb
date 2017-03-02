/*
 * Copyright 2017 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifdef DRV_VC4

#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <vc4_drm.h>
#include <xf86drm.h>

#include "drv_priv.h"
#include "helpers.h"
#include "util.h"

static const uint32_t supported_formats[] = {
	DRM_FORMAT_ARGB8888, DRM_FORMAT_RGB565, DRM_FORMAT_XRGB8888
};

static int vc4_init(struct driver *drv)
{
	return drv_add_linear_combinations(drv, supported_formats,
					   ARRAY_SIZE(supported_formats));
}

static int vc4_bo_create(struct bo *bo, uint32_t width, uint32_t height,
			 uint32_t format, uint32_t flags)
{
	int ret;
	size_t plane;
	struct drm_vc4_create_bo bo_create;

	drv_bo_from_format(bo, width, height, format);

	memset(&bo_create, 0, sizeof(bo_create));
	bo_create.size = bo->total_size;

	ret = drmIoctl(bo->drv->fd, DRM_IOCTL_VC4_CREATE_BO, &bo_create);
	if (ret) {
		fprintf(stderr, "drv: DRM_IOCTL_VC4_GEM_CREATE failed "
				"(size=%zu)\n", bo->total_size);
		return ret;
	}

	for (plane = 0; plane < bo->num_planes; plane++)
		bo->handles[plane].u32 = bo_create.handle;

	return 0;
}

static void *vc4_bo_map(struct bo *bo, struct map_info *data, size_t plane)
{
	int ret;
	struct drm_vc4_mmap_bo bo_map;

	memset(&bo_map, 0, sizeof(bo_map));
	bo_map.handle = bo->handles[0].u32;

	ret = drmCommandWriteRead(bo->drv->fd, DRM_VC4_MMAP_BO, &bo_map,
				  sizeof(bo_map));
	if (ret) {
		fprintf(stderr, "drv: DRM_VC4_MMAP_BO failed\n");
		return MAP_FAILED;
	}

	data->length = bo->total_size;

	return mmap(0, bo->total_size, PROT_READ | PROT_WRITE, MAP_SHARED,
		    bo->drv->fd, bo_map.offset);
}

struct backend backend_vc4 =
{
	.name = "vc4",
	.init = vc4_init,
	.bo_create = vc4_bo_create,
	.bo_import = drv_prime_bo_import,
	.bo_destroy = drv_gem_bo_destroy,
	.bo_map = vc4_bo_map,
};

#endif
