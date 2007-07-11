/* 
 * Copyright 2005-2006 Stephane Marchesin
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
#include "nouveau_drv.h"
#include "nouveau_drm.h"


/* returns the number of hw fifos */
int nouveau_fifo_number(drm_device_t* dev)
{
	drm_nouveau_private_t *dev_priv=dev->dev_private;
	switch(dev_priv->card_type)
	{
		case NV_03:
			return 8;
		case NV_04:
		case NV_05:
			return 16;
		case NV_50:
			return 128;
		default:
			return 32;
	}
}

/* returns the size of fifo context */
int nouveau_fifo_ctx_size(drm_device_t* dev)
{
	drm_nouveau_private_t *dev_priv=dev->dev_private;

	if (dev_priv->card_type >= NV_40)
		return 128;
	else if (dev_priv->card_type >= NV_17)
		return 64;
	else
		return 32;
}

/***********************************
 * functions doing the actual work
 ***********************************/

/* voir nv_xaa.c : NVResetGraphics
 * mémoire mappée par nv_driver.c : NVMapMem
 * voir nv_driver.c : NVPreInit 
 */

static int nouveau_fifo_instmem_configure(drm_device_t *dev)
{
	drm_nouveau_private_t *dev_priv = dev->dev_private;

	NV_WRITE(NV03_PFIFO_RAMHT,
			(0x03 << 24) /* search 128 */ | 
			((dev_priv->ramht_bits - 9) << 16) |
			(dev_priv->ramht_offset >> 8)
			);

	NV_WRITE(NV03_PFIFO_RAMRO, dev_priv->ramro_offset>>8);

	switch(dev_priv->card_type)
	{
		case NV_50:
		case NV_40:
			NV_WRITE(NV40_PFIFO_RAMFC, 0x30002);
			if((dev_priv->chipset == 0x49) || (dev_priv->chipset == 0x4b))
				NV_WRITE(0x2230,0x00000001);
			break;
		case NV_44:
			NV_WRITE(NV40_PFIFO_RAMFC, ((nouveau_mem_fb_amount(dev)-512*1024+dev_priv->ramfc_offset)>>16) |
					(2 << 16));
			break;
		case NV_30:
		case NV_20:
		case NV_17:
			NV_WRITE(NV03_PFIFO_RAMFC, (dev_priv->ramfc_offset>>8) |
					(1 << 16) /* 64 Bytes entry*/);
			/* XXX nvidia blob set bit 18, 21,23 for nv20 & nv30 */
			break;
		case NV_10:
		case NV_04:
		case NV_03:
			NV_WRITE(NV03_PFIFO_RAMFC, dev_priv->ramfc_offset>>8);
			break;
	}

	return 0;
}

