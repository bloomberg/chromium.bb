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

void nouveau_irq_preinstall(drm_device_t *dev)
{
	drm_nouveau_private_t *dev_priv = dev->dev_private;

	DRM_DEBUG("IRQ: preinst\n");

	if (!dev_priv) {
		DRM_ERROR("AIII, no dev_priv\n");
		return;
	}
	if (!dev_priv->mmio) {
		DRM_ERROR("AIII, no dev_priv->mmio\n");
		return;
	}

	/* Disable/Clear PFIFO interrupts */
	NV_WRITE(NV03_PFIFO_INTR_EN_0, 0);
	NV_WRITE(NV03_PFIFO_INTR_0, 0xFFFFFFFF);
	/* Disable/Clear PGRAPH interrupts */
	if (dev_priv->card_type<NV_40)
		NV_WRITE(NV03_PGRAPH_INTR_EN, 0);
	else
		NV_WRITE(NV40_PGRAPH_INTR_EN, 0);
	NV_WRITE(NV03_PGRAPH_INTR, 0xFFFFFFFF);
#if 0
	/* Disable/Clear CRTC0/1 interrupts */
	NV_WRITE(NV_CRTC0_INTEN, 0);
	NV_WRITE(NV_CRTC0_INTSTAT, NV_CRTC_INTR_VBLANK);
	NV_WRITE(NV_CRTC1_INTEN, 0);
	NV_WRITE(NV_CRTC1_INTSTAT, NV_CRTC_INTR_VBLANK);
#endif
	/* Master disable */
	NV_WRITE(NV03_PMC_INTR_EN_0, 0);
}

void nouveau_irq_postinstall(drm_device_t *dev)
{
	drm_nouveau_private_t *dev_priv = dev->dev_private;

	if (!dev_priv) {
		DRM_ERROR("AIII, no dev_priv\n");
		return;
	}
	if (!dev_priv->mmio) {
		DRM_ERROR("AIII, no dev_priv->mmio\n");
		return;
	}

	DRM_DEBUG("IRQ: postinst\n");

	/* Enable PFIFO error reporting */
	NV_WRITE(NV03_PFIFO_INTR_EN_0 , 
			NV_PFIFO_INTR_CACHE_ERROR |
			NV_PFIFO_INTR_RUNOUT |
			NV_PFIFO_INTR_RUNOUT_OVERFLOW |
			NV_PFIFO_INTR_DMA_PUSHER |
			NV_PFIFO_INTR_DMA_PT |
			NV_PFIFO_INTR_SEMAPHORE |
			NV_PFIFO_INTR_ACQUIRE_TIMEOUT
			);
	NV_WRITE(NV03_PFIFO_INTR_0, 0xFFFFFFFF);

	/* Enable PGRAPH interrupts */
	if (dev_priv->card_type<NV_40)
		NV_WRITE(NV03_PGRAPH_INTR_EN,
				NV_PGRAPH_INTR_NOTIFY |
				NV_PGRAPH_INTR_MISSING_HW |
				NV_PGRAPH_INTR_CONTEXT_SWITCH |
				NV_PGRAPH_INTR_BUFFER_NOTIFY |
				NV_PGRAPH_INTR_ERROR
				);
	else
		NV_WRITE(NV40_PGRAPH_INTR_EN,
				NV_PGRAPH_INTR_NOTIFY |
				NV_PGRAPH_INTR_MISSING_HW |
				NV_PGRAPH_INTR_CONTEXT_SWITCH |
				NV_PGRAPH_INTR_BUFFER_NOTIFY |
				NV_PGRAPH_INTR_ERROR
				);
	NV_WRITE(NV03_PGRAPH_INTR, 0xFFFFFFFF);

#if 0
	/* Enable CRTC0/1 interrupts */
	NV_WRITE(NV_CRTC0_INTEN, NV_CRTC_INTR_VBLANK);
	NV_WRITE(NV_CRTC1_INTEN, NV_CRTC_INTR_VBLANK);
#endif

	/* Master enable */
	NV_WRITE(NV03_PMC_INTR_EN_0, NV_PMC_INTR_EN_0_MASTER_ENABLE);
}

