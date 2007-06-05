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
	struct nouveau_config *config = &dev_priv->config;
	struct mem_block *cb;
	struct nouveau_object *cb_dma = NULL;
	int cb_min_size = max(NV03_FIFO_SIZE,PAGE_SIZE);

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
		cb_dma = nouveau_object_dma_create(dev, channel,
				NV_CLASS_DMA_IN_MEMORY,
				cb->start - dev_priv->agp_phys,
				cb->size,
				NV_DMA_ACCESS_RO, NV_DMA_TARGET_AGP);
	} else if (dev_priv->card_type != NV_04) {
		cb_dma = nouveau_object_dma_create(dev, channel,
				NV_CLASS_DMA_IN_MEMORY,
				cb->start - drm_get_resource_start(dev, 1),
				cb->size,
				NV_DMA_ACCESS_RO, NV_DMA_TARGET_VIDMEM);
	} else {
		/* NV04 cmdbuf hack, from original ddx.. not sure of it's
		 * exact reason for existing :)  PCI access to cmdbuf in
		 * VRAM.
		 */
		cb_dma = nouveau_object_dma_create(dev, channel,
				NV_CLASS_DMA_IN_MEMORY,
				cb->start, cb->size,
				NV_DMA_ACCESS_RO, NV_DMA_TARGET_PCI);
	}

	if (!cb_dma) {
		nouveau_mem_free(dev, cb);
		DRM_ERROR("Failed to alloc DMA object for command buffer\n");
		return DRM_ERR(ENOMEM);
	}

	dev_priv->fifos[channel].cmdbuf_mem = cb;
	dev_priv->fifos[channel].cmdbuf_obj = cb_dma;
	return 0;
}

#define RAMFC_WR(offset, val) NV_WRITE(fifoctx + NV04_RAMFC_##offset, (val))
static void nouveau_nv04_context_init(drm_device_t *dev, int channel)
{
        drm_nouveau_private_t *dev_priv = dev->dev_private;
	struct nouveau_object *cb_obj;
	uint32_t fifoctx, ctx_size = 32;
	int i;

	cb_obj = dev_priv->fifos[channel].cmdbuf_obj;

	fifoctx=NV_RAMIN+dev_priv->ramfc_offset+channel*ctx_size;
	
        // clear the fifo context
        for(i=0;i<ctx_size/4;i++)
                NV_WRITE(fifoctx+4*i,0x0);

	RAMFC_WR(DMA_INSTANCE	, nouveau_chip_instance_get(dev, cb_obj->instance));

        RAMFC_WR(DMA_FETCH,	NV_PFIFO_CACHE1_DMA_FETCH_TRIG_112_BYTES |
                                NV_PFIFO_CACHE1_DMA_FETCH_SIZE_128_BYTES |
                                NV_PFIFO_CACHE1_DMA_FETCH_MAX_REQS_4     |
#ifdef __BIG_ENDIAN
                                NV_PFIFO_CACHE1_BIG_ENDIAN |
#endif
				0x00000000);
}
#undef RAMFC_WR

#define RAMFC_WR(offset, val) NV_WRITE(fifoctx + NV10_RAMFC_##offset, (val))
static void nouveau_nv10_context_init(drm_device_t *dev, int channel)
{
        drm_nouveau_private_t *dev_priv = dev->dev_private;
        struct nouveau_object *cb_obj;
        uint32_t fifoctx;
        int ctx_size = nouveau_fifo_ctx_size(dev);
        int i;
        cb_obj  = dev_priv->fifos[channel].cmdbuf_obj;
        fifoctx = NV_RAMIN + dev_priv->ramfc_offset + channel*ctx_size;

        for (i=0;i<ctx_size;i+=4)
                NV_WRITE(fifoctx + i, 0);

        /* Fill entries that are seen filled in dumps of nvidia driver just
         * after channel's is put into DMA mode
         */

        RAMFC_WR(DMA_INSTANCE  , nouveau_chip_instance_get(dev,
                                cb_obj->instance));

        RAMFC_WR(DMA_FETCH, NV_PFIFO_CACHE1_DMA_FETCH_TRIG_112_BYTES | 
                        NV_PFIFO_CACHE1_DMA_FETCH_SIZE_128_BYTES |
                        NV_PFIFO_CACHE1_DMA_FETCH_MAX_REQS_4     |
#ifdef __BIG_ENDIAN
                        NV_PFIFO_CACHE1_BIG_ENDIAN |
#endif
			0x00000000);
}

