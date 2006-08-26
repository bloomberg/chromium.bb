/* 
 * Copyright 2005 Stephane Marchesin
 * All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * PRECISION INSIGHT AND/OR ITS SUPPLIERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#include "drmP.h"
#include "drm.h"
#include "drm_sarea.h"
#include "nouveau_drv.h"

/* here a client dies, release the stuff that was allocated for its filp */
void nouveau_preclose(drm_device_t * dev, DRMFILE filp)
{
	drm_nouveau_private_t *dev_priv = dev->dev_private;

	nouveau_mem_release(filp,dev_priv->fb_heap);
	nouveau_mem_release(filp,dev_priv->agp_heap);
	nouveau_object_cleanup(dev, filp);
	nouveau_fifo_cleanup(dev, filp);
}

/* first module load, setup the mmio/fb mapping */
int nouveau_firstopen(struct drm_device *dev)
{
	int ret;
	drm_nouveau_private_t *dev_priv = dev->dev_private;

	/* resource 0 is mmio regs */
	/* resource 1 is linear FB */
	/* resource 2 is ??? (mmio regs + 0x1000000) */
	/* resource 6 is bios */

	/* map the mmio regs */
	ret = drm_addmap(dev, drm_get_resource_start(dev, 0), drm_get_resource_len(dev, 0), 
			_DRM_REGISTERS, _DRM_READ_ONLY, &dev_priv->mmio);
	if (dev_priv->mmio)
	{
		DRM_INFO("regs mapped ok at 0x%lx\n",dev_priv->mmio->offset);
	}
	else
	{
		DRM_ERROR("Unable to initialize the mmio mapping. Please report your setup to " DRIVER_EMAIL "\n");
		return 1; 
	}

	DRM_INFO("%lld MB of video ram detected\n",nouveau_mem_fb_amount(dev)>>20);

	if (dev_priv->card_type>=NV_40)
		dev_priv->fb_usable_size=nouveau_mem_fb_amount(dev)-560*1024;
	else
		dev_priv->fb_usable_size=nouveau_mem_fb_amount(dev)-256*1024;

	nouveau_hash_table_init(dev);

	if (dev_priv->card_type >= NV_40)
		dev_priv->fb_obj = nouveau_dma_object_create(dev,
				0, nouveau_mem_fb_amount(dev),
				NV_DMA_ACCESS_RW, NV_DMA_TARGET_VIDMEM);

	/* allocate one buffer for all the fifos */
	dev_priv->cmdbuf_alloc = nouveau_mem_alloc(dev, 0, 1024*1024, NOUVEAU_MEM_FB, (DRMFILE)-2);

	if (dev_priv->cmdbuf_alloc->flags&NOUVEAU_MEM_AGP) {
		dev_priv->cmdbuf_location = NV_DMA_TARGET_AGP;
		dev_priv->cmdbuf_ch_size  = NV03_FIFO_SIZE;
		dev_priv->cmdbuf_base     = dev_priv->cmdbuf_alloc->start;
		dev_priv->cmdbuf_obj = nouveau_dma_object_create(dev,
				dev_priv->cmdbuf_base, nouveau_fifo_number(dev)*NV03_FIFO_SIZE,
				NV_DMA_ACCESS_RO, dev_priv->cmdbuf_location);
	} else { /* NOUVEAU_MEM_FB */
		dev_priv->cmdbuf_location = NV_DMA_TARGET_VIDMEM;
		dev_priv->cmdbuf_ch_size  = NV03_FIFO_SIZE;
		dev_priv->cmdbuf_base     = dev_priv->cmdbuf_alloc->start;
		dev_priv->cmdbuf_obj = nouveau_dma_object_create(dev,
				dev_priv->cmdbuf_base - drm_get_resource_start(dev, 1),
				nouveau_fifo_number(dev)*NV03_FIFO_SIZE,
				NV_DMA_ACCESS_RO, dev_priv->cmdbuf_location);
	}

	DRM_INFO("DMA command buffer is %dKiB at 0x%08x(%s)\n",
			(nouveau_fifo_number(dev)*dev_priv->cmdbuf_ch_size)/1024,
			dev_priv->cmdbuf_base,
			dev_priv->cmdbuf_location == NV_DMA_TARGET_AGP ? "AGP" : "VRAM"
			);

	return 0;
}

int nouveau_load(struct drm_device *dev, unsigned long flags)
{
	drm_nouveau_private_t *dev_priv;

	if (flags==NV_UNKNOWN)
		return DRM_ERR(EINVAL);

	dev_priv = drm_alloc(sizeof(drm_nouveau_private_t), DRM_MEM_DRIVER);
	if (!dev_priv)                   
		return DRM_ERR(ENOMEM);

	memset(dev_priv, 0, sizeof(drm_nouveau_private_t));
	dev_priv->card_type=flags&NOUVEAU_FAMILY;
	dev_priv->flags=flags&NOUVEAU_FLAGS;

	dev->dev_private = (void *)dev_priv;

	return 0;
}

int nouveau_unload(struct drm_device *dev)
{
	drm_free(dev->dev_private, sizeof(*dev->dev_private), DRM_MEM_DRIVER);
	dev->dev_private = NULL;
	return 0;
}

