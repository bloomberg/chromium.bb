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
#include "amd_r3xx_fence.h"

#define R3XX_FENCE_SEQUENCE_RW_FLUSH	0x80000000u

static inline int r3xx_fence_emit_sequence(struct drm_device *dev,
					   struct drm_radeon_private *dev_priv,
					   uint32_t sequence)
{
	struct r3xx_fence *r3xx_fence = dev_priv->fence;
	uint32_t cmd[2];
	int i, r;

	if (sequence & R3XX_FENCE_SEQUENCE_RW_FLUSH) {
		r3xx_fence->sequence_last_flush =
			sequence & ~R3XX_FENCE_SEQUENCE_RW_FLUSH;
		/* Ask flush for VERTEX & FRAGPROG pipeline
		 * have 3D idle  */
		dev_priv->flush_cache(dev);
	}
	cmd[0] = CP_PACKET0(dev_priv->fence_reg, 0);
	cmd[1] = sequence;
	for (i = 0; i < dev_priv->usec_timeout; i++) {
		r = radeon_ms_ring_emit(dev, cmd, 2);
		if (!r) {
			dev_priv->irq_emit(dev);
			return 0;
		}
	}
	return -EBUSY;
}

static inline uint32_t r3xx_fence_sequence(struct r3xx_fence *r3xx_fence)
{
	r3xx_fence->sequence += 1;
	if (unlikely(r3xx_fence->sequence > 0x7fffffffu)) {
		r3xx_fence->sequence = 1;
	}
	return r3xx_fence->sequence;
}

static inline void r3xx_fence_report(struct drm_device *dev,
				     struct drm_radeon_private *dev_priv,
				     struct r3xx_fence *r3xx_fence)
{
	uint32_t fence_types = DRM_FENCE_TYPE_EXE;
	uint32_t sequence;

	if (dev_priv == NULL) {
		return;
	}
	sequence = mmio_read(dev_priv, dev_priv->fence_reg);
	if (sequence & R3XX_FENCE_SEQUENCE_RW_FLUSH) {
		sequence &= ~R3XX_FENCE_SEQUENCE_RW_FLUSH;
		fence_types |= DRM_RADEON_FENCE_TYPE_RW;
		if (sequence == r3xx_fence->sequence_last_flush) {
			r3xx_fence->sequence_last_flush = 0;
		}
	}
	/* avoid to report already reported sequence */
	if (sequence != r3xx_fence->sequence_last_reported) {
		drm_fence_handler(dev, 0, sequence, fence_types, 0);
		r3xx_fence->sequence_last_reported = sequence;
	}
}

static void r3xx_fence_flush(struct drm_device *dev, uint32_t class)
{
	struct drm_radeon_private *dev_priv = dev->dev_private;
	struct r3xx_fence *r3xx_fence = dev_priv->fence;
	uint32_t sequence;

	sequence = r3xx_fence_sequence(r3xx_fence);
	sequence |= R3XX_FENCE_SEQUENCE_RW_FLUSH;
	r3xx_fence_emit_sequence(dev, dev_priv, sequence);
}

static void r3xx_fence_poll(struct drm_device *dev, uint32_t fence_class,
			    uint32_t waiting_types)
{
	struct drm_radeon_private *dev_priv = dev->dev_private;
	struct drm_fence_manager *fm = &dev->fm;
	struct drm_fence_class_manager *fc = &fm->fence_class[fence_class];
	struct r3xx_fence *r3xx_fence = dev_priv->fence;

	if (unlikely(!dev_priv)) {
		return;
	}
	/* if there is a RW flush pending then submit new sequence
	 * preceded by flush cmds */
	if (fc->pending_flush & DRM_RADEON_FENCE_TYPE_RW) {
		r3xx_fence_flush(dev, 0);
		fc->pending_flush &= ~DRM_RADEON_FENCE_TYPE_RW;
	}
	r3xx_fence_report(dev, dev_priv, r3xx_fence);
	return;
}

static int r3xx_fence_emit(struct drm_device *dev, uint32_t class,
			   uint32_t flags, uint32_t *sequence,
			   uint32_t *native_type)
{
	struct drm_radeon_private *dev_priv = dev->dev_private;
	struct r3xx_fence *r3xx_fence = dev_priv->fence;
	uint32_t tmp;

	if (!dev_priv || dev_priv->cp_ready != 1) {
		return -EINVAL;
	}
	*sequence = tmp = r3xx_fence_sequence(r3xx_fence);
	*native_type = DRM_FENCE_TYPE_EXE;
	if (flags & DRM_RADEON_FENCE_FLAG_FLUSHED) {
		*native_type |= DRM_RADEON_FENCE_TYPE_RW;
		tmp |= R3XX_FENCE_SEQUENCE_RW_FLUSH;
	}
	return r3xx_fence_emit_sequence(dev, dev_priv, tmp);
}

