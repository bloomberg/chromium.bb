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

	/* Disable/Clear PFIFO interrupts */
	NV_WRITE(NV_PFIFO_INTEN, 0);
	NV_WRITE(NV_PFIFO_INTSTAT, 0xFFFFFFFF);
	/* Disable/Clear PGRAPH interrupts */
	if (dev_priv->card_type<NV_40)
		NV_WRITE(NV04_PGRAPH_INTEN, 0);
	else
		NV_WRITE(NV40_PGRAPH_INTEN, 0);
	NV_WRITE(NV_PGRAPH_INTSTAT, 0xFFFFFFFF);
#if 0
	/* Disable/Clear CRTC0/1 interrupts */
	NV_WRITE(NV_CRTC0_INTEN, 0);
	NV_WRITE(NV_CRTC0_INTSTAT, NV_CRTC_INTR_VBLANK);
	NV_WRITE(NV_CRTC1_INTEN, 0);
	NV_WRITE(NV_CRTC1_INTSTAT, NV_CRTC_INTR_VBLANK);
#endif
	/* Master disable */
	NV_WRITE(NV_PMC_INTEN, 0);
}

void nouveau_irq_postinstall(drm_device_t *dev)
{
	drm_nouveau_private_t *dev_priv = dev->dev_private;

	DRM_DEBUG("IRQ: postinst\n");

	/* Enable PFIFO error reporting */
	NV_WRITE(NV_PFIFO_INTEN , NV_PFIFO_INTR_ERROR);
	NV_WRITE(NV_PFIFO_INTSTAT, 0xFFFFFFFF);

	/* Enable PGRAPH interrupts */
	if (dev_priv->card_type<NV_40)
		NV_WRITE(NV04_PGRAPH_INTEN,
				NV_PGRAPH_INTR_NOTIFY |
				NV_PGRAPH_INTR_MISSING_HW |
				NV_PGRAPH_INTR_CONTEXT_SWITCH |
				NV_PGRAPH_INTR_BUFFER_NOTIFY |
				NV_PGRAPH_INTR_ERROR
				);
	else
		NV_WRITE(NV40_PGRAPH_INTEN,
				NV_PGRAPH_INTR_NOTIFY |
				NV_PGRAPH_INTR_MISSING_HW |
				NV_PGRAPH_INTR_CONTEXT_SWITCH |
				NV_PGRAPH_INTR_BUFFER_NOTIFY |
				NV_PGRAPH_INTR_ERROR
				);
	NV_WRITE(NV_PGRAPH_INTSTAT, 0xFFFFFFFF);

#if 0
	/* Enable CRTC0/1 interrupts */
	NV_WRITE(NV_CRTC0_INTEN, NV_CRTC_INTR_VBLANK);
	NV_WRITE(NV_CRTC1_INTEN, NV_CRTC_INTR_VBLANK);
#endif

	/* Master enable */
	NV_WRITE(NV_PMC_INTEN, NV_PMC_INTEN_MASTER_ENABLE);
}

void nouveau_irq_uninstall(drm_device_t *dev)
{
	drm_nouveau_private_t *dev_priv = dev->dev_private;

	DRM_DEBUG("IRQ: uninst\n");

	/* Disable PFIFO interrupts */
	NV_WRITE(NV_PFIFO_INTEN, 0);
	/* Disable PGRAPH interrupts */
	if (dev_priv->card_type<NV_40)
		NV_WRITE(NV04_PGRAPH_INTEN, 0);
	else
		NV_WRITE(NV40_PGRAPH_INTEN, 0);
#if 0
	/* Disable CRTC0/1 interrupts */
	NV_WRITE(NV_CRTC0_INTEN, 0);
	NV_WRITE(NV_CRTC1_INTEN, 0);
#endif
	/* Master disable */
	NV_WRITE(NV_PMC_INTEN, 0);
}

