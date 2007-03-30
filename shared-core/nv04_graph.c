/* 
 * Copyright 2007 Stephane Marchesin
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
#include "nouveau_drm.h"
#include "nouveau_drv.h"

struct reg_interval
{
	uint32_t reg;
	int number;
} nv04_graph_ctx_regs [] = {
	{NV04_PGRAPH_CTX_SWITCH1,1},
	{NV04_PGRAPH_CTX_SWITCH2,1},
	{NV04_PGRAPH_CTX_SWITCH3,1},
	{NV04_PGRAPH_CTX_SWITCH4,1},
	{NV04_PGRAPH_CTX_CACHE1,1},
	{NV04_PGRAPH_CTX_CACHE2,1},
	{NV04_PGRAPH_CTX_CACHE3,1},
	{NV04_PGRAPH_CTX_CACHE4,1},
	{0x00400184,1},
	{0x004001a4,1},
	{0x004001c4,1},
	{0x004001e4,1},
	{0x00400188,1},
	{0x004001a8,1},
	{0x004001c8,1},
	{0x004001e8,1},
	{0x0040018c,1},
	{0x004001ac,1},
	{0x004001cc,1},
	{0x004001ec,1},
	{0x00400190,1},
	{0x004001b0,1},
	{0x004001d0,1},
	{0x004001f0,1},
	{0x00400194,1},
	{0x004001b4,1},
	{0x004001d4,1},
	{0x004001f4,1},
	{0x00400198,1},
	{0x004001b8,1},
	{0x004001d8,1},
	{0x004001f8,1},
	{0x0040019c,1},
	{0x004001bc,1},
	{0x004001dc,1},
	{0x004001fc,1},
	{0x00400174,1},
	{NV04_PGRAPH_DMA_START_0,1},
	{NV04_PGRAPH_DMA_START_1,1},
	{NV04_PGRAPH_DMA_LENGTH,1},
	{NV04_PGRAPH_DMA_MISC,1},
	{NV04_PGRAPH_DMA_PITCH,1},
	{NV04_PGRAPH_BOFFSET0,1},
	{NV04_PGRAPH_BBASE0,1},
	{NV04_PGRAPH_BLIMIT0,1},
	{NV04_PGRAPH_BOFFSET1,1},
	{NV04_PGRAPH_BBASE1,1},
	{NV04_PGRAPH_BLIMIT1,1},
	{NV04_PGRAPH_BOFFSET2,1},
	{NV04_PGRAPH_BBASE2,1},
	{NV04_PGRAPH_BLIMIT2,1},
	{NV04_PGRAPH_BOFFSET3,1},
	{NV04_PGRAPH_BBASE3,1},
	{NV04_PGRAPH_BLIMIT3,1},
	{NV04_PGRAPH_BOFFSET4,1},
	{NV04_PGRAPH_BBASE4,1},
	{NV04_PGRAPH_BLIMIT4,1},
	{NV04_PGRAPH_BOFFSET5,1},
	{NV04_PGRAPH_BBASE5,1},
	{NV04_PGRAPH_BLIMIT5,1},
	{NV04_PGRAPH_BPITCH0,1},
	{NV04_PGRAPH_BPITCH1,1},
	{NV04_PGRAPH_BPITCH2,1},
	{NV04_PGRAPH_BPITCH3,1},
	{NV04_PGRAPH_BPITCH4,1},
	{NV04_PGRAPH_SURFACE,1},
	{NV04_PGRAPH_STATE,1},
	{NV04_PGRAPH_BSWIZZLE2,1},
	{NV04_PGRAPH_BSWIZZLE5,1},
	{NV04_PGRAPH_BPIXEL,1},
	{NV04_PGRAPH_NOTIFY,1},
	{NV04_PGRAPH_PATT_COLOR0,1},
	{NV04_PGRAPH_PATT_COLOR1,1},
	{NV04_PGRAPH_PATT_COLORRAM,64},
	{NV04_PGRAPH_PATTERN,1},
	{0x0040080c,1},
	{NV04_PGRAPH_PATTERN_SHAPE,1},
	{0x00400600,1},
	{NV04_PGRAPH_ROP3,1},
	{NV04_PGRAPH_CHROMA,1},
	{NV04_PGRAPH_BETA_AND,1},
	{NV04_PGRAPH_BETA_PREMULT,1},
	{NV04_PGRAPH_CONTROL0,1},
	{NV04_PGRAPH_CONTROL1,1},
	{NV04_PGRAPH_CONTROL2,1},
	{NV04_PGRAPH_BLEND,1},
	{NV04_PGRAPH_STORED_FMT,1},
	{NV04_PGRAPH_SOURCE_COLOR,1},
	{0x00400560,1},
	{0x00400568,1},
	{0x00400564,1},
	{0x0040056c,1},
	{0x00400400,1},
	{0x00400480,1},
	{0x00400404,1},
	{0x00400484,1},
	{0x00400408,1},
	{0x00400488,1},
	{0x0040040c,1},
	{0x0040048c,1},
	{0x00400410,1},
	{0x00400490,1},
	{0x00400414,1},
	{0x00400494,1},
	{0x00400418,1},
	{0x00400498,1},
	{0x0040041c,1},
	{0x0040049c,1},
	{0x00400420,1},
	{0x004004a0,1},
	{0x00400424,1},
	{0x004004a4,1},
	{0x00400428,1},
	{0x004004a8,1},
	{0x0040042c,1},
	{0x004004ac,1},
	{0x00400430,1},
	{0x004004b0,1},
	{0x00400434,1},
	{0x004004b4,1},
	{0x00400438,1},
	{0x004004b8,1},
	{0x0040043c,1},
	{0x004004bc,1},
	{0x00400440,1},
	{0x004004c0,1},
	{0x00400444,1},
	{0x004004c4,1},
	{0x00400448,1},
	{0x004004c8,1},
	{0x0040044c,1},
	{0x004004cc,1},
	{0x00400450,1},
	{0x004004d0,1},
	{0x00400454,1},
	{0x004004d4,1},
	{0x00400458,1},
	{0x004004d8,1},
	{0x0040045c,1},
	{0x004004dc,1},
	{0x00400460,1},
	{0x004004e0,1},
	{0x00400464,1},
	{0x004004e4,1},
	{0x00400468,1},
	{0x004004e8,1},
	{0x0040046c,1},
	{0x004004ec,1},
	{0x00400470,1},
	{0x004004f0,1},
	{0x00400474,1},
	{0x004004f4,1},
	{0x00400478,1},
	{0x004004f8,1},
	{0x0040047c,1},
	{0x004004fc,1},
	{0x0040053c,1},
	{0x00400544,1},
	{0x00400540,1},
	{0x00400548,1},
	{0x00400560,1},
	{0x00400568,1},
	{0x00400564,1},
	{0x0040056c,1},
	{0x00400534,1},
	{0x00400538,1},
	{0x00400514,1},
	{0x00400518,1},
	{0x0040051c,1},
	{0x00400520,1},
	{0x00400524,1},
	{0x00400528,1},
	{0x0040052c,1},
	{0x00400530,1},
	{0x00400d00,1},
	{0x00400d40,1},
	{0x00400d80,1},
	{0x00400d04,1},
	{0x00400d44,1},
	{0x00400d84,1},
	{0x00400d08,1},
	{0x00400d48,1},
	{0x00400d88,1},
	{0x00400d0c,1},
	{0x00400d4c,1},
	{0x00400d8c,1},
	{0x00400d10,1},
	{0x00400d50,1},
	{0x00400d90,1},
	{0x00400d14,1},
	{0x00400d54,1},
	{0x00400d94,1},
	{0x00400d18,1},
	{0x00400d58,1},
	{0x00400d98,1},
	{0x00400d1c,1},
	{0x00400d5c,1},
	{0x00400d9c,1},
	{0x00400d20,1},
	{0x00400d60,1},
	{0x00400da0,1},
	{0x00400d24,1},
	{0x00400d64,1},
	{0x00400da4,1},
	{0x00400d28,1},
	{0x00400d68,1},
	{0x00400da8,1},
	{0x00400d2c,1},
	{0x00400d6c,1},
	{0x00400dac,1},
	{0x00400d30,1},
	{0x00400d70,1},
	{0x00400db0,1},
	{0x00400d34,1},
	{0x00400d74,1},
	{0x00400db4,1},
	{0x00400d38,1},
	{0x00400d78,1},
	{0x00400db8,1},
	{0x00400d3c,1},
	{0x00400d7c,1},
	{0x00400dbc,1},
	{0x00400590,1},
	{0x00400594,1},
	{0x00400598,1},
	{0x0040059c,1},
	{0x004005a8,1},
	{0x004005ac,1},
	{0x004005b0,1},
	{0x004005b4,1},
	{0x004005c0,1},
	{0x004005c4,1},
	{0x004005c8,1},
	{0x004005cc,1},
	{0x004005d0,1},
	{0x004005d4,1},
	{0x004005d8,1},
	{0x004005dc,1},
	{0x004005e0,1},
	{NV04_PGRAPH_PASSTHRU_0,1},
	{NV04_PGRAPH_PASSTHRU_1,1},
	{NV04_PGRAPH_PASSTHRU_2,1},
	{NV04_PGRAPH_DVD_COLORFMT,1},
	{NV04_PGRAPH_SCALED_FORMAT,1},
	{NV04_PGRAPH_MISC24_0,1},
	{NV04_PGRAPH_MISC24_1,1},
	{NV04_PGRAPH_MISC24_2,1},
	{0x00400500,1},
	{0x00400504,1},
	{NV04_PGRAPH_VALID1,1},
	{NV04_PGRAPH_VALID2,1}


};

void nouveau_nv04_context_switch(drm_device_t *dev)
{
	drm_nouveau_private_t *dev_priv = dev->dev_private;
	int channel, channel_old, i, j, index;

	channel=NV_READ(NV03_PFIFO_CACHE1_PUSH1)&(nouveau_fifo_number(dev)-1);
	channel_old = (NV_READ(NV04_PGRAPH_CTX_USER) >> 24) & (nouveau_fifo_number(dev)-1);

	DRM_DEBUG("NV: PGRAPH context switch interrupt channel %x -> %x\n",channel_old, channel);

	NV_WRITE(NV03_PFIFO_CACHES, 0x0);
	NV_WRITE(NV04_PFIFO_CACHE0_PULL0, 0x0);
	NV_WRITE(NV04_PFIFO_CACHE1_PULL0, 0x0);
	NV_WRITE(NV04_PGRAPH_FIFO,0x0);

	nouveau_wait_for_idle(dev);

	// save PGRAPH context
	index=0;
	for (i = 0; i<sizeof(nv04_graph_ctx_regs)/sizeof(nv04_graph_ctx_regs[0]); i++)
		for (j = 0; j<nv04_graph_ctx_regs[i].number; j++)
		{
			dev_priv->fifos[channel_old].pgraph_ctx[index] = NV_READ(nv04_graph_ctx_regs[i].reg+j*4);
			index++;
		}

	NV_WRITE(NV04_PGRAPH_CTX_CONTROL, 0x10000000);
	NV_WRITE(NV04_PGRAPH_CTX_USER, (NV_READ(NV04_PGRAPH_CTX_USER) & 0xffffff) | (0x0f << 24));

	// restore PGRAPH context
	index=0;
	for (i = 0; i<sizeof(nv04_graph_ctx_regs)/sizeof(nv04_graph_ctx_regs[0]); i++)
		for (j = 0; j<nv04_graph_ctx_regs[i].number; j++)
		{
			NV_WRITE(nv04_graph_ctx_regs[i].reg+j*4, dev_priv->fifos[channel].pgraph_ctx[index]);
			index++;
		}

	NV_WRITE(NV04_PGRAPH_CTX_CONTROL, 0x10010100);
	NV_WRITE(NV04_PGRAPH_CTX_USER, channel << 24);
	NV_WRITE(NV04_PGRAPH_FFINTFC_ST2, NV_READ(NV04_PGRAPH_FFINTFC_ST2)&0x000FFFFF);

	NV_WRITE(NV04_PGRAPH_FIFO,0x0);
	NV_WRITE(NV04_PFIFO_CACHE0_PULL0, 0x0);
	NV_WRITE(NV04_PFIFO_CACHE1_PULL0, 0x1);
	NV_WRITE(NV03_PFIFO_CACHES, 0x1);
	NV_WRITE(NV04_PGRAPH_FIFO,0x1);
}

int nv04_graph_context_create(drm_device_t *dev, int channel) {
	drm_nouveau_private_t *dev_priv = dev->dev_private;
	DRM_DEBUG("nv04_graph_context_create %d\n", channel);

	memset(dev_priv->fifos[channel].pgraph_ctx, 0, sizeof(dev_priv->fifos[channel].pgraph_ctx));

	//dev_priv->fifos[channel].pgraph_ctx_user = channel << 24;
	dev_priv->fifos[channel].pgraph_ctx[0] = 0x0001ffff;
	/* is it really needed ??? */
	//dev_priv->fifos[channel].pgraph_ctx[1] = NV_READ(NV_PGRAPH_DEBUG_4);
	//dev_priv->fifos[channel].pgraph_ctx[2] = NV_READ(0x004006b0);

	return 0;
}


