/*
 * Copyright (C) 2007 Ben Skeggs.
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

#define IS_G80 ((dev_priv->chipset & 0xf0) == 0x50)

static void
nv50_graph_init_reset(struct drm_device *dev)
{
	struct drm_nouveau_private *dev_priv = dev->dev_private;
	uint32_t pmc_e = NV_PMC_ENABLE_PGRAPH | (1 << 21);

	DRM_DEBUG("\n");

	NV_WRITE(NV03_PMC_ENABLE, NV_READ(NV03_PMC_ENABLE) & ~pmc_e);
	NV_WRITE(NV03_PMC_ENABLE, NV_READ(NV03_PMC_ENABLE) |  pmc_e);
}

static void
nv50_graph_init_intr(struct drm_device *dev)
{
	struct drm_nouveau_private *dev_priv = dev->dev_private;

	DRM_DEBUG("\n");
	NV_WRITE(NV03_PGRAPH_INTR, 0xffffffff);
	NV_WRITE(NV40_PGRAPH_INTR_EN, 0xffffffff);
}

static void
nv50_graph_init_regs__nv(struct drm_device *dev)
{
	struct drm_nouveau_private *dev_priv = dev->dev_private;

	DRM_DEBUG("\n");

	NV_WRITE(0x400804, 0xc0000000);
	NV_WRITE(0x406800, 0xc0000000);
	NV_WRITE(0x400c04, 0xc0000000);
	NV_WRITE(0x401804, 0xc0000000);
	NV_WRITE(0x405018, 0xc0000000);
	NV_WRITE(0x402000, 0xc0000000);

	NV_WRITE(0x400108, 0xffffffff);

	NV_WRITE(0x400824, 0x00004000);
	NV_WRITE(0x400500, 0x00010001);
}

static void
nv50_graph_init_regs(struct drm_device *dev)
{
	struct drm_nouveau_private *dev_priv = dev->dev_private;

	DRM_DEBUG("\n");

	NV_WRITE(NV04_PGRAPH_DEBUG_3, (1<<2) /* HW_CONTEXT_SWITCH_ENABLED */);
}