static void nouveau_nv30_context_init(drm_device_t *dev, int channel)
{
        drm_nouveau_private_t *dev_priv = dev->dev_private;
        struct nouveau_fifo *chan = &dev_priv->fifos[channel];
	struct nouveau_object *cb_obj;
	uint32_t fifoctx, grctx_inst, cb_inst, ctx_size = 64;
	int i;

	cb_obj = dev_priv->fifos[channel].cmdbuf_obj;
        cb_inst = nouveau_chip_instance_get(dev, chan->cmdbuf_obj->instance);
        grctx_inst = nouveau_chip_instance_get(dev, chan->ramin_grctx);
        fifoctx = NV_RAMIN + dev_priv->ramfc_offset + channel * ctx_size;
        
        for (i = 0; i < ctx_size; i += 4)
                NV_WRITE(fifoctx + i, 0);

        RAMFC_WR(REF_CNT,       NV_READ(NV10_PFIFO_CACHE1_REF_CNT));
        RAMFC_WR(DMA_INSTANCE,  cb_inst);
        RAMFC_WR(DMA_STATE,     NV_READ(NV04_PFIFO_CACHE1_DMA_STATE));
        RAMFC_WR(DMA_FETCH,     NV_PFIFO_CACHE1_DMA_FETCH_TRIG_128_BYTES | 
                                NV_PFIFO_CACHE1_DMA_FETCH_SIZE_128_BYTES |
                                NV_PFIFO_CACHE1_DMA_FETCH_MAX_REQS_8 |
#ifdef __BIG_ENDIAN
                                NV_PFIFO_CACHE1_BIG_ENDIAN |
#endif
                                0x00000000);
        
        RAMFC_WR(ENGINE,                NV_READ(NV04_PFIFO_CACHE1_ENGINE));
        RAMFC_WR(PULL1_ENGINE,          NV_READ(NV04_PFIFO_CACHE1_PULL1)); 
        RAMFC_WR(ACQUIRE_VALUE,         NV_READ(NV10_PFIFO_CACHE1_ACQUIRE_VALUE));
        RAMFC_WR(ACQUIRE_TIMESTAMP,     NV_READ(NV10_PFIFO_CACHE1_ACQUIRE_TIMESTAMP));
        RAMFC_WR(ACQUIRE_TIMEOUT,       NV_READ(NV10_PFIFO_CACHE1_ACQUIRE_TIMEOUT));
        RAMFC_WR(SEMAPHORE,             NV_READ(NV10_PFIFO_CACHE1_SEMAPHORE));
}

#if 0
static void nouveau_nv10_context_save(drm_device_t *dev)
{
	drm_nouveau_private_t *dev_priv = dev->dev_private;
	uint32_t fifoctx;
	int channel;

	channel = NV_READ(NV03_PFIFO_CACHE1_PUSH1) & (nouveau_fifo_number(dev)-1);
	fifoctx = NV_RAMIN + dev_priv->ramfc_offset + channel*64;

	RAMFC_WR(DMA_PUT          , NV_READ(NV04_PFIFO_CACHE1_DMA_PUT));
	RAMFC_WR(DMA_GET          , NV_READ(NV04_PFIFO_CACHE1_DMA_GET));
	RAMFC_WR(REF_CNT          , NV_READ(NV10_PFIFO_CACHE1_REF_CNT));
	RAMFC_WR(DMA_INSTANCE     , NV_READ(NV04_PFIFO_CACHE1_DMA_INSTANCE));
	RAMFC_WR(DMA_STATE        , NV_READ(NV04_PFIFO_CACHE1_DMA_STATE));
	RAMFC_WR(DMA_FETCH        , NV_READ(NV04_PFIFO_CACHE1_DMA_FETCH));
	RAMFC_WR(ENGINE           , NV_READ(NV04_PFIFO_CACHE1_ENGINE));
	RAMFC_WR(PULL1_ENGINE     , NV_READ(NV04_PFIFO_CACHE1_PULL1));
	RAMFC_WR(ACQUIRE_VALUE    , NV_READ(NV10_PFIFO_CACHE1_ACQUIRE_VALUE));
	RAMFC_WR(ACQUIRE_TIMESTAMP, NV_READ(NV10_PFIFO_CACHE1_ACQUIRE_TIMESTAMP));
	RAMFC_WR(ACQUIRE_TIMEOUT  , NV_READ(NV10_PFIFO_CACHE1_ACQUIRE_TIMEOUT));
	RAMFC_WR(SEMAPHORE        , NV_READ(NV10_PFIFO_CACHE1_SEMAPHORE));
	RAMFC_WR(DMA_SUBROUTINE   , NV_READ(NV10_PFIFO_CACHE1_DMA_SUBROUTINE));
}
#endif
#undef RAMFC_WR

