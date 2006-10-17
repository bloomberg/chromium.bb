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

/***********************************
 * functions doing the actual work
 ***********************************/

/* voir nv_xaa.c : NVResetGraphics
 * mémoire mappée par nv_driver.c : NVMapMem
 * voir nv_driver.c : NVPreInit 
 */

static void nouveau_fifo_init(drm_device_t* dev)
{
	drm_nouveau_private_t *dev_priv = dev->dev_private;
	
	/* Init PFIFO - This is an exact copy of what's done in the Xorg ddx so far.
	 *              We should be able to figure out what's happening from the
	 *              resources available..
	 */

	if (dev->irq_enabled)
		nouveau_irq_postinstall(dev);

	if (dev_priv->card_type >= NV_40)
		NV_WRITE(NV_PGRAPH_NV40_UNK220, dev_priv->fb_obj->instance >> 4);

	DRM_DEBUG("%s: setting FIFO %d active\n", __func__, dev_priv->cur_fifo);

	// FIXME remove all the stuff that's done in nouveau_fifo_alloc
	NV_WRITE(NV_PFIFO_CACHES, 0x00000000);
	NV_WRITE(NV_PFIFO_MODE, 0x00000000);

	NV_WRITE(NV_PFIFO_CACH1_PSH0, 0x00000000);
	NV_WRITE(NV_PFIFO_CACH1_PUL0, 0x00000000);
	if (dev_priv->card_type >= NV_40)
		NV_WRITE(NV_PFIFO_CACH1_PSH1, 0x00010000|dev_priv->cur_fifo);
	else
		NV_WRITE(NV_PFIFO_CACH1_PSH1, 0x00000100|dev_priv->cur_fifo);
	NV_WRITE(NV_PFIFO_CACH1_DMAP, dev_priv->cur_fifo * dev_priv->cmdbuf_ch_size);
	NV_WRITE(NV_PFIFO_CACH1_DMAG, dev_priv->cur_fifo * dev_priv->cmdbuf_ch_size);
	NV_WRITE(NV_PFIFO_CACH1_DMAI, dev_priv->cmdbuf_obj->instance >> 4);
	NV_WRITE(NV_PFIFO_CACH0_PSH0, 0x00000000);
	NV_WRITE(NV_PFIFO_CACH0_PUL0, 0x00000000);
	NV_WRITE(NV_PFIFO_SIZE , 0x0000FFFF);
	NV_WRITE(NV_PFIFO_CACH1_HASH, 0x0000FFFF);
	NV_WRITE(NV_PFIFO_RAMHT,
			(0x03 << 24) /* search 128 */ | 
			((dev_priv->objs.ht_bits - 9) << 16) |
			(dev_priv->objs.ht_base >> 8)
			);
	/* RAMFC needs to be at RAMIN+0x20000 on NV40, I currently don't know
	 * how to move it..
	 */
	dev_priv->ramfc_offset=0x20000;
	if (dev_priv->card_type < NV_40)
		NV_WRITE(NV_PFIFO_RAMFC, dev_priv->ramfc_offset>>8); /* RAMIN+0x11000 0.5k */
	else
		NV_WRITE(0x2220, 0x30002);
	dev_priv->ramro_offset=0x11200;
	NV_WRITE(NV_PFIFO_RAMRO, dev_priv->ramro_offset>>8); /* RAMIN+0x11200 0.5k */
	NV_WRITE(NV_PFIFO_CACH0_PUL1, 0x00000001);
	NV_WRITE(NV_PFIFO_CACH1_DMAC, 0x00000000);
	NV_WRITE(NV_PFIFO_CACH1_DMAS, 0x00000000);
	NV_WRITE(NV_PFIFO_CACH1_ENG, 0x00000000);
#ifdef __BIG_ENDIAN
		NV_WRITE(NV_PFIFO_CACH1_DMAF, NV_PFIFO_CACH1_DMAF_TRIG_112_BYTES|NV_PFIFO_CACH1_DMAF_SIZE_128_BYTES|NV_PFIFO_CACH1_DMAF_MAX_REQS_4|NV_PFIFO_CACH1_BIG_ENDIAN);
#else
		NV_WRITE(NV_PFIFO_CACH1_DMAF, NV_PFIFO_CACH1_DMAF_TRIG_112_BYTES|NV_PFIFO_CACH1_DMAF_SIZE_128_BYTES|NV_PFIFO_CACH1_DMAF_MAX_REQS_4);
#endif
	NV_WRITE(NV_PFIFO_CACH1_DMAPSH, 0x00000001);
	NV_WRITE(NV_PFIFO_CACH1_PSH0, 0x00000001);
	NV_WRITE(NV_PFIFO_CACH1_PUL0, 0x00000001);
	NV_WRITE(NV_PFIFO_CACH1_PUL1, 0x00000001);

	NV_WRITE(NV_PGRAPH_CTX_USER, 0x0);
	NV_WRITE(NV_PFIFO_DELAY_0, 0xff /* retrycount*/ );
	if (dev_priv->card_type >= NV_40)
		NV_WRITE(NV_PGRAPH_CTX_CONTROL, 0x00002001);
	else
		NV_WRITE(NV_PGRAPH_CTX_CONTROL, 0x10110000);

	NV_WRITE(NV_PFIFO_DMA_TIMESLICE, 0x001fffff);
	NV_WRITE(NV_PFIFO_CACHES, 0x00000001);

	DRM_DEBUG("%s: CACHE1 GET/PUT readback %d/%d\n", __func__,
			NV_READ(NV_PFIFO_CACH1_DMAG),
			NV_READ(NV_PFIFO_CACH1_DMAP));

	DRM_INFO("%s: OK\n", __func__);
}