int nv04_graph_init(drm_device_t *dev) {
	drm_nouveau_private_t *dev_priv = dev->dev_private;
	int i,sum=0;

	NV_WRITE(NV03_PMC_ENABLE, NV_READ(NV03_PMC_ENABLE) &
			~NV_PMC_ENABLE_PGRAPH);
	NV_WRITE(NV03_PMC_ENABLE, NV_READ(NV03_PMC_ENABLE) |
			 NV_PMC_ENABLE_PGRAPH);

	// check the context is big enough
	for ( i = 0 ; i<sizeof(nv04_graph_ctx_regs)/sizeof(nv04_graph_ctx_regs[0]); i++)
		sum+=nv04_graph_ctx_regs[i].number;
	if ( sum*4>sizeof(dev_priv->fifos[0].pgraph_ctx) )
		DRM_ERROR("pgraph_ctx too small\n");

	NV_WRITE(NV03_PGRAPH_INTR_EN, 0x00000000);
	NV_WRITE(NV03_PGRAPH_INTR   , 0xFFFFFFFF);

	NV_WRITE(NV04_PGRAPH_DEBUG_0, 0x000001FF);
	NV_WRITE(NV04_PGRAPH_DEBUG_0, 0x1230C000);
	NV_WRITE(NV04_PGRAPH_DEBUG_1, 0x72111101);
	NV_WRITE(NV04_PGRAPH_DEBUG_2, 0x11D5F071);
	NV_WRITE(NV04_PGRAPH_DEBUG_3, 0x0004FF31);
	NV_WRITE(NV04_PGRAPH_DEBUG_3, 0x4004FF31 |
				    (0x00D00000) |
				    (1<<29) |
				    (1<<31));

	NV_WRITE(NV04_PGRAPH_STATE        , 0xFFFFFFFF);
	NV_WRITE(NV04_PGRAPH_CTX_CONTROL  , 0x10010100);
	NV_WRITE(NV04_PGRAPH_FIFO         , 0x00000001);

	/* These don't belong here, they're part of a per-channel context */
	NV_WRITE(NV04_PGRAPH_PATTERN_SHAPE, 0x00000000);
	NV_WRITE(NV04_PGRAPH_BETA_AND     , 0xFFFFFFFF);

	return 0;
}

void nv04_graph_takedown(drm_device_t *dev)
{
}