#define RAMFC_WR(offset, val) NV_WRITE(fifoctx + NV40_RAMFC_##offset, (val))
static void nouveau_nv40_context_init(drm_device_t *dev, int channel)
{
	drm_nouveau_private_t *dev_priv = dev->dev_private;
	struct nouveau_fifo *chan = &dev_priv->fifos[channel];
	uint32_t fifoctx, cb_inst, grctx_inst;
	int i;

	cb_inst = nouveau_chip_instance_get(dev, chan->cmdbuf_obj->instance);
	grctx_inst = nouveau_chip_instance_get(dev, chan->ramin_grctx);
	fifoctx = NV_RAMIN + dev_priv->ramfc_offset + channel*128;
	for (i=0;i<128;i+=4)
		NV_WRITE(fifoctx + i, 0);

	/* Fill entries that are seen filled in dumps of nvidia driver just
	 * after channel's is put into DMA mode
	 */
	RAMFC_WR(DMA_INSTANCE  , cb_inst);
	RAMFC_WR(DMA_FETCH     , NV_PFIFO_CACHE1_DMA_FETCH_TRIG_128_BYTES |
				 NV_PFIFO_CACHE1_DMA_FETCH_SIZE_128_BYTES |
				 NV_PFIFO_CACHE1_DMA_FETCH_MAX_REQS_8 |
#ifdef __BIG_ENDIAN
				 NV_PFIFO_CACHE1_BIG_ENDIAN |
#endif
				 0x30000000 /* no idea.. */);
	RAMFC_WR(GRCTX_INSTANCE, grctx_inst);
	RAMFC_WR(DMA_TIMESLICE , 0x0001FFFF);
}

static void nouveau_nv40_context_save(drm_device_t *dev)
{
	drm_nouveau_private_t *dev_priv = dev->dev_private;
	uint32_t fifoctx;
	int channel;

	channel = NV_READ(NV03_PFIFO_CACHE1_PUSH1) & (nouveau_fifo_number(dev)-1);
	fifoctx = NV_RAMIN + dev_priv->ramfc_offset + channel*128;

	RAMFC_WR(DMA_PUT          , NV_READ(NV04_PFIFO_CACHE1_DMA_PUT));
	RAMFC_WR(DMA_GET          , NV_READ(NV04_PFIFO_CACHE1_DMA_GET));
	RAMFC_WR(REF_CNT          , NV_READ(NV10_PFIFO_CACHE1_REF_CNT));
	RAMFC_WR(DMA_INSTANCE     , NV_READ(NV04_PFIFO_CACHE1_DMA_INSTANCE));
	RAMFC_WR(DMA_DCOUNT       , NV_READ(NV10_PFIFO_CACHE1_DMA_DCOUNT));
	RAMFC_WR(DMA_STATE        , NV_READ(NV04_PFIFO_CACHE1_DMA_STATE));
	RAMFC_WR(DMA_FETCH	  , NV_READ(NV04_PFIFO_CACHE1_DMA_FETCH));
	RAMFC_WR(ENGINE           , NV_READ(NV04_PFIFO_CACHE1_ENGINE));
	RAMFC_WR(PULL1_ENGINE     , NV_READ(NV04_PFIFO_CACHE1_PULL1));
	RAMFC_WR(ACQUIRE_VALUE    , NV_READ(NV10_PFIFO_CACHE1_ACQUIRE_VALUE));
	RAMFC_WR(ACQUIRE_TIMESTAMP, NV_READ(NV10_PFIFO_CACHE1_ACQUIRE_TIMESTAMP));
	RAMFC_WR(ACQUIRE_TIMEOUT  , NV_READ(NV10_PFIFO_CACHE1_ACQUIRE_TIMEOUT));
	RAMFC_WR(SEMAPHORE        , NV_READ(NV10_PFIFO_CACHE1_SEMAPHORE));
	RAMFC_WR(DMA_SUBROUTINE   , NV_READ(NV04_PFIFO_CACHE1_DMA_GET));
	RAMFC_WR(GRCTX_INSTANCE   , NV_READ(NV40_PFIFO_GRCTX_INSTANCE));
	RAMFC_WR(DMA_TIMESLICE    , NV_READ(NV04_PFIFO_DMA_TIMESLICE) & 0x1FFFF);
	RAMFC_WR(UNK_40           , NV_READ(NV40_PFIFO_UNK32E4));
}
#undef RAMFC_WR

