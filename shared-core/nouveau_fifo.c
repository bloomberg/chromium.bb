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

/* setup the fifo enable register */
static void nouveau_fifo_enable(drm_device_t* dev)
{
	int i;
	unsigned enable_val=0;
	drm_nouveau_private_t *dev_priv = dev->dev_private;

	for(i=31;i>=0;i--)
	{
		enable_val<<=1;
		if (dev_priv->fifos[i].used)
			enable_val|=1;
	}
	DRM_DEBUG("enable_val=0x%08x\n", enable_val);
	NV_WRITE(NV03_FIFO_ENABLE,enable_val);
}

/***********************************
 * functions doing the actual work
 ***********************************/

/* voir nv_xaa.c : NVResetGraphics
 * mémoire mappée par nv_driver.c : NVMapMem
 * voir nv_driver.c : NVPreInit 
 */

/* initializes a fifo */
static int nouveau_fifo_init(drm_device_t* dev,drm_nouveau_fifo_init_t* init, DRMFILE filp)
{
	int i;
	int ret;
	drm_nouveau_private_t *dev_priv = dev->dev_private;

	/*
	 * Alright, here is the full story
	 * Nvidia cards have multiple hw fifo contexts (praise them for that, 
	 * no complicated crash-prone context switches)
	 * X always uses context 0 (0x00800000)
	 * We allocate a new context for each app and let it write to it directly 
	 * (woo, full userspace command submission !)
	 * When there are no more contexts, you lost
	 */
	for(i=0;i<nouveau_fifo_number(dev);i++)
		if (dev_priv->fifos[i].used==0)
		{
			dev_priv->fifos[i].used=1;
			break;
		}

	/* no more fifos. you lost. */
	if (i==nouveau_fifo_number(dev))
		return DRM_ERR(EINVAL);

	/* that fifo is used */
	dev_priv->fifos[i].used=1;
	dev_priv->fifos[i].filp=filp;

	/* enable the fifo */
	nouveau_fifo_enable(dev);

	/* make the fifo available to user space */
	init->channel  = i;
	init->put_base = i*dev_priv->cmdbuf_ch_size;

	NV_WRITE(NV03_FIFO_REGS_DMAPUT(i), init->put_base);
	NV_WRITE(NV03_FIFO_REGS_DMAGET(i), init->put_base);

	/* first, the fifo control regs */
	init->ctrl      = dev_priv->mmio->offset + NV03_FIFO_REGS(i);
	init->ctrl_size = NV03_FIFO_REGS_SIZE;
	ret = drm_addmap(dev, init->ctrl, init->ctrl_size, _DRM_REGISTERS,
			 0, &dev_priv->fifos[i].regs);
	if (ret != 0)
		return ret;

	/* then, the fifo itself */
	init->cmdbuf       = dev_priv->cmdbuf_base;
	init->cmdbuf      += init->channel * dev_priv->cmdbuf_ch_size;
	init->cmdbuf_size  = dev_priv->cmdbuf_ch_size;
	ret = drm_addmap(dev, init->cmdbuf, init->cmdbuf_size, _DRM_REGISTERS,
			 0, &dev_priv->fifos[i].map);
	if (ret != 0)
		return ret;

	/* FIFO has no objects yet */
	dev_priv->fifos[i].objs = NULL;

	DRM_DEBUG("%s: initialised FIFO %d\n", __func__, i);
	dev_priv->cur_fifo = i;
	return 0;
}
static void nouveau_pfifo_init(drm_device_t* dev);