static void nouveau_fifo_irq_handler(drm_device_t *dev)
{
	uint32_t status, chmode, chstat;
	drm_nouveau_private_t *dev_priv = dev->dev_private;

	status = NV_READ(NV_PFIFO_INTSTAT);
	if (!status)
		return;
	chmode = NV_READ(NV_PFIFO_MODE);
	chstat = NV_READ(NV_PFIFO_DMA);

	DRM_DEBUG("NV: PFIFO interrupt! INTSTAT=0x%08x/MODE=0x%08x/PEND=0x%08x\n",
			status, chmode, chstat);

	if (status & NV_PFIFO_INTR_ERROR) {
		DRM_ERROR("NV: PFIFO error interrupt\n");

		status &= ~NV_PFIFO_INTR_ERROR;
		NV_WRITE(NV_PFIFO_INTSTAT, NV_PFIFO_INTR_ERROR);
	}

	if (status) {
		DRM_INFO("NV: unknown PFIFO interrupt. status=0x%08x\n", status);

		NV_WRITE(NV_PFIFO_INTSTAT, status);
	}

	NV_WRITE(NV_PMC_INTSTAT, NV_PMC_INTSTAT_PFIFO_PENDING);
}

static void nouveau_nv04_context_switch(drm_device_t *dev)
{
	drm_nouveau_private_t *dev_priv = dev->dev_private;
	uint32_t channel,i;
	uint32_t max=0;
	NV_WRITE(NV_PGRAPH_FIFO,0x0);
	channel=NV_READ(NV_PFIFO_CACH1_PSH1)&(nouveau_fifo_number(dev)-1);
	//DRM_INFO("raw PFIFO_CACH1_PHS1 reg is %x\n",NV_READ(NV_PFIFO_CACH1_PSH1));
	//DRM_INFO("currently on channel %d\n",channel);
	for (i=0;i<nouveau_fifo_number(dev);i++)
		if ((dev_priv->fifos[i].used)&&(i!=channel)) {
			uint32_t put,get,pending;
			//put=NV_READ(dev_priv->ramfc_offset+i*32);
			//get=NV_READ(dev_priv->ramfc_offset+4+i*32);
			put=NV_READ(NV03_FIFO_REGS_DMAPUT(i));
			get=NV_READ(NV03_FIFO_REGS_DMAGET(i));
			pending=NV_READ(NV_PFIFO_DMA);
			//DRM_INFO("Channel %d (put/get %x/%x)\n",i,put,get);
			/* mark all pending channels as such */
			if ((put!=get)&!(pending&(1<<i)))
			{
				pending|=(1<<i);
				NV_WRITE(NV_PFIFO_DMA,pending);
			}
			max++;
		}
	nouveau_wait_for_idle(dev);

#if 1
	/* 2-channel commute */
	//		NV_WRITE(NV_PFIFO_CACH1_PSH1,channel|0x100);
	if (channel==0)
		channel=1;
	else
		channel=0;
	//		dev_priv->cur_fifo=channel;
	NV_WRITE(0x2050,channel|0x100);
#endif
	//NV_WRITE(NV_PFIFO_CACH1_PSH1,max|0x100);
	//NV_WRITE(0x2050,max|0x100);

	NV_WRITE(NV_PGRAPH_FIFO,0x1);
	
}

static void nouveau_nv10_context_switch(drm_device_t *dev)
{
	drm_nouveau_private_t *dev_priv = dev->dev_private;
	int channel;

	channel=NV_READ(NV_PFIFO_CACH1_PSH1)&(nouveau_fifo_number(dev)-1);
	/* 2-channel commute */
	if (channel==0)
		channel=1;
	else
		channel=0;
	dev_priv->cur_fifo=channel;

	NV_WRITE(NV_PGRAPH_CTX_CONTROL, 0x10000100);
	NV_WRITE(NV_PGRAPH_CTX_USER, (NV_READ(NV_PGRAPH_CTX_USER)&0xE0FFFFFF)|(dev_priv->cur_fifo<<24));
	NV_WRITE(NV_PGRAPH_FFINTFC_ST2, NV_READ(NV_PGRAPH_FFINTFC_ST2)&0xCFFFFFFF);
	/* touch PGRAPH_CTX_SWITCH* here ? */
	NV_WRITE(NV_PGRAPH_CTX_CONTROL, 0x10010100);
}