/* This function should load values from RAMFC into PFIFO, but for now
 * it just clobbers PFIFO with what nouveau_fifo_alloc used to setup
 * unconditionally.
 */
static void
nouveau_fifo_context_restore(drm_device_t *dev, int channel)
{
	drm_nouveau_private_t *dev_priv = dev->dev_private;
	struct nouveau_fifo *chan = &dev_priv->fifos[channel];
	uint32_t cb_inst;

	cb_inst = nouveau_chip_instance_get(dev, chan->cmdbuf_obj->instance);

	// FIXME check if we need to refill the time quota with something like NV_WRITE(0x204C, 0x0003FFFF);

	if (dev_priv->card_type >= NV_40)
		NV_WRITE(NV03_PFIFO_CACHE1_PUSH1, 0x00010000|channel);
	else
		NV_WRITE(NV03_PFIFO_CACHE1_PUSH1, 0x00000100|channel);

	NV_WRITE(NV04_PFIFO_CACHE1_DMA_PUT, 0 /*RAMFC_DMA_PUT*/);
	NV_WRITE(NV04_PFIFO_CACHE1_DMA_GET, 0 /*RAMFC_DMA_GET*/);
	NV_WRITE(NV04_PFIFO_CACHE1_DMA_INSTANCE, cb_inst);
	NV_WRITE(NV04_PFIFO_SIZE , 0x0000FFFF);
	NV_WRITE(NV04_PFIFO_CACHE1_HASH, 0x0000FFFF);

	NV_WRITE(NV04_PFIFO_CACHE0_PULL1, 0x00000001);
	NV_WRITE(NV04_PFIFO_CACHE1_DMA_CTL, 0x00000000);
	NV_WRITE(NV04_PFIFO_CACHE1_DMA_STATE, 0x00000000);
	NV_WRITE(NV04_PFIFO_CACHE1_ENGINE, 0x00000000);

	NV_WRITE(NV04_PFIFO_CACHE1_DMA_FETCH,	NV_PFIFO_CACHE1_DMA_FETCH_TRIG_112_BYTES |
					NV_PFIFO_CACHE1_DMA_FETCH_SIZE_128_BYTES |
					NV_PFIFO_CACHE1_DMA_FETCH_MAX_REQS_4 |
#ifdef __BIG_ENDIAN
					NV_PFIFO_CACHE1_BIG_ENDIAN |
#endif
					0x00000000);
}

