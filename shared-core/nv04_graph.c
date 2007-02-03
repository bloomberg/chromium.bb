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
	int reg;
	int number;
} nv04_graph_ctx_regs [] = {
	{NV04_PGRAPH_CTX_SWITCH1,	1},
	{NV04_PGRAPH_CTX_SWITCH2,	1},
	{NV04_PGRAPH_CTX_SWITCH3,	1},
	{NV04_PGRAPH_CTX_SWITCH4,	1},
	{NV04_PGRAPH_CTX_USER,		1},
	{NV04_PGRAPH_CTX_CACHE1,	8},
	{NV04_PGRAPH_CTX_CACHE2,	8},
	{NV04_PGRAPH_CTX_CACHE3,	8},
	{NV04_PGRAPH_CTX_CACHE4,	8},
	{NV03_PGRAPH_ABS_X_RAM,		32},
	{NV03_PGRAPH_ABS_Y_RAM,		32},
	{NV03_PGRAPH_X_MISC,		1},
	{NV03_PGRAPH_Y_MISC,		1},
	{NV04_PGRAPH_VALID1,		1},
	{NV04_PGRAPH_SOURCE_COLOR,	1},
	{NV04_PGRAPH_MISC24_0,		1},
	{NV03_PGRAPH_XY_LOGIC_MISC0,	1},
	{NV03_PGRAPH_XY_LOGIC_MISC1,	1},
	{NV03_PGRAPH_XY_LOGIC_MISC2,	1},
	{NV03_PGRAPH_XY_LOGIC_MISC3,	1},
	{NV03_PGRAPH_CLIPX_0,		1},
	{NV03_PGRAPH_CLIPX_1,		1},
	{NV03_PGRAPH_CLIPY_0,		1},
	{NV03_PGRAPH_CLIPY_1,		1},
	{NV03_PGRAPH_ABS_ICLIP_XMAX,	1},
	{NV03_PGRAPH_ABS_ICLIP_YMAX,	1},
	{NV03_PGRAPH_ABS_UCLIP_XMIN,	1},
	{NV03_PGRAPH_ABS_UCLIP_YMIN,	1},
	{NV03_PGRAPH_ABS_UCLIP_XMAX,	1},
	{NV03_PGRAPH_ABS_UCLIP_YMAX,	1},
	{NV03_PGRAPH_ABS_UCLIPA_XMIN,	1},
	{NV03_PGRAPH_ABS_UCLIPA_YMIN,	1},
	{NV03_PGRAPH_ABS_UCLIPA_XMAX,	1},
	{NV03_PGRAPH_ABS_UCLIPA_YMAX,	1},
	{NV04_PGRAPH_MISC24_1,		1},
	{NV04_PGRAPH_MISC24_2,		1},
	{NV04_PGRAPH_VALID2,		1},
	{NV04_PGRAPH_PASSTHRU_0,	1},
	{NV04_PGRAPH_PASSTHRU_1,	1},
	{NV04_PGRAPH_PASSTHRU_2,	1},
	{NV04_PGRAPH_COMBINE_0_ALPHA,	1},
	{NV04_PGRAPH_COMBINE_0_COLOR,	1},
	{NV04_PGRAPH_COMBINE_1_ALPHA,	1},
	{NV04_PGRAPH_COMBINE_1_COLOR,	1},
	// texture state
	{NV04_PGRAPH_FORMAT_0,		1},
	{NV04_PGRAPH_FORMAT_1,		1},
	{NV04_PGRAPH_FILTER_0,		1},
	{NV04_PGRAPH_FILTER_1,		1},
	// vertex state
	{0x004005c0,			1},
	{0x004005c4,			1},
	{0x004005c8,			1},
	{0x004005cc,			1},
	{0x004005d0,			1},
	{0x004005d4,			1},
	{0x004005d8,			1},
	{0x004005dc,			1},
	{0x004005e0,			1},
	{NV03_PGRAPH_MONO_COLOR0,	1},
	{NV04_PGRAPH_ROP3,		1},
	{NV04_PGRAPH_BETA_AND,		1},
	{NV04_PGRAPH_BETA_PREMULT,	1},
	{NV04_PGRAPH_FORMATS,		1},
	{NV04_PGRAPH_BOFFSET0,		6},
	{NV04_PGRAPH_BBASE0,		6},
	{NV04_PGRAPH_BPITCH0,		5},
	{NV04_PGRAPH_BLIMIT0,		6},
	{NV04_PGRAPH_BSWIZZLE2,		1},
	{NV04_PGRAPH_BSWIZZLE5,		1},
	{NV04_PGRAPH_SURFACE,		1},
	{NV04_PGRAPH_STATE,		1},
	{NV04_PGRAPH_NOTIFY,		1},
	{NV04_PGRAPH_BPIXEL,		1},
	{NV04_PGRAPH_DMA_PITCH,		1},
	{NV04_PGRAPH_DVD_COLORFMT,	1},
	{NV04_PGRAPH_SCALED_FORMAT,	1},
	{NV04_PGRAPH_PATT_COLOR0,	1},
	{NV04_PGRAPH_PATT_COLOR1,	1},
	{NV04_PGRAPH_PATTERN,		2},
	{NV04_PGRAPH_PATTERN_SHAPE,	1},
	{NV04_PGRAPH_CHROMA,		1},
	{NV04_PGRAPH_CONTROL0,		1},
	{NV04_PGRAPH_CONTROL1,		1},
	{NV04_PGRAPH_CONTROL2,		1},
	{NV04_PGRAPH_BLEND,		1},
	{NV04_PGRAPH_STORED_FMT,	1},
	{NV04_PGRAPH_PATT_COLORRAM,	64},
	{NV04_PGRAPH_U_RAM,		16},
	{NV04_PGRAPH_V_RAM,		16},
	{NV04_PGRAPH_W_RAM,		16},
	{NV04_PGRAPH_DMA_START_0,	1},
	{NV04_PGRAPH_DMA_START_1,	1},
	{NV04_PGRAPH_DMA_LENGTH,	1},
	{NV04_PGRAPH_DMA_MISC,		1},
	{NV04_PGRAPH_DMA_DATA_0,	1},
	{NV04_PGRAPH_DMA_DATA_1,	1},
	{NV04_PGRAPH_DMA_RM,		1},
	{NV04_PGRAPH_DMA_A_XLATE_INST,	1},
	{NV04_PGRAPH_DMA_A_CONTROL,	1},
	{NV04_PGRAPH_DMA_A_LIMIT,	1},
	{NV04_PGRAPH_DMA_A_TLB_PTE,	1},
	{NV04_PGRAPH_DMA_A_TLB_TAG,	1},
	{NV04_PGRAPH_DMA_A_ADJ_OFFSET,	1},
	{NV04_PGRAPH_DMA_A_OFFSET,	1},
	{NV04_PGRAPH_DMA_A_SIZE,	1},
	{NV04_PGRAPH_DMA_A_Y_SIZE,	1},
	{NV04_PGRAPH_DMA_B_XLATE_INST,	1},
	{NV04_PGRAPH_DMA_B_CONTROL,	1},
	{NV04_PGRAPH_DMA_B_LIMIT,	1},
	{NV04_PGRAPH_DMA_B_TLB_PTE,	1},
	{NV04_PGRAPH_DMA_B_TLB_TAG,	1},
	{NV04_PGRAPH_DMA_B_ADJ_OFFSET,	1},
	{NV04_PGRAPH_DMA_B_OFFSET,	1},
	{NV04_PGRAPH_DMA_B_SIZE,	1},
	{NV04_PGRAPH_DMA_B_Y_SIZE,	1},
};