static uint32_t nv84_ctx_voodoo[] = {
	0x0070008e, 0x0070009c, 0x00200020, 0x00600008, 0x0050004c, 0x00400e89,
	0x00200000, 0x00600007, 0x00300000, 0x00c000ff, 0x00200000, 0x008000ff,
	0x00700009, 0x0041634d, 0x00402944, 0x00402905, 0x0040290d, 0x00413e06,
	0x00600005, 0x004015c5, 0x00600011, 0x0040270b, 0x004021c5, 0x00700000,
	0x00700081, 0x00600004, 0x0050004a, 0x00216f40, 0x00600007, 0x00c02801,
	0x0020002e, 0x00800001, 0x005000cb, 0x0090ffff, 0x0091ffff, 0x00200020,
	0x00600008, 0x0050004c, 0x00600009, 0x00413e45, 0x0041594d, 0x0070009d,
	0x00402dcf, 0x0070009f, 0x0050009f, 0x00402ac0, 0x00200200, 0x00600008,
	0x00402a4f, 0x00402ac0, 0x004030cc, 0x00700081, 0x00200000, 0x00600006,
	0x00700000, 0x00111bfc, 0x00700083, 0x00300000, 0x00216f40, 0x00600007,
	0x00c00b01, 0x0020001e, 0x00800001, 0x005000cb, 0x00c000ff, 0x00700080,
	0x00700083, 0x00200047, 0x00600006, 0x0011020a, 0x00200480, 0x00600007,
	0x00300000, 0x00c000ff, 0x00c800ff, 0x00414907, 0x00202916, 0x008000ff,
	0x0040508c, 0x005000cb, 0x00a0023f, 0x00200040, 0x00600006, 0x0070000f,
	0x00170202, 0x0011020a, 0x00200032, 0x0010020d, 0x001c0242, 0x00120302,
	0x00140402, 0x00180500, 0x00130509, 0x00150550, 0x00110605, 0x0020000f,
	0x00100607, 0x00110700, 0x00110900, 0x00120902, 0x00110a00, 0x00160b02,
	0x00120b28, 0x00140b2b, 0x00110c01, 0x00111400, 0x00111405, 0x00111407,
	0x00111409, 0x0011140b, 0x002000cb, 0x00101500, 0x0040790f, 0x0040794b,
	0x00214d40, 0x00600007, 0x0020043e, 0x008800ff, 0x0070008f, 0x0040798c,
	0x005000cb, 0x00000000, 0x0020002b, 0x00101a05, 0x00131c00, 0x00121c04,
	0x00141c20, 0x00111c25, 0x00131c40, 0x00121c44, 0x00141c60, 0x00111c65,
	0x00131c80, 0x00121c84, 0x00141ca0, 0x00111ca5, 0x00131cc0, 0x00121cc4,
	0x00141ce0, 0x00111ce5, 0x00131f00, 0x00191f40, 0x0040a1e0, 0x002001ed,
	0x00600006, 0x00200044, 0x00102080, 0x001120c6, 0x001520c9, 0x001920d0,
	0x00122100, 0x00122103, 0x00162200, 0x00122207, 0x00112280, 0x00112300,
	0x00112302, 0x00122380, 0x0011238b, 0x00112394, 0x0011239c, 0x0040bee1,
	0x00200254, 0x00600006, 0x00200044, 0x00102480, 0x0040af0f, 0x0040af4b,
	0x00214d40, 0x00600007, 0x0020043e, 0x008800ff, 0x0070008f, 0x0040af8c,
	0x005000cb, 0x00000000, 0x001124c6, 0x001524c9, 0x001924d0, 0x00122500,
	0x00122503, 0x00162600, 0x00122607, 0x00112680, 0x00112700, 0x00112702,
	0x00122780, 0x0011278b, 0x00112794, 0x0011279c, 0x0040d1e2, 0x002002bb,
	0x00600006, 0x00200044, 0x00102880, 0x001128c6, 0x001528c9, 0x001928d0,
	0x00122900, 0x00122903, 0x00162a00, 0x00122a07, 0x00112a80, 0x00112b00,
	0x00112b02, 0x00122b80, 0x00112b8b, 0x00112b94, 0x00112b9c, 0x0040eee3,
	0x00200322, 0x00600006, 0x00200044, 0x00102c80, 0x0040df0f, 0x0040df4b,
	0x00214d40, 0x00600007, 0x0020043e, 0x008800ff, 0x0070008f, 0x0040df8c,
	0x005000cb, 0x00000000, 0x00112cc6, 0x00152cc9, 0x00192cd0, 0x00122d00,
	0x00122d03, 0x00162e00, 0x00122e07, 0x00112e80, 0x00112f00, 0x00112f02,
	0x00122f80, 0x00112f8b, 0x00112f94, 0x00112f9c, 0x004101e4, 0x00200389,
	0x00600006, 0x00200044, 0x00103080, 0x001130c6, 0x001530c9, 0x001930d0,
	0x00123100, 0x00123103, 0x00163200, 0x00123207, 0x00113280, 0x00113300,
	0x00113302, 0x00123380, 0x0011338b, 0x00113394, 0x0011339c, 0x00411ee5,
	0x002003f0, 0x00600006, 0x00200044, 0x00103480, 0x00410f0f, 0x00410f4b,
	0x00214d40, 0x00600007, 0x0020043e, 0x008800ff, 0x0070008f, 0x00410f8c,
	0x005000cb, 0x00000000, 0x001134c6, 0x001534c9, 0x001934d0, 0x00123500,
	0x00123503, 0x00163600, 0x00123607, 0x00113680, 0x00113700, 0x00113702,
	0x00123780, 0x0011378b, 0x00113794, 0x0011379c, 0x00000000, 0x0041250f,
	0x005000cb, 0x00214d40, 0x00600007, 0x0020043e, 0x008800ff, 0x005000cb,
	0x00412887, 0x0060000a, 0x00000000, 0x00413700, 0x007000a0, 0x00700080,
	0x00200480, 0x00600007, 0x00200004, 0x00c000ff, 0x008000ff, 0x005000cb,
	0x00700000, 0x00200000, 0x00600006, 0x00111bfe, 0x0041594d, 0x00700000,
	0x00200000, 0x00600006, 0x00111bfe, 0x00700080, 0x0070001d, 0x0040114d,
	0x00700081, 0x00600004, 0x0050004a, 0x00414388, 0x0060000b, 0x00200000,
	0x00600006, 0x00700000, 0x0041590b, 0x00111bfd, 0x0040424d, 0x00202916,
	0x008000fd, 0x005000cb, 0x00c00002, 0x00200480, 0x00600007, 0x00200160,
	0x00800002, 0x005000cb, 0x00c01802, 0x002027b6, 0x00800002, 0x005000cb,
	0x00404e4d, 0x0060000b, 0x0041574d, 0x00700001, 0x005000cf, 0x00700003,
	0x00415e06, 0x00415f05, 0x0060000d, 0x00700005, 0x0070000d, 0x00700006,
	0x0070000b, 0x0070000e, 0x0070001c, 0x0060000c, ~0
};
 