static int nouveau_dma_init(struct drm_device *dev)
{
	drm_nouveau_private_t *dev_priv = dev->dev_private;
	struct nouveau_config *config = &dev_priv->config;
	struct mem_block *cb;
	int cb_min_size = nouveau_fifo_number(dev) * NV03_FIFO_SIZE;

	/* XXX this should be done earlier on init */
	nouveau_hash_table_init(dev);

	if (dev_priv->card_type >= NV_40)
		dev_priv->fb_obj = nouveau_dma_object_create(dev,
				0, nouveau_mem_fb_amount(dev),
				NV_DMA_ACCESS_RW, NV_DMA_TARGET_VIDMEM);

	/* Defaults for unconfigured values */
	if (!config->cmdbuf.location)
		config->cmdbuf.location = NOUVEAU_MEM_FB;
	if (!config->cmdbuf.size || config->cmdbuf.size < cb_min_size)
		config->cmdbuf.size = cb_min_size;

	cb = nouveau_mem_alloc(dev, 0, config->cmdbuf.size,
			config->cmdbuf.location, (DRMFILE)-2);
	/* Try defaults if that didn't succeed */
	if (!cb) {
		config->cmdbuf.location = NOUVEAU_MEM_FB;
		config->cmdbuf.size = cb_min_size;
		cb = nouveau_mem_alloc(dev, 0, config->cmdbuf.size,
				config->cmdbuf.location, (DRMFILE)-2);
	}
	if (!cb) {
		DRM_ERROR("Couldn't allocate DMA command buffer.\n");
		return DRM_ERR(ENOMEM);
	}

	if (config->cmdbuf.location == NOUVEAU_MEM_AGP)
		dev_priv->cmdbuf_obj = nouveau_dma_object_create(dev,
				cb->start, cb->size, NV_DMA_ACCESS_RO, NV_DMA_TARGET_AGP);
	else
		dev_priv->cmdbuf_obj = nouveau_dma_object_create(dev,
				cb->start - drm_get_resource_start(dev, 1),
				cb->size, NV_DMA_ACCESS_RO, NV_DMA_TARGET_VIDMEM);
	dev_priv->cmdbuf_ch_size = (uint32_t)cb->size / nouveau_fifo_number(dev);
	dev_priv->cmdbuf_alloc = cb;

	nouveau_fifo_init(dev);
	DRM_INFO("DMA command buffer is %dKiB at 0x%08x(%s)\n",
			(uint32_t)cb->size>>10, (uint32_t)cb->start,
			config->cmdbuf.location == NOUVEAU_MEM_FB ? "VRAM" : "AGP");
	DRM_INFO("FIFO size is %dKiB\n", dev_priv->cmdbuf_ch_size>>10);

	return 0;
}