void nouveau_irq_uninstall(drm_device_t *dev)
{
	drm_nouveau_private_t *dev_priv = dev->dev_private;

	if (!dev_priv) {
		DRM_ERROR("AIII, no dev_priv\n");
		return;
	}
	if (!dev_priv->mmio) {
		DRM_ERROR("AIII, no dev_priv->mmio\n");
		return;
	}

	DRM_DEBUG("IRQ: uninst\n");

	/* Disable PFIFO interrupts */
	NV_WRITE(NV03_PFIFO_INTR_EN_0, 0);
	/* Disable PGRAPH interrupts */
	if (dev_priv->card_type<NV_40)
		NV_WRITE(NV03_PGRAPH_INTR_EN, 0);
	else
		NV_WRITE(NV40_PGRAPH_INTR_EN, 0);
#if 0
	/* Disable CRTC0/1 interrupts */
	NV_WRITE(NV_CRTC0_INTEN, 0);
	NV_WRITE(NV_CRTC1_INTEN, 0);
#endif
	/* Master disable */
	NV_WRITE(NV03_PMC_INTR_EN_0, 0);
}

static void nouveau_fifo_irq_handler(drm_device_t *dev)
{
	uint32_t status, chmode, chstat, channel;
	drm_nouveau_private_t *dev_priv = dev->dev_private;

	status = NV_READ(NV03_PFIFO_INTR_0);
	if (!status)
		return;
	chmode = NV_READ(NV04_PFIFO_MODE);
	chstat = NV_READ(NV04_PFIFO_DMA);
	channel=NV_READ(NV03_PFIFO_CACHE1_PUSH1)&(nouveau_fifo_number(dev)-1);

	DRM_DEBUG("NV: PFIFO interrupt! Channel=%d, INTSTAT=0x%08x/MODE=0x%08x/PEND=0x%08x\n", channel, status, chmode, chstat);

	if (status & NV_PFIFO_INTR_CACHE_ERROR) {
		uint32_t c1get, c1method, c1data;

		DRM_ERROR("NV: PFIFO error interrupt\n");

		c1get = NV_READ(NV03_PFIFO_CACHE1_GET) >> 2;
		if (dev_priv->card_type < NV_40) {
			/* Untested, so it may not work.. */
			c1method = NV_READ(NV04_PFIFO_CACHE1_METHOD(c1get));
			c1data   = NV_READ(NV04_PFIFO_CACHE1_DATA(c1get));
		} else {
			c1method = NV_READ(NV40_PFIFO_CACHE1_METHOD(c1get));
			c1data   = NV_READ(NV40_PFIFO_CACHE1_DATA(c1get));
		}

		DRM_ERROR("NV: Channel %d/%d - Method 0x%04x, Data 0x%08x\n",
				channel, (c1method >> 13) & 7,
				c1method & 0x1ffc, c1data
			 );

		status &= ~NV_PFIFO_INTR_CACHE_ERROR;
		NV_WRITE(NV03_PFIFO_INTR_0, NV_PFIFO_INTR_CACHE_ERROR);
	}

	if (status & NV_PFIFO_INTR_DMA_PUSHER) {
		DRM_INFO("NV: PFIFO DMA pusher interrupt\n");

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
		DRM_INFO("NV: unknown PFIFO interrupt. status=0x%08x\n", status);

		NV_WRITE(NV03_PFIFO_INTR_0, status);
	}

	NV_WRITE(NV03_PMC_INTR_0, NV_PMC_INTR_0_PFIFO_PENDING);
}

