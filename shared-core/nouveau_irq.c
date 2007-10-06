/*
 * Copyright (C) 2006 Ben Skeggs.
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

/*
 * Authors:
 *   Ben Skeggs <darktama@iinet.net.au>
 */

#include "drmP.h"
#include "drm.h"
#include "nouveau_drm.h"
#include "nouveau_drv.h"
#include "nouveau_reg.h"

void nouveau_irq_preinstall(struct drm_device *dev)
{
	struct drm_nouveau_private *dev_priv = dev->dev_private;

	/* Master disable */
	NV_WRITE(NV03_PMC_INTR_EN_0, 0);
}

void nouveau_irq_postinstall(struct drm_device *dev)
{
	struct drm_nouveau_private *dev_priv = dev->dev_private;

	/* Master enable */
	NV_WRITE(NV03_PMC_INTR_EN_0, NV_PMC_INTR_EN_0_MASTER_ENABLE);
}

void nouveau_irq_uninstall(struct drm_device *dev)
{
	struct drm_nouveau_private *dev_priv = dev->dev_private;

	/* Master disable */
	NV_WRITE(NV03_PMC_INTR_EN_0, 0);
}

static void nouveau_fifo_irq_handler(struct drm_device *dev)
{
	uint32_t status, chmode, chstat, channel;
	struct drm_nouveau_private *dev_priv = dev->dev_private;

	status = NV_READ(NV03_PFIFO_INTR_0);
	if (!status)
		return;
	chmode = NV_READ(NV04_PFIFO_MODE);
	chstat = NV_READ(NV04_PFIFO_DMA);
	channel=NV_READ(NV03_PFIFO_CACHE1_PUSH1)&(nouveau_fifo_number(dev)-1);

	if (status & NV_PFIFO_INTR_CACHE_ERROR) {
		uint32_t c1get, c1method, c1data;

		DRM_ERROR("PFIFO error interrupt\n");

		c1get = NV_READ(NV03_PFIFO_CACHE1_GET) >> 2;
		if (dev_priv->card_type < NV_40) {
			/* Untested, so it may not work.. */
			c1method = NV_READ(NV04_PFIFO_CACHE1_METHOD(c1get));
			c1data   = NV_READ(NV04_PFIFO_CACHE1_DATA(c1get));
		} else {
			c1method = NV_READ(NV40_PFIFO_CACHE1_METHOD(c1get));
			c1data   = NV_READ(NV40_PFIFO_CACHE1_DATA(c1get));
		}

		DRM_ERROR("Channel %d/%d - Method 0x%04x, Data 0x%08x\n",
			  channel, (c1method >> 13) & 7, c1method & 0x1ffc,
			  c1data);

		status &= ~NV_PFIFO_INTR_CACHE_ERROR;
		NV_WRITE(NV03_PFIFO_INTR_0, NV_PFIFO_INTR_CACHE_ERROR);
	}

	if (status & NV_PFIFO_INTR_DMA_PUSHER) {
		DRM_ERROR("PFIFO DMA pusher interrupt: ch%d, 0x%08x\n",
			  channel, NV_READ(NV04_PFIFO_CACHE1_DMA_GET));

		status &= ~NV_PFIFO_INTR_DMA_PUSHER;
		NV_WRITE(NV03_PFIFO_INTR_0, NV_PFIFO_INTR_DMA_PUSHER);

		NV_WRITE(NV04_PFIFO_CACHE1_DMA_STATE, 0x00000000);
		if (NV_READ(NV04_PFIFO_CACHE1_DMA_PUT)!=NV_READ(NV04_PFIFO_CACHE1_DMA_GET))
		{
			uint32_t getval=NV_READ(NV04_PFIFO_CACHE1_DMA_GET)+4;
			NV_WRITE(NV04_PFIFO_CACHE1_DMA_GET,getval);
		}
	}

	if (status) {
		DRM_ERROR("Unhandled PFIFO interrupt: status=0x%08x\n", status);

		NV_WRITE(NV03_PFIFO_INTR_0, status);
	}

	NV_WRITE(NV03_PMC_INTR_0, NV_PMC_INTR_0_PFIFO_PENDING);
}