int nouveau_fifo_init(drm_device_t *dev)
{
	drm_nouveau_private_t *dev_priv = dev->dev_private;
	int ret;

	NV_WRITE(NV03_PMC_ENABLE, NV_READ(NV03_PMC_ENABLE) &
			~NV_PMC_ENABLE_PFIFO);
	NV_WRITE(NV03_PMC_ENABLE, NV_READ(NV03_PMC_ENABLE) |
			 NV_PMC_ENABLE_PFIFO);

	NV_WRITE(NV03_PFIFO_CACHES, 0x00000000);

	ret = nouveau_fifo_instmem_configure(dev);
	if (ret) {
		DRM_ERROR("Failed to configure instance memory\n");
		return ret;
	}

	/* FIXME remove all the stuff that's done in nouveau_fifo_alloc */

	DRM_DEBUG("Setting defaults for remaining PFIFO regs\n");

	/* All channels into PIO mode */
	NV_WRITE(NV04_PFIFO_MODE, 0x00000000);

	NV_WRITE(NV03_PFIFO_CACHE1_PUSH0, 0x00000000);
	NV_WRITE(NV04_PFIFO_CACHE1_PULL0, 0x00000000);
	/* Channel 0 active, PIO mode */
	NV_WRITE(NV03_PFIFO_CACHE1_PUSH1, 0x00000000);
	/* PUT and GET to 0 */
	NV_WRITE(NV04_PFIFO_CACHE1_DMA_PUT, 0x00000000);
	NV_WRITE(NV04_PFIFO_CACHE1_DMA_GET, 0x00000000);
	/* No cmdbuf object */
	NV_WRITE(NV04_PFIFO_CACHE1_DMA_INSTANCE, 0x00000000);
	NV_WRITE(NV03_PFIFO_CACHE0_PUSH0, 0x00000000);
	NV_WRITE(NV03_PFIFO_CACHE0_PULL0, 0x00000000);
	NV_WRITE(NV04_PFIFO_SIZE, 0x0000FFFF);
	NV_WRITE(NV04_PFIFO_CACHE1_HASH, 0x0000FFFF);
	NV_WRITE(NV04_PFIFO_CACHE0_PULL1, 0x00000001);
	NV_WRITE(NV04_PFIFO_CACHE1_DMA_CTL, 0x00000000);
	NV_WRITE(NV04_PFIFO_CACHE1_DMA_STATE, 0x00000000);
	NV_WRITE(NV04_PFIFO_CACHE1_ENGINE, 0x00000000);

	NV_WRITE(NV04_PFIFO_CACHE1_DMA_FETCH, NV_PFIFO_CACHE1_DMA_FETCH_TRIG_112_BYTES |
				      NV_PFIFO_CACHE1_DMA_FETCH_SIZE_128_BYTES |
				      NV_PFIFO_CACHE1_DMA_FETCH_MAX_REQS_4 |
#ifdef __BIG_ENDIAN
				      NV_PFIFO_CACHE1_BIG_ENDIAN |
#endif				      
				      0x00000000);

	NV_WRITE(NV04_PFIFO_CACHE1_DMA_PUSH, 0x00000001);
	NV_WRITE(NV03_PFIFO_CACHE1_PUSH0, 0x00000001);
	NV_WRITE(NV04_PFIFO_CACHE1_PULL0, 0x00000001);
	NV_WRITE(NV04_PFIFO_CACHE1_PULL1, 0x00000001);

	/* FIXME on NV04 */
	if (dev_priv->card_type >= NV_10) {
		NV_WRITE(NV10_PGRAPH_CTX_USER, 0x0);
		NV_WRITE(NV04_PFIFO_DELAY_0, 0xff /* retrycount*/ );
		if (dev_priv->card_type >= NV_40)
			NV_WRITE(NV10_PGRAPH_CTX_CONTROL, 0x00002001);
		else
			NV_WRITE(NV10_PGRAPH_CTX_CONTROL, 0x10110000);
	} else {
		NV_WRITE(NV04_PGRAPH_CTX_USER, 0x0);
		NV_WRITE(NV04_PFIFO_DELAY_0, 0xff /* retrycount*/ );
		NV_WRITE(NV04_PGRAPH_CTX_CONTROL, 0x10110000);
	}

	NV_WRITE(NV04_PFIFO_DMA_TIMESLICE, 0x001fffff);
	NV_WRITE(NV03_PFIFO_CACHES, 0x00000001);
	return 0;
}