/* allocates and initializes a fifo for user space consumption */
static int nouveau_fifo_alloc(drm_device_t* dev, int *chan_ret, DRMFILE filp)
{
	int ret;
	drm_nouveau_private_t *dev_priv = dev->dev_private;
	struct nouveau_object *cb_obj;
	int channel;

	/*
	 * Alright, here is the full story
	 * Nvidia cards have multiple hw fifo contexts (praise them for that, 
	 * no complicated crash-prone context switches)
	 * We allocate a new context for each app and let it write to it directly 
	 * (woo, full userspace command submission !)
	 * When there are no more contexts, you lost
	 */
	for(channel=0; channel<nouveau_fifo_number(dev); channel++)
		if (dev_priv->fifos[channel].used==0)
			break;
	/* no more fifos. you lost. */
	if (channel==nouveau_fifo_number(dev))
		return DRM_ERR(EINVAL);
	(*chan_ret) = channel;

	DRM_INFO("Allocating FIFO number %d\n", channel);

	/* that fifo is used */
	dev_priv->fifos[channel].used = 1;
	dev_priv->fifos[channel].filp = filp;
	/* FIFO has no objects yet */
	dev_priv->fifos[channel].objs = NULL;

	/* allocate a command buffer, and create a dma object for the gpu */
	ret = nouveau_fifo_cmdbuf_alloc(dev, channel);
	if (ret) {
		nouveau_fifo_free(dev, channel);
		return ret;
	}
	cb_obj = dev_priv->fifos[channel].cmdbuf_obj;

	nouveau_wait_for_idle(dev);

	/* disable the fifo caches */
	NV_WRITE(NV03_PFIFO_CACHES, 0x00000000);
	NV_WRITE(NV04_PFIFO_CACHE1_DMA_PUSH, NV_READ(NV04_PFIFO_CACHE1_DMA_PUSH)&(~0x1));
	NV_WRITE(NV03_PFIFO_CACHE1_PUSH0, 0x00000000);
	NV_WRITE(NV04_PFIFO_CACHE1_PULL0, 0x00000000);

	/* Construct inital RAMFC for new channel */
	switch(dev_priv->card_type)
	{
		case NV_04:
		case NV_05:
			nv04_graph_context_create(dev, channel);
			nouveau_nv04_context_init(dev, channel);
			break;
		case NV_10:
		case NV_17:
			nv10_graph_context_create(dev, channel);
			nouveau_nv10_context_init(dev, channel);
			break;
		case NV_20:
			ret = nv20_graph_context_create(dev, channel);
			if (ret) {
				nouveau_fifo_free(dev, channel);
				return ret;
			}
			nouveau_nv10_context_init(dev, channel);
			break;
		case NV_30:
			ret = nv30_graph_context_create(dev, channel);
			if (ret) {
				nouveau_fifo_free(dev, channel);
				return ret;
			}
			nouveau_nv30_context_init(dev, channel);
			break;
		case NV_40:
		case NV_44:
		case NV_50:
			ret = nv40_graph_context_create(dev, channel);
			if (ret) {
				nouveau_fifo_free(dev, channel);
				return ret;
			}
			nouveau_nv40_context_init(dev, channel);
			break;
	}

	/* enable the fifo dma operation */
	NV_WRITE(NV04_PFIFO_MODE,NV_READ(NV04_PFIFO_MODE)|(1<<channel));

	/* setup channel's default get/put values */
	NV_WRITE(NV03_FIFO_REGS_DMAPUT(channel), 0);
	NV_WRITE(NV03_FIFO_REGS_DMAGET(channel), 0);

	/* If this is the first channel, setup PFIFO ourselves.  For any
	 * other case, the GPU will handle this when it switches contexts.
	 */
	if (dev_priv->fifo_alloc_count == 0) {
		nouveau_fifo_context_restore(dev, channel);
		if (dev_priv->card_type >= NV_30) {
			struct nouveau_fifo *chan;
			uint32_t inst;

			chan = &dev_priv->fifos[channel];
			inst = nouveau_chip_instance_get(dev,
							 chan->ramin_grctx);

			/* see comments in nv40_graph_context_restore() */
			NV_WRITE(NV10_PGRAPH_CHANNEL_CTX_SIZE, inst);
                        if (dev_priv->card_type >= NV_40) {
                                NV_WRITE(0x40032C, inst | 0x01000000);
                                NV_WRITE(NV40_PFIFO_GRCTX_INSTANCE, inst);
                        }
		}
	}

	NV_WRITE(NV04_PFIFO_CACHE1_DMA_PUSH, 0x00000001);
	NV_WRITE(NV03_PFIFO_CACHE1_PUSH0, 0x00000001);
	NV_WRITE(NV04_PFIFO_CACHE1_PULL0, 0x00000001);
	NV_WRITE(NV04_PFIFO_CACHE1_PULL1, 0x00000001);

	/* reenable the fifo caches */
	NV_WRITE(NV03_PFIFO_CACHES, 0x00000001);

	dev_priv->fifo_alloc_count++;

	DRM_INFO("%s: initialised FIFO %d\n", __func__, channel);
	return 0;
}

/* stops a fifo */
void nouveau_fifo_free(drm_device_t* dev, int channel)
{
	drm_nouveau_private_t *dev_priv = dev->dev_private;
	struct nouveau_fifo *chan = &dev_priv->fifos[channel];
	int i;
	int ctx_size = nouveau_fifo_ctx_size(dev);

	chan->used = 0;
	DRM_INFO("%s: freeing fifo %d\n", __func__, channel);

	/* disable the fifo caches */
	NV_WRITE(NV03_PFIFO_CACHES, 0x00000000);

	NV_WRITE(NV04_PFIFO_MODE, NV_READ(NV04_PFIFO_MODE)&~(1<<channel));
	// FIXME XXX needs more code
	
	/* Clean RAMFC */
	for (i=0;i<ctx_size;i+=4) {
		DRM_DEBUG("RAMFC +%02x: 0x%08x\n", i, NV_READ(NV_RAMIN +
					dev_priv->ramfc_offset + 
					channel*ctx_size + i));
		NV_WRITE(NV_RAMIN + dev_priv->ramfc_offset +
				channel*ctx_size + i, 0);
	}

	/* Cleanup PGRAPH state */
	if (dev_priv->card_type >= NV_40)
		nouveau_instmem_free(dev, chan->ramin_grctx);
	else if (dev_priv->card_type >= NV_30) {
	}
	else if (dev_priv->card_type >= NV_20) {
		/* clear ctx table */
		INSTANCE_WR(dev_priv->ctx_table, channel, 0);
		nouveau_instmem_free(dev, chan->ramin_grctx);
	}

	/* reenable the fifo caches */
	NV_WRITE(NV03_PFIFO_CACHES, 0x00000001);

	/* Deallocate command buffer */
	if (chan->cmdbuf_mem)
		nouveau_mem_free(dev, chan->cmdbuf_mem);

	/* Destroy objects belonging to the channel */
	nouveau_object_cleanup(dev, channel);

	dev_priv->fifo_alloc_count--;
}