#if 0
static void nouveau_nv04_context_switch(struct drm_device *dev)
{
	struct drm_nouveau_private *dev_priv = dev->dev_private;
	uint32_t channel,i;
	uint32_t max=0;
	NV_WRITE(NV04_PGRAPH_FIFO,0x0);
	channel=NV_READ(NV03_PFIFO_CACHE1_PUSH1)&(nouveau_fifo_number(dev)-1);
	//DRM_INFO("raw PFIFO_CACH1_PHS1 reg is %x\n",NV_READ(NV03_PFIFO_CACHE1_PUSH1));
	//DRM_INFO("currently on channel %d\n",channel);
	for (i=0;i<nouveau_fifo_number(dev);i++)
		if ((dev_priv->fifos[i].used)&&(i!=channel)) {
			uint32_t put,get,pending;
			//put=NV_READ(dev_priv->ramfc_offset+i*32);
			//get=NV_READ(dev_priv->ramfc_offset+4+i*32);
			put=NV_READ(NV03_FIFO_REGS_DMAPUT(i));
			get=NV_READ(NV03_FIFO_REGS_DMAGET(i));
			pending=NV_READ(NV04_PFIFO_DMA);
			//DRM_INFO("Channel %d (put/get %x/%x)\n",i,put,get);
			/* mark all pending channels as such */
			if ((put!=get)&!(pending&(1<<i)))
			{
				pending|=(1<<i);
				NV_WRITE(NV04_PFIFO_DMA,pending);
			}
			max++;
		}
	nouveau_wait_for_idle(dev);

#if 1
	/* 2-channel commute */
	//		NV_WRITE(NV03_PFIFO_CACHE1_PUSH1,channel|0x100);
	if (channel==0)
		channel=1;
	else
		channel=0;
	//		dev_priv->cur_fifo=channel;
	NV_WRITE(NV04_PFIFO_NEXT_CHANNEL,channel|0x100);
#endif
	//NV_WRITE(NV03_PFIFO_CACHE1_PUSH1,max|0x100);
	//NV_WRITE(0x2050,max|0x100);

	NV_WRITE(NV04_PGRAPH_FIFO,0x1);
	
}
#endif


struct nouveau_bitfield_names
{
	uint32_t mask;
	const char * name;
};

static struct nouveau_bitfield_names nouveau_nstatus_names[] =
{
	{ NV03_PGRAPH_NSTATUS_STATE_IN_USE,       "STATE_IN_USE" },
	{ NV03_PGRAPH_NSTATUS_INVALID_STATE,      "INVALID_STATE" },
	{ NV03_PGRAPH_NSTATUS_BAD_ARGUMENT,       "BAD_ARGUMENT" },
	{ NV03_PGRAPH_NSTATUS_PROTECTION_FAULT,   "PROTECTION_FAULT" }
};

static struct nouveau_bitfield_names nouveau_nsource_names[] =
{
	{ NV03_PGRAPH_NSOURCE_NOTIFICATION,       "NOTIFICATION" },
	{ NV03_PGRAPH_NSOURCE_DATA_ERROR,         "DATA_ERROR" },
	{ NV03_PGRAPH_NSOURCE_PROTECTION_ERROR,   "PROTECTION_ERROR" },
	{ NV03_PGRAPH_NSOURCE_RANGE_EXCEPTION,    "RANGE_EXCEPTION" },
	{ NV03_PGRAPH_NSOURCE_LIMIT_COLOR,        "LIMIT_COLOR" },
	{ NV03_PGRAPH_NSOURCE_LIMIT_ZETA,         "LIMIT_ZETA" },
	{ NV03_PGRAPH_NSOURCE_ILLEGAL_MTHD,       "ILLEGAL_MTHD" },
	{ NV03_PGRAPH_NSOURCE_DMA_R_PROTECTION,   "DMA_R_PROTECTION" },
	{ NV03_PGRAPH_NSOURCE_DMA_W_PROTECTION,   "DMA_W_PROTECTION" },
	{ NV03_PGRAPH_NSOURCE_FORMAT_EXCEPTION,   "FORMAT_EXCEPTION" },
	{ NV03_PGRAPH_NSOURCE_PATCH_EXCEPTION,    "PATCH_EXCEPTION" },
	{ NV03_PGRAPH_NSOURCE_STATE_INVALID,      "STATE_INVALID" },
	{ NV03_PGRAPH_NSOURCE_DOUBLE_NOTIFY,      "DOUBLE_NOTIFY" },
	{ NV03_PGRAPH_NSOURCE_NOTIFY_IN_USE,      "NOTIFY_IN_USE" },
	{ NV03_PGRAPH_NSOURCE_METHOD_CNT,         "METHOD_CNT" },
	{ NV03_PGRAPH_NSOURCE_BFR_NOTIFICATION,   "BFR_NOTIFICATION" },
	{ NV03_PGRAPH_NSOURCE_DMA_VTX_PROTECTION, "DMA_VTX_PROTECTION" },
	{ NV03_PGRAPH_NSOURCE_DMA_WIDTH_A,        "DMA_WIDTH_A" },
	{ NV03_PGRAPH_NSOURCE_DMA_WIDTH_B,        "DMA_WIDTH_B" },
};