static int
nouveau_fifo_cmdbuf_alloc(struct drm_device *dev, int channel)
{
	drm_nouveau_private_t *dev_priv = dev->dev_private;
	struct nouveau_fifo *chan = dev_priv->fifos[channel];
	struct nouveau_config *config = &dev_priv->config;
	struct mem_block *cb;
	int cb_min_size = max(NV03_FIFO_SIZE,PAGE_SIZE);
	nouveau_gpuobj_t *pushbuf = NULL;
	int ret;

	/* Defaults for unconfigured values */
	if (!config->cmdbuf.location)
		config->cmdbuf.location = NOUVEAU_MEM_FB;
	if (!config->cmdbuf.size || config->cmdbuf.size < cb_min_size)
		config->cmdbuf.size = cb_min_size;

	cb = nouveau_mem_alloc(dev, 0, config->cmdbuf.size,
			config->cmdbuf.location | NOUVEAU_MEM_MAPPED,
			(DRMFILE)-2);
	if (!cb) {
		DRM_ERROR("Couldn't allocate DMA command buffer.\n");
		return DRM_ERR(ENOMEM);
	}

	if (cb->flags & NOUVEAU_MEM_AGP) {
		DRM_DEBUG("Creating CB in AGP memory\n");
		ret = nouveau_gpuobj_dma_new(dev, channel,
				NV_CLASS_DMA_IN_MEMORY,
				cb->start - dev_priv->agp_phys,
				cb->size,
				NV_DMA_ACCESS_RO, NV_DMA_TARGET_AGP, &pushbuf);
	} else if ( cb->flags & NOUVEAU_MEM_PCI) {
		DRM_DEBUG("Creating CB in PCI memory\n", cb->start, cb->size);
		ret = nouveau_gpuobj_dma_new(dev, channel,
				NV_CLASS_DMA_IN_MEMORY,
				cb->start,
				cb->size,
				NV_DMA_ACCESS_RO, NV_DMA_TARGET_PCI_NONLINEAR, &pushbuf);
	} else if (dev_priv->card_type != NV_04) {
		ret = nouveau_gpuobj_dma_new
			(dev, channel, NV_CLASS_DMA_IN_MEMORY,
			 cb->start - drm_get_resource_start(dev, 1),
			 cb->size, NV_DMA_ACCESS_RO, NV_DMA_TARGET_VIDMEM,
			 &pushbuf);
	} else {
		/* NV04 cmdbuf hack, from original ddx.. not sure of it's
		 * exact reason for existing :)  PCI access to cmdbuf in
		 * VRAM.
		 */
		ret = nouveau_gpuobj_dma_new
			(dev, channel, NV_CLASS_DMA_IN_MEMORY,
			 cb->start, cb->size, NV_DMA_ACCESS_RO,
			 NV_DMA_TARGET_PCI, &pushbuf);
	}

	if (ret) {
		nouveau_mem_free(dev, cb);
		DRM_ERROR("Error creating push buffer ctxdma: %d\n", ret);
		return ret;
	}

	if ((ret = nouveau_gpuobj_ref_add(dev, channel, 0, pushbuf,
					  &chan->pushbuf))) {
		DRM_ERROR("Error referencing push buffer ctxdma: %d\n", ret);
		return ret;
	}

	dev_priv->fifos[channel]->pushbuf_base = 0;
	dev_priv->fifos[channel]->pushbuf_mem = cb;
	return 0;
}

