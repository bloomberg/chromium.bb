/*
 * Copyright (C) 2007 Ben Skeggs.
 * All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial
 * portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE COPYRIGHT OWNER(S) AND/OR ITS SUPPLIERS BE
 * LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
 * OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 */

#include "drmP.h"
#include "drm.h"
#include "nouveau_drv.h"

static void
nv50_fifo_init_reset(drm_device_t *dev)
{
	drm_nouveau_private_t *dev_priv = dev->dev_private;
	uint32_t pmc_e;

	pmc_e = NV_READ(NV03_PMC_ENABLE);
	NV_WRITE(NV03_PMC_ENABLE, pmc_e & ~NV_PMC_ENABLE_PFIFO);
	pmc_e = NV_READ(NV03_PMC_ENABLE);
	NV_WRITE(NV03_PMC_ENABLE, pmc_e |  NV_PMC_ENABLE_PFIFO);
}

int
nv50_fifo_init(drm_device_t *dev)
{
	nv50_fifo_init_reset(dev);

	DRM_ERROR("stub!\n");
	return 0;
}

void
nv50_fifo_takedown(drm_device_t *dev)
{
	DRM_ERROR("stub!\n");
}

int
nv50_fifo_create_context(drm_device_t *dev, int channel)
{
	DRM_ERROR("stub!\n");
	return 0;
}

void
nv50_fifo_destroy_context(drm_device_t *dev, int channel)
{
}

int
nv50_fifo_load_context(drm_device_t *dev, int channel)
{
	DRM_ERROR("stub!\n");
	return 0;
}

int
nv50_fifo_save_context(drm_device_t *dev, int channel)
{
	DRM_ERROR("stub!\n");
	return 0;
}