static uint32_t nv86_ctx_voodoo[] = {
	0x0070008e, 0x0070009c, 0x00200020, 0x00600008, 0x0050004c, 0x00400e89, 
	0x00200000, 0x00600007, 0x00300000, 0x00c000ff, 0x00200000, 0x008000ff, 
	0x00700009, 0x0040dd4d, 0x00402944, 0x00402905, 0x0040290d, 0x0040b906, 
	0x00600005, 0x004015c5, 0x00600011, 0x0040270b, 0x004021c5, 0x00700000, 
	0x00700081, 0x00600004, 0x0050004a, 0x00216d80, 0x00600007, 0x00c02801, 
	0x0020002e, 0x00800001, 0x005000cb, 0x0090ffff, 0x0091ffff, 0x00200020, 
	0x00600008, 0x0050004c, 0x00600009, 0x0040b945, 0x0040d44d, 0x0070009d, 
	0x00402dcf, 0x0070009f, 0x0050009f, 0x00402ac0, 0x00200200, 0x00600008, 
	0x00402a4f, 0x00402ac0, 0x004030cc, 0x00700081, 0x00200000, 0x00600006, 
	0x00700000, 0x00111bfc, 0x00700083, 0x00300000, 0x00216d80, 0x00600007, 
	0x00c00b01, 0x0020001e, 0x00800001, 0x005000cb, 0x00c000ff, 0x00700080, 
	0x00700083, 0x00200047, 0x00600006, 0x0011020a, 0x00200280, 0x00600007, 
	0x00300000, 0x00c000ff, 0x00c800ff, 0x0040c407, 0x00202916, 0x008000ff, 
	0x0040508c, 0x005000cb, 0x00a0023f, 0x00200040, 0x00600006, 0x0070000f, 
	0x00170202, 0x0011020a, 0x00200032, 0x0010020d, 0x001c0242, 0x00120302, 
	0x00140402, 0x00180500, 0x00130509, 0x00150550, 0x00110605, 0x0020000f, 
	0x00100607, 0x00110700, 0x00110900, 0x00120902, 0x00110a00, 0x00160b02, 
	0x00120b28, 0x00140b2b, 0x00110c01, 0x00111400, 0x00111405, 0x00111407, 
	0x00111409, 0x0011140b, 0x002000cb, 0x00101500, 0x0040790f, 0x0040794b, 
	0x00214b40, 0x00600007, 0x00200442, 0x008800ff, 0x0070008f, 0x0040798c, 
	0x005000cb, 0x00000000, 0x0020002b, 0x00101a05, 0x00131c00, 0x00121c04, 
	0x00141c20, 0x00111c25, 0x00131c40, 0x00121c44, 0x00141c60, 0x00111c65, 
	0x00131f00, 0x00191f40, 0x004099e0, 0x002001d9, 0x00600006, 0x00200044, 
	0x00102080, 0x001120c6, 0x001520c9, 0x001920d0, 0x00122100, 0x00122103, 
	0x00162200, 0x00122207, 0x00112280, 0x00112300, 0x00112302, 0x00122380, 
	0x0011238b, 0x00112394, 0x0011239c, 0x00000000, 0x0040a00f, 0x005000cb, 
	0x00214b40, 0x00600007, 0x00200442, 0x008800ff, 0x005000cb, 0x0040a387, 
	0x0060000a, 0x00000000, 0x0040b200, 0x007000a0, 0x00700080, 0x00200280, 
	0x00600007, 0x00200004, 0x00c000ff, 0x008000ff, 0x005000cb, 0x00700000, 
	0x00200000, 0x00600006, 0x00111bfe, 0x0040d44d, 0x00700000, 0x00200000, 
	0x00600006, 0x00111bfe, 0x00700080, 0x0070001d, 0x0040114d, 0x00700081, 
	0x00600004, 0x0050004a, 0x0040be88, 0x0060000b, 0x00200000, 0x00600006, 
	0x00700000, 0x0040d40b, 0x00111bfd, 0x0040424d, 0x00202916, 0x008000fd, 
	0x005000cb, 0x00c00002, 0x00200280, 0x00600007, 0x00200160, 0x00800002, 
	0x005000cb, 0x00c01802, 0x002027b6, 0x00800002, 0x005000cb, 0x00404e4d, 
	0x0060000b, 0x0040d24d, 0x00700001, 0x00700003, 0x0040d806, 0x0040d905, 
	0x0060000d, 0x00700005, 0x0070000d, 0x00700006, 0x0070000b, 0x0070000e, 
	0x0070001c, 0x0060000c, ~0
};

