/* 
 * Copyright 2007 Matthieu CASTET <castet.matthieu@free.fr>
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

#define NV20_GRCTX_SIZE (3529*4)

int nv20_graph_context_create(drm_device_t *dev, int channel) {
	drm_nouveau_private_t *dev_priv =
		(drm_nouveau_private_t *)dev->dev_private;
	struct nouveau_fifo *chan = &dev_priv->fifos[channel];
	unsigned int ctx_size = NV20_GRCTX_SIZE;
	int i;

	/* Alloc and clear RAMIN to store the context */
	chan->ramin_grctx = nouveau_instmem_alloc(dev, ctx_size, 4);
	if (!chan->ramin_grctx)
		return DRM_ERR(ENOMEM);
	for (i=0; i<ctx_size; i+=4)
		INSTANCE_WR(chan->ramin_grctx, i/4, 0x00000000);

	/* Initialise default context values */
	INSTANCE_WR(chan->ramin_grctx, 10, channel << 24); /* CTX_USER */

	INSTANCE_WR(dev_priv->ctx_table, channel, nouveau_chip_instance_get(dev, chan->ramin_grctx));

	return 0;
}

static void nv20_graph_rdi(drm_device_t *dev) {
	drm_nouveau_private_t *dev_priv =
		(drm_nouveau_private_t *)dev->dev_private;
	int i;

	NV_WRITE(NV10_PGRAPH_RDI_INDEX, 0x2c80000);
	for (i = 0; i < 32; i++)
		NV_WRITE(NV10_PGRAPH_RDI_DATA, 0);

	nouveau_wait_for_idle(dev);
}

/* Save current context (from PGRAPH) into the channel's context
 */
static void nv20_graph_context_save_current(drm_device_t *dev, int channel) {
	drm_nouveau_private_t *dev_priv =
		(drm_nouveau_private_t *)dev->dev_private;
	uint32_t instance;

	instance = INSTANCE_RD(dev_priv->ctx_table, channel);
	if (!instance) {
		return;
	}
	if (instance != nouveau_chip_instance_get(dev, dev_priv->fifos[channel].ramin_grctx))
		DRM_ERROR("nv20_graph_context_save_current : bad instance\n");

	NV_WRITE(NV10_PGRAPH_CHANNEL_CTX_SIZE, instance);
	NV_WRITE(NV10_PGRAPH_CHANNEL_CTX_POINTER, 2 /* save ctx */);
}


/* Restore the context for a specific channel into PGRAPH
 */
static void nv20_graph_context_restore(drm_device_t *dev, int channel) {
	drm_nouveau_private_t *dev_priv =
		(drm_nouveau_private_t *)dev->dev_private;
	uint32_t instance;

	instance = INSTANCE_RD(dev_priv->ctx_table, channel);
	if (!instance) {
		return;
	}
	if (instance != nouveau_chip_instance_get(dev, dev_priv->fifos[channel].ramin_grctx))
		DRM_ERROR("nv20_graph_context_restore_current : bad instance\n");

	NV_WRITE(NV10_PGRAPH_CTX_USER, channel << 24);
	NV_WRITE(NV10_PGRAPH_CHANNEL_CTX_SIZE, instance);
	NV_WRITE(NV10_PGRAPH_CHANNEL_CTX_POINTER, 1 /* restore ctx */);
}

void nouveau_nv20_context_switch(drm_device_t *dev)
{
	drm_nouveau_private_t *dev_priv = dev->dev_private;
	int channel, channel_old;

	channel=NV_READ(NV03_PFIFO_CACHE1_PUSH1)&(nouveau_fifo_number(dev)-1);
	channel_old = (NV_READ(NV10_PGRAPH_CTX_USER) >> 24) & (nouveau_fifo_number(dev)-1);

	DRM_DEBUG("NV: PGRAPH context switch interrupt channel %x -> %x\n",channel_old, channel);

	NV_WRITE(NV04_PGRAPH_FIFO,0x0);

	nv20_graph_context_save_current(dev, channel_old);
	
	nouveau_wait_for_idle(dev);

	NV_WRITE(NV10_PGRAPH_CTX_CONTROL, 0x10000000);

	nv20_graph_context_restore(dev, channel);

	nouveau_wait_for_idle(dev);
	
	if ((NV_READ(NV10_PGRAPH_CTX_USER) >> 24) != channel)
		DRM_ERROR("nouveau_nv20_context_switch : wrong channel restored %x %x!!!\n", channel, NV_READ(NV10_PGRAPH_CTX_USER) >> 24);

	NV_WRITE(NV10_PGRAPH_CTX_CONTROL, 0x10010100);
	NV_WRITE(NV10_PGRAPH_FFINTFC_ST2, NV_READ(NV10_PGRAPH_FFINTFC_ST2)&0xCFFFFFFF);

	NV_WRITE(NV04_PGRAPH_FIFO,0x1);
}