void nouveau_nv04_context_switch(drm_device_t *dev)
{
	drm_nouveau_private_t *dev_priv = dev->dev_private;
	int channel, channel_old, i, j, index;

	channel=NV_READ(NV03_PFIFO_CACHE1_PUSH1)&(nouveau_fifo_number(dev)-1);
	channel_old = (NV_READ(NV04_PGRAPH_CTX_USER) >> 24) & (nouveau_fifo_number(dev)-1);

	DRM_INFO("NV: PGRAPH context switch interrupt channel %x -> %x\n",channel_old, channel);

	NV_WRITE(NV04_PGRAPH_FIFO,0x0);
#if 0
	NV_WRITE(NV_PFIFO_CACH1_PUL0, 0x00000000);
	NV_WRITE(NV_PFIFO_CACH1_PUL1, 0x00000000);
	NV_WRITE(NV_PFIFO_CACHES, 0x00000000);
#endif

	// save PGRAPH context
	index=0;
	for (i = 0; i<sizeof(nv04_graph_ctx_regs)/sizeof(nv04_graph_ctx_regs[0]); i++)
		for (j = 0; j<nv04_graph_ctx_regs[i].number; j++)
		{
			dev_priv->fifos[channel_old].pgraph_ctx[index] = NV_READ(nv04_graph_ctx_regs[i].reg+j*4);
			index++;
		}

	nouveau_wait_for_idle(dev);

	NV_WRITE(NV03_PGRAPH_CTX_CONTROL, 0x10000000);
	NV_WRITE(NV04_PGRAPH_CTX_USER, (NV_READ(NV04_PGRAPH_CTX_USER) & 0xffffff) | (0x1f << 24));

	nouveau_wait_for_idle(dev);
	// restore PGRAPH context
	//XXX not working yet
#if 1
	index=0;
	for (i = 0; i<sizeof(nv04_graph_ctx_regs)/sizeof(nv04_graph_ctx_regs[0]); i++)
		for (j = 0; j<nv04_graph_ctx_regs[i].number; j++)
		{
			NV_WRITE(nv04_graph_ctx_regs[i].reg+j*4, dev_priv->fifos[channel].pgraph_ctx[index]);
			index++;
		}
	nouveau_wait_for_idle(dev);
#endif

	NV_WRITE(NV03_PGRAPH_CTX_CONTROL, 0x10010100);
	NV_WRITE(NV04_PGRAPH_CTX_USER, channel << 24);
	NV_WRITE(NV04_PGRAPH_FFINTFC_ST2, NV_READ(NV04_PGRAPH_FFINTFC_ST2)&0xCFFFFFFF);

#if 0
	NV_WRITE(NV_PFIFO_CACH1_PUL0, 0x00000001);
	NV_WRITE(NV_PFIFO_CACH1_PUL1, 0x00000001);
	NV_WRITE(NV_PFIFO_CACHES, 0x00000001);
#endif
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

	// check the context is big enough
	int i,sum=0;
	for ( i = 0 ; i<sizeof(nv04_graph_ctx_regs)/sizeof(nv04_graph_ctx_regs[0]); i++)
		sum+=nv04_graph_ctx_regs[i].number;
	if ( sum*4>sizeof(dev_priv->fifos[0].pgraph_ctx) )
		DRM_ERROR();
	return 0;
}