static void
nv50_graph_init_ctxctl(struct drm_device *dev)
{
	struct drm_nouveau_private *dev_priv = dev->dev_private;
	uint32_t *voodoo = NULL;

	DRM_DEBUG("\n");

	switch (dev_priv->chipset) {
	case 0x84:
		voodoo = nv84_ctx_voodoo;
		break;
	case 0x86:
		voodoo = nv86_ctx_voodoo;
		break;
	default:
		DRM_ERROR("no voodoo for chipset NV%02x\n", dev_priv->chipset);
		break;
	}

	if (voodoo) {
		NV_WRITE(NV40_PGRAPH_CTXCTL_UCODE_INDEX, 0);
		while (*voodoo != ~0) {
			NV_WRITE(NV40_PGRAPH_CTXCTL_UCODE_DATA, *voodoo);
			voodoo++;
		}
	}

	NV_WRITE(0x400320, 4);
	NV_WRITE(NV40_PGRAPH_CTXCTL_CUR, 0);
	NV_WRITE(NV20_PGRAPH_CHANNEL_CTX_POINTER, 0);
}

int
nv50_graph_init(struct drm_device *dev)
{
	DRM_DEBUG("\n");

	nv50_graph_init_reset(dev);
	nv50_graph_init_intr(dev);
	nv50_graph_init_regs__nv(dev);
	nv50_graph_init_regs(dev);
	nv50_graph_init_ctxctl(dev);

	return 0;
}

void
nv50_graph_takedown(struct drm_device *dev)
{
	DRM_DEBUG("\n");
}