int nv20_graph_init(drm_device_t *dev) {
	drm_nouveau_private_t *dev_priv =
		(drm_nouveau_private_t *)dev->dev_private;
	int i;
	uint32_t tmp, vramsz;

	NV_WRITE(NV03_PMC_ENABLE, NV_READ(NV03_PMC_ENABLE) &
			~NV_PMC_ENABLE_PGRAPH);
	NV_WRITE(NV03_PMC_ENABLE, NV_READ(NV03_PMC_ENABLE) |
			 NV_PMC_ENABLE_PGRAPH);

	/* Create Context Pointer Table */
	dev_priv->ctx_table_size = 32 * 4;
	dev_priv->ctx_table = nouveau_instmem_alloc(dev, dev_priv->ctx_table_size, 4);
	if (!dev_priv->ctx_table)
		return DRM_ERR(ENOMEM);

	for (i=0; i< dev_priv->ctx_table_size; i+=4)
		INSTANCE_WR(dev_priv->ctx_table, i/4, 0x00000000);

	NV_WRITE(NV10_PGRAPH_CHANNEL_CTX_TABLE, nouveau_chip_instance_get(dev, dev_priv->ctx_table));

	//XXX need to be done and save/restore for each fifo ???
	nv20_graph_rdi(dev);

	NV_WRITE(NV03_PGRAPH_INTR_EN, 0x00000000);
	NV_WRITE(NV03_PGRAPH_INTR   , 0xFFFFFFFF);

	NV_WRITE(NV04_PGRAPH_DEBUG_0, 0xFFFFFFFF);
	NV_WRITE(NV04_PGRAPH_DEBUG_0, 0x00000000);
	NV_WRITE(NV04_PGRAPH_DEBUG_1, 0x00118700);
	NV_WRITE(NV04_PGRAPH_DEBUG_3, 0xF20E0431);
	NV_WRITE(NV10_PGRAPH_DEBUG_4, 0x00000000);
	NV_WRITE(0x40009C           , 0x00000040);

	if (dev_priv->chipset >= 0x25) {
		NV_WRITE(0x400890, 0x00080000);
		NV_WRITE(0x400610, 0x304B1FB6);
		NV_WRITE(0x400B80, 0x18B82880);
		NV_WRITE(0x400B84, 0x44000000);
		NV_WRITE(0x400098, 0x40000080);
		NV_WRITE(0x400B88, 0x000000ff);
	} else {
		NV_WRITE(0x400880, 0x00080000);
		NV_WRITE(0x400094, 0x00000005);
		NV_WRITE(0x400B80, 0x45CAA208);
		NV_WRITE(0x400B84, 0x24000000);
		NV_WRITE(0x400098, 0x00000040);
		NV_WRITE(NV10_PGRAPH_RDI_INDEX, 0x00E00038);
		NV_WRITE(NV10_PGRAPH_RDI_DATA , 0x00000030);
		NV_WRITE(NV10_PGRAPH_RDI_INDEX, 0x00E10038);
		NV_WRITE(NV10_PGRAPH_RDI_DATA , 0x00000030);
	}

	/* copy tile info from PFB */
	for (i=0; i<NV10_PFB_TILE__SIZE; i++) {
		NV_WRITE(NV10_PGRAPH_TILE(i), NV_READ(NV10_PFB_TILE(i)));
		NV_WRITE(NV10_PGRAPH_TLIMIT(i), NV_READ(NV10_PFB_TLIMIT(i)));
		NV_WRITE(NV10_PGRAPH_TSIZE(i), NV_READ(NV10_PFB_TSIZE(i)));
		NV_WRITE(NV10_PGRAPH_TSTATUS(i), NV_READ(NV10_PFB_TSTATUS(i)));
	}

	NV_WRITE(NV10_PGRAPH_CTX_CONTROL, 0x10010100);
	NV_WRITE(NV10_PGRAPH_STATE      , 0xFFFFFFFF);
	NV_WRITE(NV04_PGRAPH_FIFO       , 0x00000001);

	tmp = NV_READ(NV10_PGRAPH_SURFACE) & 0x0007ff00;
	NV_WRITE(NV10_PGRAPH_SURFACE, tmp);
	tmp = NV_READ(NV10_PGRAPH_SURFACE) | 0x00020100;
	NV_WRITE(NV10_PGRAPH_SURFACE, tmp);

	/* begin RAM config */
	vramsz = drm_get_resource_len(dev, 0) - 1;
	NV_WRITE(0x4009A4, NV_READ(NV04_PFB_CFG0));
	NV_WRITE(0x4009A8, NV_READ(NV04_PFB_CFG1));
	NV_WRITE(NV10_PGRAPH_RDI_INDEX, 0x00EA0000);
	NV_WRITE(NV10_PGRAPH_RDI_DATA , NV_READ(NV04_PFB_CFG0));
	NV_WRITE(NV10_PGRAPH_RDI_INDEX, 0x00EA0004);
	NV_WRITE(NV10_PGRAPH_RDI_DATA , NV_READ(NV04_PFB_CFG1));
	NV_WRITE(0x400820, 0);
	NV_WRITE(0x400824, 0);
	NV_WRITE(0x400864, vramsz-1);
	NV_WRITE(0x400868, vramsz-1);

	/* interesting.. the below overwrites some of the tile setup above.. */
	NV_WRITE(0x400B20, 0x00000000);
	NV_WRITE(0x400B04, 0xFFFFFFFF);

	NV_WRITE(NV03_PGRAPH_ABS_UCLIP_XMIN, 0);
	NV_WRITE(NV03_PGRAPH_ABS_UCLIP_YMIN, 0);
	NV_WRITE(NV03_PGRAPH_ABS_UCLIP_XMAX, 0x7fff);
	NV_WRITE(NV03_PGRAPH_ABS_UCLIP_YMAX, 0x7fff);

	return 0;
}

void nv20_graph_takedown(drm_device_t *dev)
{
}

