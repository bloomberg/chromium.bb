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

#define NV20_GRCTX_SIZE (3529)

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

	NV_WRITE(NV_PGRAPH_RDI_INDEX, 0x2c80000);
	for (i = 0; i < 32; i++)
		NV_WRITE(NV_PGRAPH_RDI_DATA, 0);

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

	NV_WRITE(NV_PGRAPH_CHANNEL_CTX_SIZE, instance);
	NV_WRITE(NV_PGRAPH_CHANNEL_CTX_POINTER, 2 /* save ctx */);
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

	NV_WRITE(NV_PGRAPH_CTX_USER, channel << 24);
	NV_WRITE(NV_PGRAPH_CHANNEL_CTX_SIZE, instance);
	NV_WRITE(NV_PGRAPH_CHANNEL_CTX_POINTER, 1 /* restore ctx */);
}

void nouveau_nv20_context_switch(drm_device_t *dev)
{
	drm_nouveau_private_t *dev_priv = dev->dev_private;
	int channel, channel_old;

	channel=NV_READ(NV_PFIFO_CACH1_PSH1)&(nouveau_fifo_number(dev)-1);
	channel_old = (NV_READ(NV_PGRAPH_CTX_USER) >> 24) & (nouveau_fifo_number(dev)-1);

	DRM_INFO("NV: PGRAPH context switch interrupt channel %x -> %x\n",channel_old, channel);

	NV_WRITE(NV_PGRAPH_FIFO,0x0);

	nv20_graph_context_save_current(dev, channel_old);
	
	nouveau_wait_for_idle(dev);

	NV_WRITE(NV_PGRAPH_CTX_CONTROL, 0x10000000);

	nv20_graph_context_restore(dev, channel);

	nouveau_wait_for_idle(dev);
	
	if ((NV_READ(NV_PGRAPH_CTX_USER) >> 24) != channel)
		DRM_ERROR("nouveau_nv20_context_switch : wrong channel restored %x %x!!!\n", channel, NV_READ(NV_PGRAPH_CTX_USER) >> 24);

	NV_WRITE(NV_PGRAPH_CTX_CONTROL, 0x10010100);
	NV_WRITE(NV_PGRAPH_FFINTFC_ST2, NV_READ(NV_PGRAPH_FFINTFC_ST2)&0xCFFFFFFF);

	NV_WRITE(NV_PGRAPH_FIFO,0x1);
}

int nv20_graph_init(drm_device_t *dev) {
	drm_nouveau_private_t *dev_priv =
		(drm_nouveau_private_t *)dev->dev_private;
	int i;

	/* Create Context Pointer Table */
	dev_priv->ctx_table_size = 32 * 4;
	dev_priv->ctx_table = nouveau_instmem_alloc(dev, dev_priv->ctx_table_size, 4);
	if (!dev_priv->ctx_table)
		return DRM_ERR(ENOMEM);

	for (i=0; i< dev_priv->ctx_table_size; i+=4)
		INSTANCE_WR(dev_priv->ctx_table, i/4, 0x00000000);

	NV_WRITE(NV_PGRAPH_CHANNEL_CTX_TABLE, nouveau_chip_instance_get(dev, dev_priv->ctx_table));

	//XXX need to be done and save/restore for each fifo ???
	nv20_graph_rdi(dev);

	return 0;
}