int
nv50_graph_create_context(struct nouveau_channel *chan)
{
	struct drm_device *dev = chan->dev;
	struct drm_nouveau_private *dev_priv = dev->dev_private;
	struct nouveau_engine *engine = &dev_priv->Engine;
	struct nouveau_gpuobj *ramin = chan->ramin->gpuobj;
	int grctx_size = 0x60000, hdr;
	int ret;

	DRM_DEBUG("ch%d\n", chan->id);

	ret = nouveau_gpuobj_new_ref(dev, chan, NULL, 0, grctx_size, 0x1000,
				     NVOBJ_FLAG_ZERO_ALLOC |
				     NVOBJ_FLAG_ZERO_FREE, &chan->ramin_grctx);
	if (ret)
		return ret;

	hdr = IS_G80 ? 0x200 : 0x20;
	INSTANCE_WR(ramin, (hdr + 0x00)/4, 0x00190002);
	INSTANCE_WR(ramin, (hdr + 0x04)/4, chan->ramin_grctx->instance +
					   grctx_size - 1);
	INSTANCE_WR(ramin, (hdr + 0x08)/4, chan->ramin_grctx->instance);
	INSTANCE_WR(ramin, (hdr + 0x0c)/4, 0);
	INSTANCE_WR(ramin, (hdr + 0x10)/4, 0);
	INSTANCE_WR(ramin, (hdr + 0x14)/4, 0x00010000);

	ret = engine->graph.load_context(chan);
	if (ret) {
		DRM_ERROR("Error hacking up initial context: %d\n", ret);
		return ret;
	}

	INSTANCE_WR(chan->ramin_grctx->gpuobj, 0x00000/4,
		    chan->ramin->instance >> 12);
	INSTANCE_WR(chan->ramin_grctx->gpuobj, 0x0011c/4, 0x00000002);

	return 0;
}

void
nv50_graph_destroy_context(struct nouveau_channel *chan)
{
	struct drm_device *dev = chan->dev;
	struct drm_nouveau_private *dev_priv = dev->dev_private;
	int i, hdr;

	DRM_DEBUG("ch%d\n", chan->id);

	hdr = IS_G80 ? 0x200 : 0x20;
	for (i=hdr; i<hdr+24; i+=4)
		INSTANCE_WR(chan->ramin->gpuobj, i/4, 0);

	nouveau_gpuobj_ref_del(dev, &chan->ramin_grctx);
}

static int
nv50_graph_transfer_context(struct drm_device *dev, uint32_t inst, int save)
{
	struct drm_nouveau_private *dev_priv = dev->dev_private;
	uint32_t old_cp, tv = 20000;
	int i;

	DRM_DEBUG("inst=0x%08x, save=%d\n", inst, save);

	old_cp = NV_READ(NV20_PGRAPH_CHANNEL_CTX_POINTER);
	NV_WRITE(NV20_PGRAPH_CHANNEL_CTX_POINTER, inst | (1<<31));
	NV_WRITE(0x400824, NV_READ(0x400824) |
		 (save ? NV40_PGRAPH_CTXCTL_0310_XFER_SAVE :
			 NV40_PGRAPH_CTXCTL_0310_XFER_LOAD));
	NV_WRITE(NV40_PGRAPH_CTXCTL_0304, NV40_PGRAPH_CTXCTL_0304_XFER_CTX);

	for (i = 0; i < tv; i++) {
		if (NV_READ(NV40_PGRAPH_CTXCTL_030C) == 0)
			break;
	}
	NV_WRITE(NV20_PGRAPH_CHANNEL_CTX_POINTER, old_cp);

	if (i == tv) {
		DRM_ERROR("failed: inst=0x%08x save=%d\n", inst, save);
		DRM_ERROR("0x40030C = 0x%08x\n",
			  NV_READ(NV40_PGRAPH_CTXCTL_030C));
		return -EBUSY;
	}

	return 0;
}

int
nv50_graph_load_context(struct nouveau_channel *chan)
{
	struct drm_device *dev = chan->dev;
	struct drm_nouveau_private *dev_priv = dev->dev_private;
	uint32_t inst = ((chan->ramin->instance >> 12) | (1<<31));
	int ret; (void)ret;

	DRM_DEBUG("ch%d\n", chan->id);

#if 0
	if ((ret = nv50_graph_transfer_context(dev, inst, 0)))
		return ret;
#endif

	NV_WRITE(NV20_PGRAPH_CHANNEL_CTX_POINTER, inst);
	NV_WRITE(0x400320, 4);
	NV_WRITE(NV40_PGRAPH_CTXCTL_CUR, inst);

	return 0;
}

int
nv50_graph_save_context(struct nouveau_channel *chan)
{
	struct drm_device *dev = chan->dev;
	uint32_t inst = ((chan->ramin->instance >> 12) | (1<<31));

	DRM_DEBUG("ch%d\n", chan->id);

	return nv50_graph_transfer_context(dev, inst, 1);
}