/* cleanups all the fifos from filp */
void nouveau_fifo_cleanup(drm_device_t* dev, DRMFILE filp)
{
	int i;
	drm_nouveau_private_t *dev_priv = dev->dev_private;

	DRM_DEBUG("clearing FIFO enables from filp\n");
	for(i=0;i<nouveau_fifo_number(dev);i++)
		if (dev_priv->fifos[i].used && dev_priv->fifos[i].filp==filp)
			nouveau_fifo_free(dev,i);
}

int
nouveau_fifo_owner(drm_device_t *dev, DRMFILE filp, int channel)
{
	drm_nouveau_private_t *dev_priv = dev->dev_private;

	if (channel >= nouveau_fifo_number(dev))
		return 0;
	if (dev_priv->fifos[channel].used == 0)
		return 0;
	return (dev_priv->fifos[channel].filp == filp);
}

/***********************************
 * ioctls wrapping the functions
 ***********************************/

static int nouveau_ioctl_fifo_alloc(DRM_IOCTL_ARGS)
{
	DRM_DEVICE;
	drm_nouveau_private_t *dev_priv = dev->dev_private;
	drm_nouveau_fifo_alloc_t init;
	int res;

	DRM_COPY_FROM_USER_IOCTL(init, (drm_nouveau_fifo_alloc_t __user *) data,
				 sizeof(init));

	res = nouveau_fifo_alloc(dev, &init.channel, filp);
	if (res)
		return res;

	/* this should probably disappear in the next abi break? */
	init.put_base = 0;

	/* make the fifo available to user space */
	/* first, the fifo control regs */
	init.ctrl      = dev_priv->mmio->offset + NV03_FIFO_REGS(init.channel);
	init.ctrl_size = NV03_FIFO_REGS_SIZE;
	res = drm_addmap(dev, init.ctrl, init.ctrl_size, _DRM_REGISTERS,
			 0, &dev_priv->fifos[init.channel].regs);
	if (res != 0)
		return res;

	/* pass back FIFO map info to the caller */
	init.cmdbuf      = dev_priv->fifos[init.channel].cmdbuf_mem->start;
	init.cmdbuf_size = dev_priv->fifos[init.channel].cmdbuf_mem->size;

	DRM_COPY_TO_USER_IOCTL((drm_nouveau_fifo_alloc_t __user *)data,
			       init, sizeof(init));
	return 0;
}

/***********************************
 * finally, the ioctl table
 ***********************************/

drm_ioctl_desc_t nouveau_ioctls[] = {
	[DRM_IOCTL_NR(DRM_NOUVEAU_FIFO_ALLOC)] = {nouveau_ioctl_fifo_alloc, DRM_AUTH},	
	[DRM_IOCTL_NR(DRM_NOUVEAU_OBJECT_INIT)] = {nouveau_ioctl_object_init, DRM_AUTH},
	[DRM_IOCTL_NR(DRM_NOUVEAU_DMA_OBJECT_INIT)] = {nouveau_ioctl_dma_object_init, DRM_AUTH},
	[DRM_IOCTL_NR(DRM_NOUVEAU_MEM_ALLOC)] = {nouveau_ioctl_mem_alloc, DRM_AUTH},
	[DRM_IOCTL_NR(DRM_NOUVEAU_MEM_FREE)] = {nouveau_ioctl_mem_free, DRM_AUTH},
	[DRM_IOCTL_NR(DRM_NOUVEAU_GETPARAM)] = {nouveau_ioctl_getparam, DRM_AUTH},
	[DRM_IOCTL_NR(DRM_NOUVEAU_SETPARAM)] = {nouveau_ioctl_setparam, DRM_AUTH|DRM_MASTER|DRM_ROOT_ONLY},	
};

int nouveau_max_ioctl = DRM_ARRAY_SIZE(nouveau_ioctls);
