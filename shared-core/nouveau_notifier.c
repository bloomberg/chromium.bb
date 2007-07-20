/*
 * Copyright (C) 2007 Ben Skeggs.
 *
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

int
nouveau_notifier_init_channel(struct drm_device *dev, int channel, DRMFILE filp)
{
	struct drm_nouveau_private *dev_priv = dev->dev_private;
	struct nouveau_fifo *chan = dev_priv->fifos[channel];
	int flags, ret;

	/*TODO: PCI notifier blocks */
	if (dev_priv->agp_heap &&
	    dev_priv->gart_info.type != NOUVEAU_GART_SGDMA)
		flags = NOUVEAU_MEM_AGP | NOUVEAU_MEM_FB_ACCEPTABLE;
	else
		flags = NOUVEAU_MEM_FB;
	flags |= NOUVEAU_MEM_MAPPED;

	chan->notifier_block = nouveau_mem_alloc(dev, 0, PAGE_SIZE, flags,filp);
	if (!chan->notifier_block)
		return -ENOMEM;

	ret = nouveau_mem_init_heap(&chan->notifier_heap,
				    0, chan->notifier_block->size);
	if (ret)
		return ret;

	return 0;
}

void
nouveau_notifier_takedown_channel(struct drm_device *dev, int channel)
{
	struct drm_nouveau_private *dev_priv = dev->dev_private;
	struct nouveau_fifo *chan = dev_priv->fifos[channel];

	if (chan->notifier_block) {
		nouveau_mem_free(dev, chan->notifier_block);
		chan->notifier_block = NULL;
	}

	/*XXX: heap destroy */
}

int
nouveau_notifier_alloc(struct drm_device *dev, int channel, uint32_t handle,
		       int count, uint32_t *b_offset)
{
	struct drm_nouveau_private *dev_priv = dev->dev_private;
	struct nouveau_fifo *chan = dev_priv->fifos[channel];
	struct nouveau_gpuobj *nobj = NULL;
	struct mem_block *mem;
	uint32_t offset;
	int target, ret;

	if (!chan->notifier_heap) {
		DRM_ERROR("Channel %d doesn't have a notifier heap!\n",
			  channel);
		return -EINVAL;
	}

	mem = nouveau_mem_alloc_block(chan->notifier_heap, 32, 0, chan->filp);
	if (!mem) {
		DRM_ERROR("Channel %d notifier block full\n", channel);
		return -ENOMEM;
	}
	mem->flags = NOUVEAU_MEM_NOTIFIER;

	offset = chan->notifier_block->start + mem->start;
	if (chan->notifier_block->flags & NOUVEAU_MEM_FB) {
		target = NV_DMA_TARGET_VIDMEM;
	} else if (chan->notifier_block->flags & NOUVEAU_MEM_AGP) {
		target = NV_DMA_TARGET_AGP;
	} else {
		DRM_ERROR("Bad DMA target, flags 0x%08x!\n",
			  chan->notifier_block->flags);
		return -EINVAL;
	}

	if ((ret = nouveau_gpuobj_dma_new(dev, channel, NV_CLASS_DMA_IN_MEMORY,
					  offset, mem->size,
					  NV_DMA_ACCESS_RW, target, &nobj))) {
		nouveau_mem_free_block(mem);
		DRM_ERROR("Error creating notifier ctxdma: %d\n", ret);
		return ret;
	}

	if ((ret = nouveau_gpuobj_ref_add(dev, channel, handle, nobj, NULL))) {
		nouveau_gpuobj_del(dev, &nobj);
		nouveau_mem_free_block(mem);
		DRM_ERROR("Error referencing notifier ctxdma: %d\n", ret);
		return ret;
	}

	*b_offset = mem->start;
	return 0;
}

int
nouveau_ioctl_notifier_alloc(DRM_IOCTL_ARGS)
{
	DRM_DEVICE;
	struct drm_nouveau_notifier_alloc na;
	int ret;

	DRM_COPY_FROM_USER_IOCTL(na,
			(struct drm_nouveau_notifier_alloc __user*)data,
			sizeof(na));

	if (!nouveau_fifo_owner(dev, filp, na.channel)) {
		DRM_ERROR("pid %d doesn't own channel %d\n",
			  DRM_CURRENTPID, na.channel);
		return -EPERM;
	}

	ret = nouveau_notifier_alloc(dev, na.channel, na.handle,
				     na.count, &na.offset);
	if (ret)
		return ret;

	DRM_COPY_TO_USER_IOCTL((struct drm_nouveau_notifier_alloc __user*)data,
			       na, sizeof(na));
	return 0;
}

