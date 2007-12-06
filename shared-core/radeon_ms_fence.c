/*
 * Copyright 2007 Dave Airlie.
 * Copyright 2007 Jérôme Glisse
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
 *    Dave Airlie
 *    Jerome Glisse <glisse@freedesktop.org>
 */
#include "radeon_ms.h"

static void radeon_ms_fence_flush(struct drm_device *dev)
{
	struct drm_radeon_private *dev_priv = dev->dev_private;
	struct drm_fence_class_manager *fc = &dev->fm.fence_class[0];
	uint32_t pending_flush_types = 0;
	uint32_t sequence;

	if (dev_priv == NULL) {
		return;
	}
	pending_flush_types = fc->pending_flush |
			      ((fc->pending_exe_flush) ?
				DRM_FENCE_TYPE_EXE : 0);
	if (pending_flush_types) {
		sequence = mmio_read(dev_priv, dev_priv->fence_reg);
		drm_fence_handler(dev, 0, sequence, pending_flush_types, 0);
	}
}

int radeon_ms_fence_emit_sequence(struct drm_device *dev, uint32_t class,
			 	  uint32_t flags, uint32_t *sequence,
				  uint32_t *native_type)
{
	struct drm_radeon_private *dev_priv = dev->dev_private;
	uint32_t fence_id, cmd[2], i, ret;

	if (!dev_priv || dev_priv->cp_ready != 1) {
		return -EINVAL;
	}
	fence_id = (++dev_priv->fence_id_last);
	if (dev_priv->fence_id_last > 0x7FFFFFFF) {
		fence_id = dev_priv->fence_id_last = 1;
	}
	*sequence = fence_id;
	*native_type = DRM_FENCE_TYPE_EXE;
	if (flags & DRM_RADEON_FENCE_FLAG_FLUSHED) {
		*native_type |= DRM_RADEON_FENCE_TYPE_RW;
		dev_priv->flush_cache(dev);
	}
	cmd[0] = CP_PACKET0(dev_priv->fence_reg, 0);
	cmd[1] = fence_id;
	for (i = 0; i < dev_priv->usec_timeout; i++) {
		ret = radeon_ms_ring_emit(dev, cmd, 2);
		if (!ret) {
			dev_priv->irq_emit(dev);
			return 0;
		}
	}
	return -EBUSY;
}

void radeon_ms_fence_handler(struct drm_device * dev)
{
	struct drm_radeon_private *dev_priv = dev->dev_private;
	struct drm_fence_manager *fm = &dev->fm;

	if (dev_priv == NULL) {
		return;
	}

	write_lock(&fm->lock);
	radeon_ms_fence_flush(dev);
	write_unlock(&fm->lock);
}

int radeon_ms_fence_has_irq(struct drm_device *dev, uint32_t class,
			    uint32_t flags)
{
	/*
	 * We have an irq that tells us when we have a new breadcrumb.
	 */
	if (class == 0 && flags == DRM_FENCE_TYPE_EXE)
		return 1;

	return 0;
}

int radeon_ms_fence_types(struct drm_buffer_object *bo,
			  uint32_t *class, uint32_t *type)
{
	*class = 0;
	if (bo->mem.flags & (DRM_BO_FLAG_READ | DRM_BO_FLAG_WRITE))
		*type = 3;
	else
		*type = 1;
	return 0;
}

void radeon_ms_poke_flush(struct drm_device *dev, uint32_t class)
{
	struct drm_fence_manager *fm = &dev->fm;
	unsigned long flags;

	if (class != 0)
		return;
	write_lock_irqsave(&fm->lock, flags);
	radeon_ms_fence_flush(dev);
	write_unlock_irqrestore(&fm->lock, flags);
}