static void nouveau_context_init(drm_device_t *dev,
				 drm_nouveau_fifo_alloc_t *init)
{
	drm_nouveau_private_t *dev_priv = dev->dev_private;
	uint32_t ctx_addr,ctx_size;
	int i;

	switch(dev_priv->card_type)
	{
		case NV_03:
		case NV_04:
		case NV_05:
			ctx_size=32;
			break;
		case NV_10:
		case NV_20:
		case NV_30:
		default:
			ctx_size=64;
			break;
	}

	ctx_addr=NV_RAMIN+dev_priv->ramfc_offset+init->channel*ctx_size;
	// clear the fifo context
	for(i=0;i<ctx_size/4;i++)
		NV_WRITE(ctx_addr+4*i,0x0);

	NV_WRITE(ctx_addr,init->put_base);
	NV_WRITE(ctx_addr+4,init->put_base);
	if (dev_priv->card_type <= NV_05)
	{
		// that's what is done in nvosdk, but that part of the code is buggy so...
		NV_WRITE(ctx_addr+8,dev_priv->cmdbuf_obj->instance >> 4);
#ifdef __BIG_ENDIAN
		NV_WRITE(ctx_addr+16,NV_PFIFO_CACH1_DMAF_TRIG_112_BYTES|NV_PFIFO_CACH1_DMAF_SIZE_128_BYTES|NV_PFIFO_CACH1_DMAF_MAX_REQS_4|NV_PFIFO_CACH1_BIG_ENDIAN);
#else
		NV_WRITE(ctx_addr+16,NV_PFIFO_CACH1_DMAF_TRIG_112_BYTES|NV_PFIFO_CACH1_DMAF_SIZE_128_BYTES|NV_PFIFO_CACH1_DMAF_MAX_REQS_4);
#endif
	}
	else
	{
		NV_WRITE(ctx_addr+12,dev_priv->cmdbuf_obj->instance >> 4/*DMA INST/DMA COUNT*/);
#ifdef __BIG_ENDIAN
		NV_WRITE(ctx_addr+20,NV_PFIFO_CACH1_DMAF_TRIG_112_BYTES|NV_PFIFO_CACH1_DMAF_SIZE_128_BYTES|NV_PFIFO_CACH1_DMAF_MAX_REQS_4|NV_PFIFO_CACH1_BIG_ENDIAN);
#else
		NV_WRITE(ctx_addr+20,NV_PFIFO_CACH1_DMAF_TRIG_112_BYTES|NV_PFIFO_CACH1_DMAF_SIZE_128_BYTES|NV_PFIFO_CACH1_DMAF_MAX_REQS_4);
#endif
	}

}

static void nouveau_nv40_context_init(drm_device_t *dev,
				      drm_nouveau_fifo_alloc_t *init)
{
#define RAMFC_WR(offset, val) NV_WRITE(fifoctx + NV40_RAMFC_##offset, (val))
	drm_nouveau_private_t *dev_priv = dev->dev_private;
	uint32_t fifoctx;
	int i;

	fifoctx = NV_RAMIN + dev_priv->ramfc_offset + init->channel*128;
	for (i=0;i<128;i+=4)
		NV_WRITE(fifoctx + i, 0);

	/* Fill entries that are seen filled in dumps of nvidia driver just
	 * after channel's is put into DMA mode
	 */
	RAMFC_WR(DMA_PUT       , init->put_base);
	RAMFC_WR(DMA_GET       , init->put_base);
	RAMFC_WR(DMA_INSTANCE  , dev_priv->cmdbuf_obj->instance >> 4);
	RAMFC_WR(DMA_FETCH     , 0x30086078);
	RAMFC_WR(DMA_SUBROUTINE, init->put_base);
	RAMFC_WR(GRCTX_INSTANCE, 0); /* XXX */
	RAMFC_WR(DMA_TIMESLICE , 0x0001FFFF);
#undef RAMFC_WR
}

