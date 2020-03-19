/*
 * Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#ifdef DRV_SYNAPTICS

#include "drv_priv.h"
#include "helpers.h"
#include "util.h"

static const uint32_t render_target_formats[] = { DRM_FORMAT_ARGB8888, DRM_FORMAT_ABGR8888,
                                                  DRM_FORMAT_XRGB8888 };

static int synaptics_init(struct driver *drv)
{
	drv_add_combinations(drv, render_target_formats, ARRAY_SIZE(render_target_formats),
			     &LINEAR_METADATA, BO_USE_RENDER_MASK | BO_USE_SCANOUT);

	return drv_modify_linear_combinations(drv);
}

const struct backend backend_synaptics = {
	.name = "synaptics",
	.init = synaptics_init,
	.bo_create = drv_dumb_bo_create,
	.bo_destroy = drv_dumb_bo_destroy,
	.bo_import = drv_prime_bo_import,
	.bo_map = drv_dumb_bo_map,
	.bo_unmap = drv_bo_munmap,
};

#endif