/* allocates and initializes a fifo for user space consumption */
int nouveau_fifo_alloc(drm_device_t* dev, int *chan_ret, DRMFILE filp,
		       uint32_t vram_handle, uint32_t tt_handle)
{
	int ret;
	drm_nouveau_private_t *dev_priv = dev->dev_private;
	nouveau_engine_func_t *engine = &dev_priv->Engine;
	struct nouveau_fifo *chan;
	int channel;

	/*
	 * Alright, here is the full story
	 * Nvidia cards have multiple hw fifo contexts (praise them for that, 
	 * no complicated crash-prone context switches)
	 * We allocate a new context for each app and let it write to it directly 
	 * (woo, full userspace command submission !)
	 * When there are no more contexts, you lost
	 */
	for(channel=0; channel<nouveau_fifo_number(dev); channel++) {
		if ((dev_priv->card_type == NV_50) && (channel == 0))
			continue;
		if (dev_priv->fifos[channel] == NULL)
			break;
	}
	/* no more fifos. you lost. */
	if (channel==nouveau_fifo_number(dev))
		return DRM_ERR(EINVAL);
	(*chan_ret) = channel;

	dev_priv->fifos[channel] = drm_calloc(1, sizeof(struct nouveau_fifo),
					      DRM_MEM_DRIVER);
	if (!dev_priv->fifos[channel])
		return DRM_ERR(ENOMEM);
	dev_priv->fifo_alloc_count++;
	chan = dev_priv->fifos[channel];
	chan->filp = filp;

	DRM_INFO("Allocating FIFO number %d\n", channel);

	/* Setup channel's default objects */
	ret = nouveau_gpuobj_channel_init(dev, channel, vram_handle, tt_handle);
	if (ret) {
		nouveau_fifo_free(dev, channel);
		return ret;
	}

	/* allocate a command buffer, and create a dma object for the gpu */
	ret = nouveau_fifo_cmdbuf_alloc(dev, channel);
	if (ret) {
		nouveau_fifo_free(dev, channel);
		return ret;
	}

	/* Allocate space for per-channel fixed notifier memory */
	ret = nouveau_notifier_init_channel(dev, channel, filp);
	if (ret) {
		nouveau_fifo_free(dev, channel);
		return ret;
	}

	nouveau_wait_for_idle(dev);

	/* disable the fifo caches */
	NV_WRITE(NV03_PFIFO_CACHES, 0x00000000);
	NV_WRITE(NV04_PFIFO_CACHE1_DMA_PUSH, NV_READ(NV04_PFIFO_CACHE1_DMA_PUSH)&(~0x1));
	NV_WRITE(NV03_PFIFO_CACHE1_PUSH0, 0x00000000);
	NV_WRITE(NV04_PFIFO_CACHE1_PULL0, 0x00000000);

	/* Create a graphics context for new channel */
	ret = engine->graph.create_context(dev, channel);
	if (ret) {
		nouveau_fifo_free(dev, channel);
		return ret;
	}

	/* Construct inital RAMFC for new channel */
	ret = engine->fifo.create_context(dev, channel);
	if (ret) {
		nouveau_fifo_free(dev, channel);
		return ret;
	}

	/* setup channel's default get/put values */
	if (dev_priv->card_type < NV_50) {
		NV_WRITE(NV03_FIFO_REGS_DMAPUT(channel), chan->pushbuf_base);
		NV_WRITE(NV03_FIFO_REGS_DMAGET(channel), chan->pushbuf_base);
	} else {
		NV_WRITE(NV50_FIFO_REGS_DMAPUT(channel), chan->pushbuf_base);
		NV_WRITE(NV50_FIFO_REGS_DMAGET(channel), chan->pushbuf_base);
	}

	/* If this is the first channel, setup PFIFO ourselves.  For any
	 * other case, the GPU will handle this when it switches contexts.
	 */
	if (dev_priv->fifo_alloc_count == 1) {
		ret = engine->fifo.load_context(dev, channel);
		if (ret) {
			nouveau_fifo_free(dev, channel);
			return ret;
		}

		ret = engine->graph.load_context(dev, channel);
		if (ret) {
			nouveau_fifo_free(dev, channel);
			return ret;
		}

		/* Temporary hack, to avoid breaking Xv on cards where the
		 * initial context value for 0x400710 doesn't have these bits
		 * set.  Proper fix would be to find which object+method is
		 * responsible for modifying this state.
		 */
		if (dev_priv->chipset >= 0x10 && dev_priv->chipset < 0x50) {
			uint32_t tmp;
			tmp = NV_READ(NV10_PGRAPH_SURFACE) & 0x0007ff00;
			NV_WRITE(NV10_PGRAPH_SURFACE, tmp);
			tmp = NV_READ(NV10_PGRAPH_SURFACE) | 0x00020100;
			NV_WRITE(NV10_PGRAPH_SURFACE, tmp);
		}
	}

	NV_WRITE(NV04_PFIFO_CACHE1_DMA_PUSH,
		 NV_READ(NV04_PFIFO_CACHE1_DMA_PUSH) | 1);
	NV_WRITE(NV03_PFIFO_CACHE1_PUSH0, 0x00000001);
	NV_WRITE(NV04_PFIFO_CACHE1_PULL0, 0x00000001);
	NV_WRITE(NV04_PFIFO_CACHE1_PULL1, 0x00000001);

	/* reenable the fifo caches */
	NV_WRITE(NV03_PFIFO_CACHES, 1);

	DRM_INFO("%s: initialised FIFO %d\n", __func__, channel);
	return 0;
}

/* stops a fifo */
void nouveau_fifo_free(drm_device_t* dev, int channel)
{
	drm_nouveau_private_t *dev_priv = dev->dev_private;
	nouveau_engine_func_t *engine = &dev_priv->Engine;
	struct nouveau_fifo *chan = dev_priv->fifos[channel];

	if (!chan) {
		DRM_ERROR("Freeing non-existant channel %d\n", channel);
		return;
	}

	DRM_INFO("%s: freeing fifo %d\n", __func__, channel);

	/* disable the fifo caches */
	NV_WRITE(NV03_PFIFO_CACHES, 0x00000000);

	// FIXME XXX needs more code

	engine->fifo.destroy_context(dev, channel);

	/* Cleanup PGRAPH state */
	engine->graph.destroy_context(dev, channel);

	/* reenable the fifo caches */
	NV_WRITE(NV03_PFIFO_CACHES, 0x00000001);

	/* Deallocate push buffer */
	nouveau_gpuobj_ref_del(dev, &chan->pushbuf);
	if (chan->pushbuf_mem) {
		nouveau_mem_free(dev, chan->pushbuf_mem);
		chan->pushbuf_mem = NULL;
	}

	nouveau_notifier_takedown_channel(dev, channel);

	/* Destroy objects belonging to the channel */
	nouveau_gpuobj_channel_takedown(dev, channel);

	dev_priv->fifos[channel] = NULL;
	dev_priv->fifo_alloc_count--;
	drm_free(chan, sizeof(*chan), DRM_MEM_DRIVER);
}