#if 0
static void nouveau_nv04_context_switch(drm_device_t *dev)
{
	drm_nouveau_private_t *dev_priv = dev->dev_private;
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

static void
nouveau_graph_dump_trap_info(drm_device_t *dev)
{
	drm_nouveau_private_t *dev_priv = dev->dev_private;
	uint32_t address;
	uint32_t channel;
	uint32_t method, subc, data;

	address = NV_READ(0x400704);
	data    = NV_READ(0x400708);
	channel = (address >> 20) & 0x1F;
	subc    = (address >> 16) & 0x7;
	method  = address & 0x1FFC;

	DRM_ERROR("NV: nSource: 0x%08x, nStatus: 0x%08x\n",
			NV_READ(0x400108), NV_READ(0x400104));
	DRM_ERROR("NV: Channel %d/%d (class 0x%04x) -"
			"Method 0x%04x, Data 0x%08x\n",
			channel, subc,
			NV_READ(0x400160+subc*4) & 0xFFFF,
			method, data
		 );
}

static void nouveau_pgraph_irq_handler(drm_device_t *dev)
{
	uint32_t status;
	drm_nouveau_private_t *dev_priv = dev->dev_private;

	status = NV_READ(NV03_PGRAPH_INTR);
	if (!status)
		return;

	if (status & NV_PGRAPH_INTR_NOTIFY) {
		uint32_t nsource, nstatus, instance, notify;
		DRM_DEBUG("NV: PGRAPH notify interrupt\n");

		nstatus = NV_READ(0x00400104);
		nsource = NV_READ(0x00400108);
		DRM_DEBUG("nsource:0x%08x\tnstatus:0x%08x\n", nsource, nstatus);

		/* if this wasn't NOTIFICATION_PENDING, dump extra trap info */
		if (nsource & ~(1<<0)) {
			nouveau_graph_dump_trap_info(dev);
		} else {
			instance = NV_READ(0x00400158);
			notify   = NV_READ(0x00400150) >> 16;
			DRM_DEBUG("instance:0x%08x\tnotify:0x%08x\n",
					nsource, nstatus);
		}

		status &= ~NV_PGRAPH_INTR_NOTIFY;
		NV_WRITE(NV03_PGRAPH_INTR, NV_PGRAPH_INTR_NOTIFY);
	}

	if (status & NV_PGRAPH_INTR_BUFFER_NOTIFY) {
		uint32_t nsource, nstatus, instance, notify;
		DRM_DEBUG("NV: PGRAPH buffer notify interrupt\n");

		nstatus = NV_READ(0x00400104);
		nsource = NV_READ(0x00400108);
		DRM_DEBUG("nsource:0x%08x\tnstatus:0x%08x\n", nsource, nstatus);

		instance = NV_READ(0x00400158);
		notify   = NV_READ(0x00400150) >> 16;
		DRM_DEBUG("instance:0x%08x\tnotify:0x%08x\n", instance, notify);

		status &= ~NV_PGRAPH_INTR_BUFFER_NOTIFY;
		NV_WRITE(NV03_PGRAPH_INTR, NV_PGRAPH_INTR_BUFFER_NOTIFY);
	}

	if (status & NV_PGRAPH_INTR_MISSING_HW) {
		DRM_ERROR("NV: PGRAPH missing hw interrupt\n");

		status &= ~NV_PGRAPH_INTR_MISSING_HW;
		NV_WRITE(NV03_PGRAPH_INTR, NV_PGRAPH_INTR_MISSING_HW);
	}

	if (status & NV_PGRAPH_INTR_ERROR) {
		uint32_t nsource, nstatus, instance;

		DRM_ERROR("NV: PGRAPH error interrupt\n");

		nstatus = NV_READ(0x00400104);
		nsource = NV_READ(0x00400108);
		DRM_ERROR("nsource:0x%08x\tnstatus:0x%08x\n", nsource, nstatus);

		instance = NV_READ(0x00400158);
		DRM_ERROR("instance:0x%08x\n", instance);

		nouveau_graph_dump_trap_info(dev);

		status &= ~NV_PGRAPH_INTR_ERROR;
		NV_WRITE(NV03_PGRAPH_INTR, NV_PGRAPH_INTR_ERROR);
	}

	if (status & NV_PGRAPH_INTR_CONTEXT_SWITCH) {
		uint32_t channel=NV_READ(NV03_PFIFO_CACHE1_PUSH1)&(nouveau_fifo_number(dev)-1);
		DRM_INFO("NV: PGRAPH context switch interrupt channel %x\n",channel);
		switch(dev_priv->card_type)
		{
			case NV_04:
			case NV_05:
				nouveau_nv04_context_switch(dev);
				break;
			case NV_10:
			case NV_17:
				nouveau_nv10_context_switch(dev);
				break;
			case NV_20:
			case NV_30:
				nouveau_nv20_context_switch(dev);
				break;
			default:
				DRM_INFO("NV: Context switch not implemented\n");
				break;
		}

		status &= ~NV_PGRAPH_INTR_CONTEXT_SWITCH;
		NV_WRITE(NV03_PGRAPH_INTR, NV_PGRAPH_INTR_CONTEXT_SWITCH);
	}

	if (status) {
		DRM_INFO("NV: Unknown PGRAPH interrupt! STAT=0x%08x\n", status);
		NV_WRITE(NV03_PGRAPH_INTR, status);
	}

	NV_WRITE(NV03_PMC_INTR_0, NV_PMC_INTR_0_PGRAPH_PENDING);
}

static void nouveau_crtc_irq_handler(drm_device_t *dev, int crtc)
{
	drm_nouveau_private_t *dev_priv = dev->dev_private;
	if (crtc&1) {
		NV_WRITE(NV_CRTC0_INTSTAT, NV_CRTC_INTR_VBLANK);
	}

	if (crtc&2) {
		NV_WRITE(NV_CRTC1_INTSTAT, NV_CRTC_INTR_VBLANK);
	}
}

irqreturn_t nouveau_irq_handler(DRM_IRQ_ARGS)
{
	drm_device_t          *dev = (drm_device_t*)arg;
	drm_nouveau_private_t *dev_priv = dev->dev_private;
	uint32_t status;

	status = NV_READ(NV03_PMC_INTR_0);
	if (!status)
		return IRQ_NONE;

	DRM_DEBUG("PMC INTSTAT: 0x%08x\n", status);

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

