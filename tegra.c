/*
 * Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifdef GBM_TEGRA

#include <string.h>
#include <xf86drm.h>
#include <tegra_drm.h>

#include "gbm_priv.h"
#include "helpers.h"

int gbm_tegra_bo_create(struct gbm_bo *bo, uint32_t width, uint32_t height, uint32_t format, uint32_t flags)
{
	size_t size = width * height * gbm_bytes_from_format(format);
	struct drm_tegra_gem_create gem_create;
	int ret;

	memset(&gem_create, 0, sizeof(gem_create));
	gem_create.size = size;
	gem_create.flags = 0;

	ret = drmIoctl(bo->gbm->fd, DRM_IOCTL_TEGRA_GEM_CREATE, &gem_create);
	if (ret)
		return ret;

	bo->handle.u32 = gem_create.handle;
	bo->size = size;
	bo->stride = width * gbm_bytes_from_format(format);

	return 0;
}

struct gbm_driver gbm_driver_tegra =
{
	.name = "tegra",
	.bo_create = gbm_tegra_bo_create,
	.bo_destroy = gbm_gem_bo_destroy,
	.format_list = {
		{GBM_FORMAT_XRGB8888, GBM_BO_USE_SCANOUT | GBM_BO_USE_CURSOR | GBM_BO_USE_RENDERING | GBM_BO_USE_WRITE},
		{GBM_FORMAT_ARGB8888, GBM_BO_USE_SCANOUT | GBM_BO_USE_CURSOR | GBM_BO_USE_RENDERING | GBM_BO_USE_WRITE},
	}
};

#endif
