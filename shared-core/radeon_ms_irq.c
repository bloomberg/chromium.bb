/*
 * Copyright 2007 Jérôme Glisse
 * Copyright 2007 Alex Deucher
 * Copyright 2007 Dave Airlie
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
 *
 * Authors:
 *    Jerome Glisse <glisse@freedesktop.org>
 */
#include "drmP.h"
#include "drm.h"
#include "radeon_ms.h"

static uint32_t radeon_ack_irqs(struct drm_radeon_private *dev_priv,
				uint32_t mask)
{
	uint32_t irqs;

	irqs = MMIO_R(GEN_INT_STATUS);
	if (irqs) {
		MMIO_W(GEN_INT_STATUS, irqs);
	}
	if (irqs & (~mask)) {
		/* reprogram irq */
		MMIO_W(GEN_INT_CNTL, dev_priv->driver_state.gen_int_cntl);
	}
	return irqs;
}

void radeon_ms_irq_emit(struct drm_device *dev)
{
	struct drm_radeon_private *dev_priv = dev->dev_private;
	uint32_t cmd[4];
	int i, ret;

	cmd[0] = CP_PACKET0(GEN_INT_CNTL, 1);
	cmd[1] = dev_priv->driver_state.gen_int_cntl | GEN_INT_CNTL__SW_INT_EN;
	cmd[2] = GEN_INT_STATUS__SW_INT_SET;
	/* try to wait but if we timeout we likely are in bad situation */
	for (i = 0; i < dev_priv->usec_timeout; i++) {
		ret = radeon_ms_ring_emit(dev, cmd, 3);
		if (!ret) {
			break;
		}
	}
}

static void radeon_ms_irq_enable(struct drm_device *dev)
{
	struct drm_radeon_private *dev_priv = dev->dev_private;
	struct radeon_state *state = &dev_priv->driver_state;

	state->gen_int_cntl = GEN_INT_CNTL__SW_INT_EN;
	radeon_ms_irq_restore(dev, state);
}

irqreturn_t radeon_ms_irq_handler(DRM_IRQ_ARGS)
{
	struct drm_device *dev = (struct drm_device *)arg;
	struct drm_radeon_private *dev_priv = dev->dev_private;
	uint32_t status, mask;

	/* Only consider the bits we're interested in - others could be used
	 * outside the DRM
	 */
	mask = GEN_INT_STATUS__SW_INT |
	       GEN_INT_STATUS__CRTC_VBLANK_STAT |
	       GEN_INT_STATUS__CRTC2_VBLANK_STAT;
	status = radeon_ack_irqs(dev_priv, mask);
	if (!status) {
		return IRQ_NONE;
	}

	/* SW interrupt */
	if (GEN_INT_STATUS__SW_INT & status) {
		radeon_ms_fence_handler(dev);
	}
	radeon_ms_fence_handler(dev);
	return IRQ_HANDLED;
}

void radeon_ms_irq_preinstall(struct drm_device * dev)
{
	struct drm_radeon_private *dev_priv = dev->dev_private;
	struct radeon_state *state = &dev_priv->driver_state;
	uint32_t mask;

	/* Disable *all* interrupts */
	state->gen_int_cntl = 0;
	radeon_ms_irq_restore(dev, state);

	/* Clear bits if they're already high */
	mask = GEN_INT_STATUS__SW_INT |
	       GEN_INT_STATUS__CRTC_VBLANK_STAT |
	       GEN_INT_STATUS__CRTC2_VBLANK_STAT;
	radeon_ack_irqs(dev_priv, mask);
}

void radeon_ms_irq_postinstall(struct drm_device * dev)
{
	radeon_ms_irq_enable(dev);
}

int radeon_ms_irq_init(struct drm_device *dev)
{
	struct drm_radeon_private *dev_priv = dev->dev_private;
	struct radeon_state *state = &dev_priv->driver_state;

	/* Disable *all* interrupts */
	state->gen_int_cntl = 0;
	radeon_ms_irq_restore(dev, state);
	return 0;
}

void radeon_ms_irq_restore(struct drm_device *dev, struct radeon_state *state)
{
	struct drm_radeon_private *dev_priv = dev->dev_private;

	MMIO_W(GEN_INT_CNTL, state->gen_int_cntl);
}

void radeon_ms_irq_save(struct drm_device *dev, struct radeon_state *state)
{
	struct drm_radeon_private *dev_priv = dev->dev_private;

	state->gen_int_cntl = MMIO_R(GEN_INT_CNTL);
}

void radeon_ms_irq_uninstall(struct drm_device * dev)
{
	struct drm_radeon_private *dev_priv = dev->dev_private;
	struct radeon_state *state = &dev_priv->driver_state;

	if (dev_priv == NULL) {
		return;
	}

	/* Disable *all* interrupts */
	state->gen_int_cntl = 0;
	radeon_ms_irq_restore(dev, state);
}