/* allocates and initializes a fifo for user space consumption */
static int nouveau_fifo_alloc(drm_device_t* dev,drm_nouveau_fifo_alloc_t* init, DRMFILE filp)
{
	int i;
	int ret;
	drm_nouveau_private_t *dev_priv = dev->dev_private;

	/* Init cmdbuf on first FIFO init, this is delayed until now to
	 * give the ddx a chance to configure the cmdbuf with SETPARAM
	 */
	if (!dev_priv->cmdbuf_alloc) {
		ret = nouveau_dma_init(dev);
		if (ret)
			return ret;
	}

	/*
	 * Alright, here is the full story
	 * Nvidia cards have multiple hw fifo contexts (praise them for that, 
	 * no complicated crash-prone context switches)
	 * We allocate a new context for each app and let it write to it directly 
	 * (woo, full userspace command submission !)
	 * When there are no more contexts, you lost
	 */
	for(i=0;i<nouveau_fifo_number(dev);i++)
		if (dev_priv->fifos[i].used==0)
			break;

	DRM_INFO("Allocating FIFO number %d\n", i);
	/* no more fifos. you lost. */
	if (i==nouveau_fifo_number(dev))
		return DRM_ERR(EINVAL);

	/* that fifo is used */
	dev_priv->fifos[i].used=1;
	dev_priv->fifos[i].filp=filp;

	init->channel  = i;
	init->put_base = i*dev_priv->cmdbuf_ch_size;
	dev_priv->cur_fifo = init->channel;

	nouveau_wait_for_idle(dev);

	/* disable the fifo caches */
	NV_WRITE(NV_PFIFO_CACHES, 0x00000000);
	NV_WRITE(NV_PFIFO_CACH1_DMAPSH, NV_READ(NV_PFIFO_CACH1_DMAPSH)&(~0x1));
	NV_WRITE(NV_PFIFO_CACH1_PSH0, 0x00000000);
	NV_WRITE(NV_PFIFO_CACH1_PUL0, 0x00000000);

	if (dev_priv->card_type < NV_40)
		nouveau_context_init(dev, init);
	else
		nouveau_nv40_context_init(dev, init);

	/* enable the fifo dma operation */
	NV_WRITE(NV_PFIFO_MODE,NV_READ(NV_PFIFO_MODE)|(1<<init->channel));

	NV_WRITE(NV03_FIFO_REGS_DMAPUT(init->channel), init->put_base);
	NV_WRITE(NV03_FIFO_REGS_DMAGET(init->channel), init->put_base);

if (init->channel == 0) {
	// FIXME check if we need to refill the time quota with something like NV_WRITE(0x204C, 0x0003FFFF);

	if (dev_priv->card_type >= NV_40)
		NV_WRITE(NV_PFIFO_CACH1_PSH1, 0x00010000|dev_priv->cur_fifo);
	else
		NV_WRITE(NV_PFIFO_CACH1_PSH1, 0x00000100|dev_priv->cur_fifo);

	NV_WRITE(NV_PFIFO_CACH1_DMAP, init->put_base);
	NV_WRITE(NV_PFIFO_CACH1_DMAG, init->put_base);
	NV_WRITE(NV_PFIFO_CACH1_DMAI, dev_priv->cmdbuf_obj->instance >> 4);
	NV_WRITE(NV_PFIFO_SIZE , 0x0000FFFF);
	NV_WRITE(NV_PFIFO_CACH1_HASH, 0x0000FFFF);

	NV_WRITE(NV_PFIFO_CACH0_PUL1, 0x00000001);
	NV_WRITE(NV_PFIFO_CACH1_DMAC, 0x00000000);
	NV_WRITE(NV_PFIFO_CACH1_DMAS, 0x00000000);
	NV_WRITE(NV_PFIFO_CACH1_ENG, 0x00000000);
#ifdef __BIG_ENDIAN
		NV_WRITE(NV_PFIFO_CACH1_DMAF, NV_PFIFO_CACH1_DMAF_TRIG_112_BYTES|NV_PFIFO_CACH1_DMAF_SIZE_128_BYTES|NV_PFIFO_CACH1_DMAF_MAX_REQS_4|NV_PFIFO_CACH1_BIG_ENDIAN);
#else
		NV_WRITE(NV_PFIFO_CACH1_DMAF, NV_PFIFO_CACH1_DMAF_TRIG_112_BYTES|NV_PFIFO_CACH1_DMAF_SIZE_128_BYTES|NV_PFIFO_CACH1_DMAF_MAX_REQS_4);
#endif
}
	NV_WRITE(NV_PFIFO_CACH1_DMAPSH, 0x00000001);
	NV_WRITE(NV_PFIFO_CACH1_PSH0, 0x00000001);
	NV_WRITE(NV_PFIFO_CACH1_PUL0, 0x00000001);
	NV_WRITE(NV_PFIFO_CACH1_PUL1, 0x00000001);

	/* reenable the fifo caches */
	NV_WRITE(NV_PFIFO_CACHES, 0x00000001);

	/* make the fifo available to user space */
	/* first, the fifo control regs */
	init->ctrl      = dev_priv->mmio->offset + NV03_FIFO_REGS(init->channel);
	init->ctrl_size = NV03_FIFO_REGS_SIZE;
	ret = drm_addmap(dev, init->ctrl, init->ctrl_size, _DRM_REGISTERS,
			 0, &dev_priv->fifos[init->channel].regs);
	if (ret != 0)
		return ret;

	/* then, the fifo itself */
	init->cmdbuf       = dev_priv->cmdbuf_alloc->start;
	init->cmdbuf      += init->channel * dev_priv->cmdbuf_ch_size;
	init->cmdbuf_size  = dev_priv->cmdbuf_ch_size;
	ret = drm_addmap(dev, init->cmdbuf, init->cmdbuf_size, _DRM_REGISTERS,
			 0, &dev_priv->fifos[init->channel].map);
	if (ret != 0)
		return ret;

	/* FIFO has no objects yet */
	dev_priv->fifos[init->channel].objs = NULL;

	DRM_INFO("%s: initialised FIFO %d\n", __func__, init->channel);
	return 0;
}

