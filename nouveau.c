/*
 * Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "drv_priv.h"
#include "helpers.h"
#include "util.h"

static struct supported_combination combos[2] = {
	{DRM_FORMAT_XRGB8888, DRM_FORMAT_MOD_NONE, BO_USE_CURSOR | BO_USE_RENDERING},
	{DRM_FORMAT_ARGB8888, DRM_FORMAT_MOD_NONE, BO_USE_CURSOR | BO_USE_RENDERING},
};

static int nouveau_init(struct driver *drv)
{
	drv_insert_combinations(drv, combos, ARRAY_SIZE(combos));
	return drv_add_kms_flags(drv);
}

struct backend backend_nouveau =
{
	.name = "nouveau",
	.init = nouveau_init,
	.bo_create = drv_dumb_bo_create,
	.bo_destroy = drv_dumb_bo_destroy,
	.bo_import = drv_prime_bo_import,
	.bo_map = drv_dumb_bo_map,
};