static void
nouveau_print_bitfield_names(uint32_t value,
                             const struct nouveau_bitfield_names *namelist,
                             const int namelist_len)
{
	int i;
	for(i=0; i<namelist_len; ++i) {
		uint32_t mask = namelist[i].mask;
		if(value & mask) {
			printk(" %s", namelist[i].name);
			value &= ~mask;
		}
	}
	if(value)
		printk(" (unknown bits 0x%08x)", value);
}

static int
nouveau_graph_trapped_channel(struct drm_device *dev, int *channel_ret)
{
	struct drm_nouveau_private *dev_priv = dev->dev_private;
	int channel;

	if (dev_priv->card_type < NV_10) {
		channel = (NV_READ(NV04_PGRAPH_TRAPPED_ADDR) >> 24) & 0xf;
	} else if (dev_priv->card_type < NV_40) {
		channel = (NV_READ(NV04_PGRAPH_TRAPPED_ADDR) >> 20) & 0x1f;
	} else
	if (dev_priv->card_type < NV_50) {
		uint32_t cur_grctx = (NV_READ(0x40032C) & 0xfffff) << 4;

		/* 0x400704 *sometimes* contains a sensible channel ID, but
		 * mostly not.. for now lookup which channel owns the active
		 * PGRAPH context.  Probably a better way, but this'll do
		 * for now.
		 */
		for (channel = 0; channel < 32; channel++) {
			if (dev_priv->fifos[channel] == NULL)
				continue;
			if (cur_grctx ==
			    dev_priv->fifos[channel]->ramin_grctx->instance)
				break;
		}
		if (channel == 32) {
			DRM_ERROR("AIII, unable to determine active channel "
				  "from PGRAPH context 0x%08x\n", cur_grctx);
			return -EINVAL;
		}
	} else {
		uint32_t cur_grctx = (NV_READ(0x40032C) & 0xfffff) << 12;

		for (channel = 0; channel < 128; channel++) {
			if (dev_priv->fifos[channel] == NULL)
				continue;
			if (cur_grctx ==
			    dev_priv->fifos[channel]->ramin_grctx->instance)
				break;
		}
		if (channel == 128) {
			DRM_ERROR("AIII, unable to determine active channel "
				  "from PGRAPH context 0x%08x\n", cur_grctx);
			return -EINVAL;
		}
	}

	if (channel > nouveau_fifo_number(dev) ||
	    dev_priv->fifos[channel] == NULL) {
		DRM_ERROR("AIII, invalid/inactive channel id %d\n", channel);
		return -EINVAL;
	}

	*channel_ret = channel;
	return 0;
}

static void
nouveau_graph_dump_trap_info(struct drm_device *dev)
{
	struct drm_nouveau_private *dev_priv = dev->dev_private;
	uint32_t address;
	uint32_t channel, class;
	uint32_t method, subc, data, data2;
	uint32_t nsource, nstatus;

	if (nouveau_graph_trapped_channel(dev, &channel))
		channel = -1;

	data    = NV_READ(NV04_PGRAPH_TRAPPED_DATA);
	address = NV_READ(NV04_PGRAPH_TRAPPED_ADDR);
	method  = address & 0x1FFC;
	if (dev_priv->card_type < NV_10) {
		subc = (address >> 13) & 0x7;
		data2= 0;
	} else {
		subc = (address >> 16) & 0x7;
		data2= NV_READ(NV10_PGRAPH_TRAPPED_DATA_HIGH);
	}
	nsource = NV_READ(NV03_PGRAPH_NSOURCE);
	nstatus = NV_READ(NV03_PGRAPH_NSTATUS);
	if (dev_priv->card_type < NV_10) {
		class = NV_READ(0x400180 + subc*4) & 0xFF;
	} else if (dev_priv->card_type < NV_40) {
		class = NV_READ(0x400160 + subc*4) & 0xFFF;
	} else if (dev_priv->card_type < NV_50) {
		class = NV_READ(0x400160 + subc*4) & 0xFFFF;
	} else {
		class = NV_READ(0x400814);
	}

	DRM_ERROR("nSource:");
	nouveau_print_bitfield_names(nsource, nouveau_nsource_names,
	                             ARRAY_SIZE(nouveau_nsource_names));
	printk(", nStatus:");
	nouveau_print_bitfield_names(nstatus, nouveau_nstatus_names,
	                             ARRAY_SIZE(nouveau_nstatus_names));
	printk("\n");

	DRM_ERROR("Channel %d/%d (class 0x%04x) - Method 0x%04x, Data 0x%08x:0x%08x\n",
		  channel, subc, class, method, data2, data);
}