static void nouveau_pgraph_irq_handler(drm_device_t *dev)
{
	uint32_t status;
	drm_nouveau_private_t *dev_priv = dev->dev_private;

	status = NV_READ(NV_PGRAPH_INTSTAT);
	if (!status)
		return;

	if (status & NV_PGRAPH_INTR_NOTIFY) {
		uint32_t nsource, nstatus, instance, notify;
		DRM_DEBUG("NV: PGRAPH notify interrupt\n");

		nstatus = NV_READ(0x00400104);
		nsource = NV_READ(0x00400108);
		DRM_DEBUG("nsource:0x%08x\tnstatus:0x%08x\n", nsource, nstatus);

		instance = NV_READ(0x00400158);
		notify   = NV_READ(0x00400150) >> 16;
		DRM_DEBUG("instance:0x%08x\tnotify:0x%08x\n", nsource, nstatus);

		status &= ~NV_PGRAPH_INTR_NOTIFY;
		NV_WRITE(NV_PGRAPH_INTSTAT, NV_PGRAPH_INTR_NOTIFY);
	}

	if (status & NV_PGRAPH_INTR_BUFFER_NOTIFY) {
		uint32_t nsource, nstatus, instance, notify;
		DRM_DEBUG("NV: PGRAPH buffer notify interrupt\n");

		nstatus = NV_READ(0x00400104);
		nsource = NV_READ(0x00400108);
		DRM_DEBUG("nsource:0x%08x\tnstatus:0x%08x\n", nsource, nstatus);

		instance = NV_READ(0x00400158);
		notify   = NV_READ(0x00400150) >> 16;
		DRM_DEBUG("instance:0x%08x\tnotify:0x%08x\n", nsource, nstatus);

		status &= ~NV_PGRAPH_INTR_BUFFER_NOTIFY;
		NV_WRITE(NV_PGRAPH_INTSTAT, NV_PGRAPH_INTR_BUFFER_NOTIFY);
	}

	if (status & NV_PGRAPH_INTR_MISSING_HW) {
		DRM_ERROR("NV: PGRAPH missing hw interrupt\n");

		status &= ~NV_PGRAPH_INTR_MISSING_HW;
		NV_WRITE(NV_PGRAPH_INTSTAT, NV_PGRAPH_INTR_MISSING_HW);
	}

	if (status & NV_PGRAPH_INTR_ERROR) {
		DRM_ERROR("NV: PGRAPH error interrupt\n");

		status &= ~NV_PGRAPH_INTR_ERROR;
		NV_WRITE(NV_PGRAPH_INTSTAT, NV_PGRAPH_INTR_ERROR);
	}

	if (status & NV_PGRAPH_INTR_CONTEXT_SWITCH) {
		uint32_t channel=NV_READ(NV_PFIFO_CACH1_PSH1)&(nouveau_fifo_number(dev)-1);
		DRM_INFO("NV: PGRAPH context switch interrupt channel %x\n",channel);
		switch(dev_priv->card_type)
		{
			case NV_04:
				nouveau_nv04_context_switch(dev);
				break;
			case NV_10:
				nouveau_nv10_context_switch(dev);
				break;
			default:
				DRM_INFO("NV: Context switch not implemented\n");
				break;
		}

		status &= ~NV_PGRAPH_INTR_CONTEXT_SWITCH;
		NV_WRITE(NV_PGRAPH_INTSTAT, NV_PGRAPH_INTR_CONTEXT_SWITCH);
	}

	if (status) {
		DRM_INFO("NV: Unknown PGRAPH interrupt! STAT=0x%08x\n", status);
		NV_WRITE(NV_PGRAPH_INTSTAT, status);
	}

	NV_WRITE(NV_PMC_INTSTAT, NV_PMC_INTSTAT_PGRAPH_PENDING);
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

	status = NV_READ(NV_PMC_INTSTAT);

	DRM_DEBUG("PMC INTSTAT: 0x%08x\n", status);

	if (status & NV_PMC_INTSTAT_PFIFO_PENDING) {
		nouveau_fifo_irq_handler(dev);
		status &= ~NV_PMC_INTSTAT_PFIFO_PENDING;
	}
	if (status & NV_PMC_INTSTAT_PGRAPH_PENDING) {
		nouveau_pgraph_irq_handler(dev);
		status &= ~NV_PMC_INTSTAT_PGRAPH_PENDING;
	}
	if (status & NV_PMC_INTSTAT_CRTCn_PENDING) {
		nouveau_crtc_irq_handler(dev, (status>>24)&3);
		status &= ~NV_PMC_INTSTAT_CRTCn_PENDING;
	}

	if (status)
		DRM_ERROR("Unhandled PMC INTR status bits 0x%08x\n", status);

	return IRQ_HANDLED;
}

