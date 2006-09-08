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
	NV_WRITE(NV_PGRAPH_INTEN, 0);
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
	NV_WRITE(NV_PGRAPH_INTEN,
				NV_PGRAPH_INTR_NOTIFY |
				NV_PGRAPH_INTR_MISSING_HW |
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
	NV_WRITE(NV_PGRAPH_INTEN, 0);
#if 0
	/* Disable CRTC0/1 interrupts */
	NV_WRITE(NV_CRTC0_INTEN, 0);
	NV_WRITE(NV_CRTC1_INTEN, 0);
#endif
	/* Master disable */
	NV_WRITE(NV_PMC_INTEN, 0);
}

void nouveau_fifo_irq_handler(drm_nouveau_private_t *dev_priv)
{
	uint32_t status, chmode, chstat;

	status = NV_READ(NV_PFIFO_INTSTAT);
	if (!status)
		return;
	chmode = NV_READ(NV_PFIFO_MODE);
	chstat = NV_READ(0x2508);

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

void nouveau_pgraph_irq_handler(drm_nouveau_private_t *dev_priv)
{
	uint32_t status;

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

	if (status) {
		DRM_INFO("NV: Unknown PGRAPH interrupt! STAT=0x%08x\n", status);
		NV_WRITE(NV_PGRAPH_INTSTAT, status);
	}

	NV_WRITE(NV_PMC_INTSTAT, NV_PMC_INTSTAT_PGRAPH_PENDING);
}

void nouveau_crtc_irq_handler(drm_nouveau_private_t *dev_priv, int crtc)
{
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
		nouveau_fifo_irq_handler(dev_priv);
		status &= ~NV_PMC_INTSTAT_PFIFO_PENDING;
	}
	if (status & NV_PMC_INTSTAT_PGRAPH_PENDING) {
		nouveau_pgraph_irq_handler(dev_priv);
		status &= ~NV_PMC_INTSTAT_PGRAPH_PENDING;
	}
	if (status & NV_PMC_INTSTAT_CRTCn_PENDING) {
		nouveau_crtc_irq_handler(dev_priv, (status>>24)&3);
		status &= ~NV_PMC_INTSTAT_CRTCn_PENDING;
	}

	if (status)
		DRM_ERROR("Unhandled PMC INTR status bits 0x%08x\n", status);

	return IRQ_HANDLED;
}