static void nouveau_pgraph_irq_handler(struct drm_device *dev)
{
	struct drm_nouveau_private *dev_priv = dev->dev_private;
	uint32_t status, nsource;

	status = NV_READ(NV03_PGRAPH_INTR);
	if (!status)
		return;
	nsource = NV_READ(NV03_PGRAPH_NSOURCE);

	if (status & NV_PGRAPH_INTR_NOTIFY) {
		DRM_DEBUG("PGRAPH notify interrupt\n");

		nouveau_graph_dump_trap_info(dev);

		status &= ~NV_PGRAPH_INTR_NOTIFY;
		NV_WRITE(NV03_PGRAPH_INTR, NV_PGRAPH_INTR_NOTIFY);
	}

	if (status & NV_PGRAPH_INTR_ERROR) {
		DRM_ERROR("PGRAPH error interrupt\n");

		nouveau_graph_dump_trap_info(dev);

		status &= ~NV_PGRAPH_INTR_ERROR;
		NV_WRITE(NV03_PGRAPH_INTR, NV_PGRAPH_INTR_ERROR);
	}

	if (status & NV_PGRAPH_INTR_CONTEXT_SWITCH) {
		uint32_t channel=NV_READ(NV03_PFIFO_CACHE1_PUSH1)&(nouveau_fifo_number(dev)-1);
		DRM_DEBUG("PGRAPH context switch interrupt channel %x\n",channel);
		switch(dev_priv->card_type)
		{
			case NV_04:
			case NV_05:
				nouveau_nv04_context_switch(dev);
				break;
			case NV_10:
			case NV_11:
			case NV_17:
				nouveau_nv10_context_switch(dev);
				break;
			default:
				DRM_ERROR("Context switch not implemented\n");
				break;
		}

		status &= ~NV_PGRAPH_INTR_CONTEXT_SWITCH;
		NV_WRITE(NV03_PGRAPH_INTR, NV_PGRAPH_INTR_CONTEXT_SWITCH);
	}

	if (status) {
		DRM_ERROR("Unhandled PGRAPH interrupt: STAT=0x%08x\n", status);
		NV_WRITE(NV03_PGRAPH_INTR, status);
	}

	NV_WRITE(NV03_PMC_INTR_0, NV_PMC_INTR_0_PGRAPH_PENDING);
}

static void nouveau_crtc_irq_handler(struct drm_device *dev, int crtc)
{
	struct drm_nouveau_private *dev_priv = dev->dev_private;

	if (crtc&1) {
		NV_WRITE(NV_CRTC0_INTSTAT, NV_CRTC_INTR_VBLANK);
	}

	if (crtc&2) {
		NV_WRITE(NV_CRTC1_INTSTAT, NV_CRTC_INTR_VBLANK);
	}
}

irqreturn_t nouveau_irq_handler(DRM_IRQ_ARGS)
{
	struct drm_device *dev = (struct drm_device*)arg;
	struct drm_nouveau_private *dev_priv = dev->dev_private;
	uint32_t status;

	status = NV_READ(NV03_PMC_INTR_0);
	if (!status)
		return IRQ_NONE;

	if (status & NV_PMC_INTR_0_PFIFO_PENDING) {
		nouveau_fifo_irq_handler(dev);
		status &= ~NV_PMC_INTR_0_PFIFO_PENDING;
	}

	if (status & NV_PMC_INTR_0_PGRAPH_PENDING) {
		nouveau_pgraph_irq_handler(dev);
		status &= ~NV_PMC_INTR_0_PGRAPH_PENDING;
	}

	if (status & NV_PMC_INTR_0_CRTCn_PENDING) {
		nouveau_crtc_irq_handler(dev, (status>>24)&3);
		status &= ~NV_PMC_INTR_0_CRTCn_PENDING;
	}

	if (status)
		DRM_ERROR("Unhandled PMC INTR status bits 0x%08x\n", status);

	return IRQ_HANDLED;
}

