/*
 * Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "drv_priv.h"
#include "helpers.h"
#include "util.h"

static struct supported_combination combos[2] = {
	{DRM_FORMAT_ARGB8888, DRM_FORMAT_MOD_NONE,
		BO_USE_CURSOR | BO_USE_LINEAR | BO_USE_RENDERING | BO_USE_SW_READ_OFTEN |
		BO_USE_SW_WRITE_OFTEN | BO_USE_SW_READ_RARELY | BO_USE_SW_WRITE_RARELY},
	{DRM_FORMAT_XRGB8888, DRM_FORMAT_MOD_NONE,
		BO_USE_CURSOR | BO_USE_LINEAR | BO_USE_RENDERING | BO_USE_SW_READ_OFTEN |
		BO_USE_SW_WRITE_OFTEN | BO_USE_SW_READ_RARELY | BO_USE_SW_WRITE_RARELY},
};

static int virtio_gpu_init(struct driver *drv)
{
	drv_insert_combinations(drv, combos, ARRAY_SIZE(combos));
	return drv_add_kms_flags(drv);
}

struct backend backend_virtio_gpu =
{
	.name = "virtio_gpu",
	.init = virtio_gpu_init,
	.bo_create = drv_dumb_bo_create,
	.bo_destroy = drv_dumb_bo_destroy,
	.bo_map = drv_dumb_bo_map,
};