/* cleanups all the fifos from filp */
void nouveau_fifo_cleanup(drm_device_t * dev, DRMFILE filp)
{
	int i;
	drm_nouveau_private_t *dev_priv = dev->dev_private;

	DRM_DEBUG("clearing FIFO enables from filp\n");
	for(i=0;i<nouveau_fifo_number(dev);i++)
		if (dev_priv->fifos[i].filp==filp)
			dev_priv->fifos[i].used=0;

	if (dev_priv->cur_fifo == i) {	
		DRM_DEBUG("%s: cur_fifo is no longer owned.\n", __func__);
		for (i=0;i<nouveau_fifo_number(dev);i++)
			if (dev_priv->fifos[i].used) break;
		if (i==nouveau_fifo_number(dev))
			i=0;
		DRM_DEBUG("%s: new cur_fifo is %d\n", __func__, i);
		dev_priv->cur_fifo = i;
	}
	
	nouveau_pfifo_init(dev);
//	nouveau_fifo_enable(dev);
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

static void nouveau_pfifo_init(drm_device_t* dev)
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

	NV_WRITE(NV_PFIFO_CACHES, 0x00000000);
	nouveau_fifo_enable(dev);

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
	NV_WRITE(NV_PFIFO_CACH0_PUL1, 0x00000001);
	NV_WRITE(NV_PFIFO_CACH1_DMAC, 0x00000000);
	NV_WRITE(NV_PFIFO_CACH1_ENG, 0x00000000);
#ifdef __BIG_ENDIAN
		NV_WRITE(NV_PFIFO_CACH1_DMAF, 0x800F0078);
#else
		NV_WRITE(NV_PFIFO_CACH1_DMAF, 0x000F0078);
#endif
	NV_WRITE(NV_PFIFO_CACH1_DMAS, 0x00000001);
	NV_WRITE(NV_PFIFO_CACH1_PSH0, 0x00000001);
	NV_WRITE(NV_PFIFO_CACH1_PUL0, 0x00000001);
	NV_WRITE(NV_PFIFO_CACH1_PUL1, 0x00000001);

	NV_WRITE(NV_PFIFO_CACHES, 0x00000001);

	DRM_DEBUG("%s: CACHE1 GET/PUT readback %d/%d\n", __func__,
			NV_READ(NV_PFIFO_CACH1_DMAG),
			NV_READ(NV_PFIFO_CACH1_DMAP));
}

/***********************************
 * ioctls wrapping the functions
 ***********************************/

static int nouveau_ioctl_fifo_init(DRM_IOCTL_ARGS)
{
	DRM_DEVICE;
	drm_nouveau_fifo_init_t init;
	int res;
	DRM_COPY_FROM_USER_IOCTL(init, (drm_nouveau_fifo_init_t __user *) data, sizeof(init));

	res=nouveau_fifo_init(dev,&init,filp);
	if (!res)
		DRM_COPY_TO_USER_IOCTL((drm_nouveau_fifo_init_t __user *)data, init, sizeof(init));

	return res;
}

static int nouveau_ioctl_fifo_reinit(DRM_IOCTL_ARGS)
{
	DRM_DEVICE;
	
	nouveau_pfifo_init(dev);
	return 0;
}

/***********************************
 * finally, the ioctl table
 ***********************************/

drm_ioctl_desc_t nouveau_ioctls[] = {
	[DRM_IOCTL_NR(DRM_NOUVEAU_FIFO_INIT)] = {nouveau_ioctl_fifo_init, DRM_AUTH},
	[DRM_IOCTL_NR(DRM_NOUVEAU_PFIFO_REINIT)] = {nouveau_ioctl_fifo_reinit, DRM_AUTH},
	[DRM_IOCTL_NR(DRM_NOUVEAU_OBJECT_INIT)] = {nouveau_ioctl_object_init, DRM_AUTH},
	[DRM_IOCTL_NR(DRM_NOUVEAU_DMA_OBJECT_INIT)] = {nouveau_ioctl_dma_object_init, DRM_AUTH},
	[DRM_IOCTL_NR(DRM_NOUVEAU_MEM_ALLOC)] = {nouveau_ioctl_mem_alloc, DRM_AUTH},
	[DRM_IOCTL_NR(DRM_NOUVEAU_MEM_FREE)] = {nouveau_ioctl_mem_free, DRM_AUTH},
};

int nouveau_max_ioctl = DRM_ARRAY_SIZE(nouveau_ioctls);