/* cleanups all the fifos from filp */
void nouveau_fifo_cleanup(drm_device_t* dev, DRMFILE filp)
{
	int i;
	drm_nouveau_private_t *dev_priv = dev->dev_private;

	DRM_DEBUG("clearing FIFO enables from filp\n");
	for(i=0;i<nouveau_fifo_number(dev);i++)
		if (dev_priv->fifos[i] && dev_priv->fifos[i]->filp==filp)
			nouveau_fifo_free(dev,i);
}

int
nouveau_fifo_owner(drm_device_t *dev, DRMFILE filp, int channel)
{
	drm_nouveau_private_t *dev_priv = dev->dev_private;

	if (channel >= nouveau_fifo_number(dev))
		return 0;
	if (dev_priv->fifos[channel] == NULL)
		return 0;
	return (dev_priv->fifos[channel]->filp == filp);
}

/***********************************
 * ioctls wrapping the functions
 ***********************************/

static int nouveau_ioctl_fifo_alloc(DRM_IOCTL_ARGS)
{
	DRM_DEVICE;
	drm_nouveau_private_t *dev_priv = dev->dev_private;
	struct nouveau_fifo *chan;
	drm_nouveau_fifo_alloc_t init;
	int res;

	DRM_COPY_FROM_USER_IOCTL(init, (drm_nouveau_fifo_alloc_t __user *) data,
				 sizeof(init));

	if (init.fb_ctxdma_handle == ~0 || init.tt_ctxdma_handle == ~0)
		return DRM_ERR(EINVAL);

	res = nouveau_fifo_alloc(dev, &init.channel, filp,
				 init.fb_ctxdma_handle,
				 init.tt_ctxdma_handle);
	if (res)
		return res;
	chan = dev_priv->fifos[init.channel];

	init.put_base = chan->pushbuf_base;

	/* make the fifo available to user space */
	/* first, the fifo control regs */
	init.ctrl = dev_priv->mmio->offset;
	if (dev_priv->card_type < NV_50) {
		init.ctrl      += NV03_FIFO_REGS(init.channel);
		init.ctrl_size  = NV03_FIFO_REGS_SIZE;
	} else {
		init.ctrl      += NV50_FIFO_REGS(init.channel);
		init.ctrl_size  = NV50_FIFO_REGS_SIZE;
	}
	res = drm_addmap(dev, init.ctrl, init.ctrl_size, _DRM_REGISTERS,
			 0, &chan->regs);
	if (res != 0)
		return res;

	/* pass back FIFO map info to the caller */
	init.cmdbuf      = chan->pushbuf_mem->start;
	init.cmdbuf_size = chan->pushbuf_mem->size;

	/* and the notifier block */
	init.notifier      = chan->notifier_block->start;
	init.notifier_size = chan->notifier_block->size;

	DRM_COPY_TO_USER_IOCTL((drm_nouveau_fifo_alloc_t __user *)data,
			       init, sizeof(init));
	return 0;
}

/***********************************
 * finally, the ioctl table
 ***********************************/

drm_ioctl_desc_t nouveau_ioctls[] = {
	[DRM_IOCTL_NR(DRM_NOUVEAU_FIFO_ALLOC)] = {nouveau_ioctl_fifo_alloc, DRM_AUTH},	
	[DRM_IOCTL_NR(DRM_NOUVEAU_GROBJ_ALLOC)] = {nouveau_ioctl_grobj_alloc, DRM_AUTH},
	[DRM_IOCTL_NR(DRM_NOUVEAU_NOTIFIER_ALLOC)] = {nouveau_ioctl_notifier_alloc, DRM_AUTH},
	[DRM_IOCTL_NR(DRM_NOUVEAU_MEM_ALLOC)] = {nouveau_ioctl_mem_alloc, DRM_AUTH},
	[DRM_IOCTL_NR(DRM_NOUVEAU_MEM_FREE)] = {nouveau_ioctl_mem_free, DRM_AUTH},
	[DRM_IOCTL_NR(DRM_NOUVEAU_GETPARAM)] = {nouveau_ioctl_getparam, DRM_AUTH},
	[DRM_IOCTL_NR(DRM_NOUVEAU_SETPARAM)] = {nouveau_ioctl_setparam, DRM_AUTH|DRM_MASTER|DRM_ROOT_ONLY},	
};

int nouveau_max_ioctl = DRM_ARRAY_SIZE(nouveau_ioctls);