/* stops a fifo */
void nouveau_fifo_free(drm_device_t* dev,int n)
{
	drm_nouveau_private_t *dev_priv = dev->dev_private;
	int i;

	dev_priv->fifos[n].used=0;
	DRM_INFO("%s: freeing fifo %d\n", __func__, n);

	/* disable the fifo caches */
	NV_WRITE(NV_PFIFO_CACHES, 0x00000000);

	NV_WRITE(NV_PFIFO_MODE,NV_READ(NV_PFIFO_MODE)&~(1<<n));
	// FIXME XXX needs more code
	
	/* Clean RAMFC */
	for (i=0;i<128;i+=4) {
		DRM_DEBUG("RAMFC +%02x: 0x%08x\n", i, NV_READ(NV_RAMIN +
					dev_priv->ramfc_offset + n*128 + i));
		NV_WRITE(NV_RAMIN + dev_priv->ramfc_offset + n*128 + i, 0);
	}

	/* reenable the fifo caches */
	NV_WRITE(NV_PFIFO_CACHES, 0x00000001);
}

/* cleanups all the fifos from filp */
void nouveau_fifo_cleanup(drm_device_t* dev, DRMFILE filp)
{
	int i;
	drm_nouveau_private_t *dev_priv = dev->dev_private;

	DRM_DEBUG("clearing FIFO enables from filp\n");
	for(i=0;i<nouveau_fifo_number(dev);i++)
		if (dev_priv->fifos[i].filp==filp)
			nouveau_fifo_free(dev,i);

	/* check we still point at an active channel */
	if (dev_priv->fifos[dev_priv->cur_fifo].used == 0) {	
		DRM_DEBUG("%s: cur_fifo is no longer owned.\n", __func__);
		for (i=0;i<nouveau_fifo_number(dev);i++)
			if (dev_priv->fifos[i].used) break;
		if (i==nouveau_fifo_number(dev))
			i=0;
		DRM_DEBUG("%s: new cur_fifo is %d\n", __func__, i);
		dev_priv->cur_fifo = i;
	}

/*	if (dev_priv->cmdbuf_alloc)
		nouveau_fifo_init(dev);*/
}

int nouveau_fifo_id_get(drm_device_t* dev, DRMFILE filp)
{
	drm_nouveau_private_t *dev_priv=dev->dev_private;
	int i;

	for(i=0;i<nouveau_fifo_number(dev);i++)
		if (dev_priv->fifos[i].filp == filp)
			return i;
	return -1;
}

/***********************************
 * ioctls wrapping the functions
 ***********************************/

static int nouveau_ioctl_fifo_alloc(DRM_IOCTL_ARGS)
{
	DRM_DEVICE;
	drm_nouveau_fifo_alloc_t init;
	int res;
	DRM_COPY_FROM_USER_IOCTL(init, (drm_nouveau_fifo_alloc_t __user *) data, sizeof(init));

	res=nouveau_fifo_alloc(dev,&init,filp);
	if (!res)
		DRM_COPY_TO_USER_IOCTL((drm_nouveau_fifo_alloc_t __user *)data, init, sizeof(init));

	return res;
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