static int r3xx_fence_has_irq(struct drm_device *dev,
			      uint32_t class, uint32_t type)
{
	const uint32_t type_irq_mask = DRM_FENCE_TYPE_EXE |
				       DRM_RADEON_FENCE_TYPE_RW;
	/*
	 * We have an irq for EXE & RW fence.
	 */
	if (class == 0 && (type & type_irq_mask)) {
		return 1;
	}
	return 0;
}

static uint32_t r3xx_fence_needed_flush(struct drm_fence_object *fence)
{
	struct drm_device *dev = fence->dev;
	struct drm_radeon_private *dev_priv = dev->dev_private;
	struct r3xx_fence *r3xx_fence = dev_priv->fence;
	struct drm_fence_driver *driver = dev->driver->fence_driver;
	uint32_t flush_types, diff;
	
	flush_types = fence->waiting_types &	
		      ~(DRM_FENCE_TYPE_EXE | fence->signaled_types);

	if (flush_types == 0 || ((flush_types & ~fence->native_types) == 0)) {
		return 0;
	}
	if (unlikely(dev_priv == NULL)) {
		return 0;
	}
	if (r3xx_fence->sequence_last_flush) {
		diff = (r3xx_fence->sequence_last_flush - fence->sequence) & 
		       driver->sequence_mask;
		if (diff < driver->wrap_diff) {
			return 0;
		}
	}
	return flush_types;
}

static int r3xx_fence_wait(struct drm_fence_object *fence,
			   int lazy, int interruptible, uint32_t mask)
{
	struct drm_device *dev = fence->dev;
	struct drm_fence_manager *fm = &dev->fm;
	struct drm_fence_class_manager *fc = &fm->fence_class[0];
	int r;

	drm_fence_object_flush(fence, mask);
	if (likely(interruptible)) {
		r = wait_event_interruptible_timeout(
			fc->fence_queue,
			drm_fence_object_signaled(fence, DRM_FENCE_TYPE_EXE), 
			3 * DRM_HZ);
	} else {
		r = wait_event_timeout(
			fc->fence_queue,
			drm_fence_object_signaled(fence, DRM_FENCE_TYPE_EXE), 
			3 * DRM_HZ);
	}
	if (unlikely(r == -ERESTARTSYS)) {
		return -EAGAIN;
	}
	if (unlikely(r == 0)) {
		return -EBUSY;
	}

	if (likely(mask == DRM_FENCE_TYPE_EXE || 
		   drm_fence_object_signaled(fence, mask))) {
		return 0;
	}

	/*
	 * Poll for sync flush completion.
	 */
	return drm_fence_wait_polling(fence, lazy, interruptible,
				      mask, 3 * DRM_HZ);
}

struct drm_fence_driver r3xx_fence_driver = {
	.num_classes = 1,
	.wrap_diff = (1 << 29),
	.flush_diff = (1 << 28),
	.sequence_mask = 0x7fffffffU,
	.has_irq = r3xx_fence_has_irq,
	.emit = r3xx_fence_emit,
	.flush = r3xx_fence_flush,
	.poll = r3xx_fence_poll,
	.needed_flush = r3xx_fence_needed_flush,
	.wait = r3xx_fence_wait,
};

/* this are used by the buffer object code */
int r3xx_fence_types(struct drm_buffer_object *bo,
		     uint32_t *class, uint32_t *type)
{
	*class = 0;
	if (bo->mem.flags & (DRM_BO_FLAG_READ | DRM_BO_FLAG_WRITE)) {
		*type = DRM_FENCE_TYPE_EXE | DRM_RADEON_FENCE_TYPE_RW;
	} else {
		*type = DRM_FENCE_TYPE_EXE;
	}
	return 0;
}

/* this are used by the irq code */
void r3xx_fence_handler(struct drm_device * dev)
{
	struct drm_radeon_private *dev_priv = dev->dev_private;
	struct drm_fence_manager *fm = &dev->fm;
	struct drm_fence_class_manager *fc = &fm->fence_class[0];

	if (unlikely(dev_priv == NULL)) {
		return;
	}

	write_lock(&fm->lock);
	r3xx_fence_poll(dev, 0, fc->waiting_types);
	write_unlock(&fm->lock);
}
