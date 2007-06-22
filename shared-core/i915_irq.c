/* i915_irq.c -- IRQ support for the I915 -*- linux-c -*-
 */
/*
 * Copyright 2003 Tungsten Graphics, Inc., Cedar Park, Texas.
 * All Rights Reserved.
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sub license, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 * 
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial portions
 * of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT.
 * IN NO EVENT SHALL TUNGSTEN GRAPHICS AND/OR ITS SUPPLIERS BE LIABLE FOR
 * ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 * 
 */

#include "drmP.h"
#include "drm.h"
#include "i915_drm.h"
#include "i915_drv.h"

#define USER_INT_FLAG (1<<1)
#define VSYNC_PIPEB_FLAG (1<<5)
#define VSYNC_PIPEA_FLAG (1<<7)

#define MAX_NOPID ((u32)~0)

/**
 * Emit a synchronous flip.
 *
 * This function must be called with the drawable spinlock held.
 */
static void
i915_dispatch_vsync_flip(drm_device_t *dev, drm_drawable_info_t *drw, int pipe)
{
	drm_i915_private_t *dev_priv = (drm_i915_private_t *) dev->dev_private;
	drm_i915_sarea_t *sarea_priv = dev_priv->sarea_priv;
	u16 x1, y1, x2, y2;
	int pf_pipes = 1 << pipe;

	/* If the window is visible on the other pipe, we have to flip on that
	 * pipe as well.
	 */
	if (pipe == 1) {
		x1 = sarea_priv->pipeA_x;
		y1 = sarea_priv->pipeA_y;
		x2 = x1 + sarea_priv->pipeA_w;
		y2 = y1 + sarea_priv->pipeA_h;
	} else {
		x1 = sarea_priv->pipeB_x;
		y1 = sarea_priv->pipeB_y;
		x2 = x1 + sarea_priv->pipeB_w;
		y2 = y1 + sarea_priv->pipeB_h;
	}

	if (x2 > 0 && y2 > 0) {
		int i, num_rects = drw->num_rects;
		drm_clip_rect_t *rect = drw->rects;

		for (i = 0; i < num_rects; i++)
			if (!(rect[i].x1 >= x2 || rect[i].y1 >= y2 ||
			      rect[i].x2 <= x1 || rect[i].y2 <= y1)) {
				pf_pipes = 0x3;

				break;
			}
	}

	i915_dispatch_flip(dev, pf_pipes, 1);
}

/**
 * Emit blits for scheduled buffer swaps.
 *
 * This function will be called with the HW lock held.
 */
static void i915_vblank_tasklet(drm_device_t *dev)
{
	drm_i915_private_t *dev_priv = (drm_i915_private_t *) dev->dev_private;
	unsigned long irqflags;
	struct list_head *list, *tmp, hits, *hit;
	int nhits, nrects, slice[2], upper[2], lower[2], i, num_pages;
	unsigned counter[2];
	drm_drawable_info_t *drw;
	drm_i915_sarea_t *sarea_priv = dev_priv->sarea_priv;
	u32 cpp = dev_priv->cpp,  offsets[3];
	u32 cmd = (cpp == 4) ? (XY_SRC_COPY_BLT_CMD |
				XY_SRC_COPY_BLT_WRITE_ALPHA |
				XY_SRC_COPY_BLT_WRITE_RGB)
			     : XY_SRC_COPY_BLT_CMD;
	u32 pitchropcpp = (sarea_priv->pitch * cpp) | (0xcc << 16) |
			  (cpp << 23) | (1 << 24);
	RING_LOCALS;

	counter[0] = drm_vblank_count(dev, 0);
	counter[1] = drm_vblank_count(dev, 1);

	DRM_DEBUG("\n");

	INIT_LIST_HEAD(&hits);

	nhits = nrects = 0;

	spin_lock_irqsave(&dev_priv->swaps_lock, irqflags);

	/* Find buffer swaps scheduled for this vertical blank */
	list_for_each_safe(list, tmp, &dev_priv->vbl_swaps.head) {
		drm_i915_vbl_swap_t *vbl_swap =
			list_entry(list, drm_i915_vbl_swap_t, head);
		int crtc = vbl_swap->pipe;

		if ((counter[vbl_swap->pipe] - vbl_swap->sequence) > (1<<23))
			continue;

		list_del(list);
		dev_priv->swaps_pending--;
		drm_vblank_put(dev, crtc);

		spin_unlock(&dev_priv->swaps_lock);
		spin_lock(&dev->drw_lock);

		drw = drm_get_drawable_info(dev, vbl_swap->drw_id);

		if (!drw) {
			spin_unlock(&dev->drw_lock);
			drm_free(vbl_swap, sizeof(*vbl_swap), DRM_MEM_DRIVER);
			spin_lock(&dev_priv->swaps_lock);
			continue;
		}

		list_for_each(hit, &hits) {
			drm_i915_vbl_swap_t *swap_cmp =
				list_entry(hit, drm_i915_vbl_swap_t, head);
			drm_drawable_info_t *drw_cmp =
				drm_get_drawable_info(dev, swap_cmp->drw_id);

			if (drw_cmp &&
			    drw_cmp->rects[0].y1 > drw->rects[0].y1) {
				list_add_tail(list, hit);
				break;
			}
		}

		spin_unlock(&dev->drw_lock);

		/* List of hits was empty, or we reached the end of it */
		if (hit == &hits)
			list_add_tail(list, hits.prev);

		nhits++;

		spin_lock(&dev_priv->swaps_lock);
	}

	if (nhits == 0) {
		spin_unlock_irqrestore(&dev_priv->swaps_lock, irqflags);
		return;
	}

	spin_unlock(&dev_priv->swaps_lock);

	i915_kernel_lost_context(dev);

	upper[0] = upper[1] = 0;
	slice[0] = max(sarea_priv->pipeA_h / nhits, 1);
	slice[1] = max(sarea_priv->pipeB_h / nhits, 1);
	lower[0] = sarea_priv->pipeA_y + slice[0];
	lower[1] = sarea_priv->pipeB_y + slice[0];

	offsets[0] = sarea_priv->front_offset;
	offsets[1] = sarea_priv->back_offset;
	offsets[2] = sarea_priv->third_offset;
	num_pages = sarea_priv->third_handle ? 3 : 2;

	spin_lock(&dev->drw_lock);

	/* Emit blits for buffer swaps, partitioning both outputs into as many
	 * slices as there are buffer swaps scheduled in order to avoid tearing
	 * (based on the assumption that a single buffer swap would always
	 * complete before scanout starts).
	 */
	for (i = 0; i++ < nhits;
	     upper[0] = lower[0], lower[0] += slice[0],
	     upper[1] = lower[1], lower[1] += slice[1]) {
		int init_drawrect = 1;

		if (i == nhits)
			lower[0] = lower[1] = sarea_priv->height;

		list_for_each(hit, &hits) {
			drm_i915_vbl_swap_t *swap_hit =
				list_entry(hit, drm_i915_vbl_swap_t, head);
			drm_clip_rect_t *rect;
			int num_rects, pipe, front, back;
			unsigned short top, bottom;

			drw = drm_get_drawable_info(dev, swap_hit->drw_id);

			if (!drw)
				continue;

			pipe = swap_hit->pipe;

			if (swap_hit->flip) {
				i915_dispatch_vsync_flip(dev, drw, pipe);
				continue;
			}

			if (init_drawrect) {
				BEGIN_LP_RING(6);

				OUT_RING(GFX_OP_DRAWRECT_INFO);
				OUT_RING(0);
				OUT_RING(0);
				OUT_RING(sarea_priv->width | sarea_priv->height << 16);
				OUT_RING(sarea_priv->width | sarea_priv->height << 16);
				OUT_RING(0);

				ADVANCE_LP_RING();

				sarea_priv->ctxOwner = DRM_KERNEL_CONTEXT;

				init_drawrect = 0;
			}

			rect = drw->rects;
			top = upper[pipe];
			bottom = lower[pipe];

			front = (dev_priv->sarea_priv->pf_current_page >>
				 (2 * pipe)) & 0x3;
			back = (front + 1) % num_pages;

			for (num_rects = drw->num_rects; num_rects--; rect++) {
				int y1 = max(rect->y1, top);
				int y2 = min(rect->y2, bottom);

				if (y1 >= y2)
					continue;

				BEGIN_LP_RING(8);

				OUT_RING(cmd);
				OUT_RING(pitchropcpp);
				OUT_RING((y1 << 16) | rect->x1);
				OUT_RING((y2 << 16) | rect->x2);
				OUT_RING(offsets[front]);
				OUT_RING((y1 << 16) | rect->x1);
				OUT_RING(pitchropcpp & 0xffff);
				OUT_RING(offsets[back]);

				ADVANCE_LP_RING();
			}
		}
	}

	spin_unlock_irqrestore(&dev->drw_lock, irqflags);

	list_for_each_safe(hit, tmp, &hits) {
		drm_i915_vbl_swap_t *swap_hit =
			list_entry(hit, drm_i915_vbl_swap_t, head);

		list_del(hit);

		drm_free(swap_hit, sizeof(*swap_hit), DRM_MEM_DRIVER);
	}
}

u32 i915_get_vblank_counter(drm_device_t *dev, int crtc)
{
	drm_i915_private_t *dev_priv = (drm_i915_private_t *) dev->dev_private;
	unsigned long high_frame = crtc ? PIPEBFRAMEHIGH : PIPEAFRAMEHIGH;
	unsigned long low_frame = crtc ? PIPEBFRAMEPIXEL : PIPEAFRAMEPIXEL;
	u32 high1, high2, low, count;

	if (!IS_I965G(dev))
		return 0;

	/*
	 * High & low register fields aren't synchronized, so make sure
	 * we get a low value that's stable across two reads of the high
	 * register.
	 */
	do {
		high1 = ((I915_READ(high_frame) & PIPE_FRAME_HIGH_MASK) >>
			 PIPE_FRAME_HIGH_SHIFT);
		low =  ((I915_READ(low_frame) & PIPE_FRAME_LOW_MASK) >>
			PIPE_FRAME_LOW_SHIFT);
		high2 = ((I915_READ(high_frame) & PIPE_FRAME_HIGH_MASK) >>
			 PIPE_FRAME_HIGH_SHIFT);
	} while (high1 != high2);

	count = (high1 << 8) | low;

	return count;
}

irqreturn_t i915_driver_irq_handler(DRM_IRQ_ARGS)
{
	drm_device_t *dev = (drm_device_t *) arg;
	drm_i915_private_t *dev_priv = (drm_i915_private_t *) dev->dev_private;
	u16 temp;
	u32 pipea_stats, pipeb_stats;

	pipea_stats = I915_READ(I915REG_PIPEASTAT);
	pipeb_stats = I915_READ(I915REG_PIPEBSTAT);
		
	temp = I915_READ16(I915REG_INT_IDENTITY_R);

#if 0
	DRM_DEBUG("%s flag=%08x\n", __FUNCTION__, temp);
#endif
	if (temp == 0)
		return IRQ_NONE;

	I915_WRITE16(I915REG_INT_IDENTITY_R, temp);
	(void) I915_READ16(I915REG_INT_IDENTITY_R); /* Flush posted write */

	temp &= (dev_priv->irq_enable_reg | USER_INT_FLAG | VSYNC_PIPEA_FLAG |
		 VSYNC_PIPEB_FLAG);

	dev_priv->sarea_priv->last_dispatch = READ_BREADCRUMB(dev_priv);

	if (temp & USER_INT_FLAG) {
		DRM_WAKEUP(&dev_priv->irq_queue);
#ifdef I915_HAVE_FENCE
		i915_fence_handler(dev);
#endif
	}

	if (!IS_I965G(dev)) {
		if (temp & VSYNC_PIPEA_FLAG)
			atomic_inc(&dev->_vblank_count[0]);
		if (temp & VSYNC_PIPEB_FLAG)
			atomic_inc(&dev->_vblank_count[1]);
	}

	/*
	 * Use drm_update_vblank_counter here to deal with potential lost
	 * interrupts
	 */
	if (temp & VSYNC_PIPEA_FLAG)
		drm_handle_vblank(dev, 0);
	if (temp & VSYNC_PIPEB_FLAG)
		drm_handle_vblank(dev, 1);

	if (temp & (VSYNC_PIPEA_FLAG | VSYNC_PIPEB_FLAG)) {
		if (dev_priv->swaps_pending > 0)
			drm_locked_tasklet(dev, i915_vblank_tasklet);
		I915_WRITE(I915REG_PIPEASTAT, 
			pipea_stats|I915_VBLANK_INTERRUPT_ENABLE|
			I915_VBLANK_CLEAR);
		I915_WRITE(I915REG_PIPEBSTAT,
			pipeb_stats|I915_VBLANK_INTERRUPT_ENABLE|
			I915_VBLANK_CLEAR);
	}

	return IRQ_HANDLED;
}

int i915_emit_irq(drm_device_t * dev)
{
	
	drm_i915_private_t *dev_priv = dev->dev_private;
	RING_LOCALS;

	i915_kernel_lost_context(dev);

	DRM_DEBUG("%s\n", __FUNCTION__);

	i915_emit_breadcrumb(dev);

	BEGIN_LP_RING(2);
	OUT_RING(0);
	OUT_RING(GFX_OP_USER_INTERRUPT);
	ADVANCE_LP_RING();

	return dev_priv->counter;


}

void i915_user_irq_on(drm_i915_private_t *dev_priv)
{
	spin_lock(&dev_priv->user_irq_lock);
	if (dev_priv->irq_enabled && (++dev_priv->user_irq_refcount == 1)){
		dev_priv->irq_enable_reg |= USER_INT_FLAG;
		I915_WRITE16(I915REG_INT_ENABLE_R, dev_priv->irq_enable_reg);
	}
	spin_unlock(&dev_priv->user_irq_lock);

}
		
void i915_user_irq_off(drm_i915_private_t *dev_priv)
{
	spin_lock(&dev_priv->user_irq_lock);
	if (dev_priv->irq_enabled && (--dev_priv->user_irq_refcount == 0)) {
		//		dev_priv->irq_enable_reg &= ~USER_INT_FLAG;
		//		I915_WRITE16(I915REG_INT_ENABLE_R, dev_priv->irq_enable_reg);
	}
	spin_unlock(&dev_priv->user_irq_lock);
}
		

static int i915_wait_irq(drm_device_t * dev, int irq_nr)
{
	drm_i915_private_t *dev_priv = (drm_i915_private_t *) dev->dev_private;
	int ret = 0;

	DRM_DEBUG("%s irq_nr=%d breadcrumb=%d\n", __FUNCTION__, irq_nr,
		  READ_BREADCRUMB(dev_priv));

	if (READ_BREADCRUMB(dev_priv) >= irq_nr)
		return 0;

	dev_priv->sarea_priv->perf_boxes |= I915_BOX_WAIT;
	
	i915_user_irq_on(dev_priv);
	DRM_WAIT_ON(ret, dev_priv->irq_queue, 3 * DRM_HZ,
		    READ_BREADCRUMB(dev_priv) >= irq_nr);
	i915_user_irq_off(dev_priv);

	if (ret == DRM_ERR(EBUSY)) {
		DRM_ERROR("%s: EBUSY -- rec: %d emitted: %d\n",
			  __FUNCTION__,
			  READ_BREADCRUMB(dev_priv), (int)dev_priv->counter);
	}

	dev_priv->sarea_priv->last_dispatch = READ_BREADCRUMB(dev_priv);
	return ret;
}

/* Needs the lock as it touches the ring.
 */
int i915_irq_emit(DRM_IOCTL_ARGS)
{
	DRM_DEVICE;
	drm_i915_private_t *dev_priv = dev->dev_private;
	drm_i915_irq_emit_t emit;
	int result;

	LOCK_TEST_WITH_RETURN(dev, filp);

	if (!dev_priv) {
		DRM_ERROR("%s called with no initialization\n", __FUNCTION__);
		return DRM_ERR(EINVAL);
	}

	DRM_COPY_FROM_USER_IOCTL(emit, (drm_i915_irq_emit_t __user *) data,
				 sizeof(emit));

	result = i915_emit_irq(dev);

	if (DRM_COPY_TO_USER(emit.irq_seq, &result, sizeof(int))) {
		DRM_ERROR("copy_to_user\n");
		return DRM_ERR(EFAULT);
	}

	return 0;
}

/* Doesn't need the hardware lock.
 */
int i915_irq_wait(DRM_IOCTL_ARGS)
{
	DRM_DEVICE;
	drm_i915_private_t *dev_priv = dev->dev_private;
	drm_i915_irq_wait_t irqwait;

	if (!dev_priv) {
		DRM_ERROR("%s called with no initialization\n", __FUNCTION__);
		return DRM_ERR(EINVAL);
	}

	DRM_COPY_FROM_USER_IOCTL(irqwait, (drm_i915_irq_wait_t __user *) data,
				 sizeof(irqwait));

	return i915_wait_irq(dev, irqwait.irq_seq);
}

int i915_enable_vblank(drm_device_t *dev, int crtc)
{
	drm_i915_private_t *dev_priv = (drm_i915_private_t *) dev->dev_private;

	if (!IS_I965G(dev))
		return 0;

	switch (crtc) {
	case 0:
		dev_priv->irq_enable_reg |= VSYNC_PIPEA_FLAG;
		break;
	case 1:
		dev_priv->irq_enable_reg |= VSYNC_PIPEB_FLAG;
		break;
	default:
		DRM_ERROR("tried to enable vblank on non-existent crtc %d\n",
			  crtc);
		break;
	}

	I915_WRITE16(I915REG_INT_ENABLE_R, dev_priv->irq_enable_reg);

	return 0;
}

void i915_disable_vblank(drm_device_t *dev, int crtc)
{
	drm_i915_private_t *dev_priv = (drm_i915_private_t *) dev->dev_private;

	if (!IS_I965G(dev))
		return;

	switch (crtc) {
	case 0:
		dev_priv->irq_enable_reg &= ~VSYNC_PIPEA_FLAG;
		break;
	case 1:
		dev_priv->irq_enable_reg &= ~VSYNC_PIPEB_FLAG;
		break;
	default:
		DRM_ERROR("tried to disable vblank on non-existent crtc %d\n",
			  crtc);
		break;
	}

	I915_WRITE16(I915REG_INT_ENABLE_R, dev_priv->irq_enable_reg);
}

static void i915_enable_interrupt (drm_device_t *dev)
{
	drm_i915_private_t *dev_priv = (drm_i915_private_t *) dev->dev_private;
	
	dev_priv->irq_enable_reg |= USER_INT_FLAG;

	I915_WRITE16(I915REG_INT_ENABLE_R, dev_priv->irq_enable_reg);
	dev_priv->irq_enabled = 1;
}

/* Set the vblank monitor pipe
 */
int i915_vblank_pipe_set(DRM_IOCTL_ARGS)
{
	DRM_DEVICE;
	drm_i915_private_t *dev_priv = dev->dev_private;
	drm_i915_vblank_pipe_t pipe;

	if (!dev_priv) {
		DRM_ERROR("%s called with no initialization\n", __FUNCTION__);
		return DRM_ERR(EINVAL);
	}

	DRM_COPY_FROM_USER_IOCTL(pipe, (drm_i915_vblank_pipe_t __user *) data,
				 sizeof(pipe));

	if (pipe.pipe & ~(DRM_I915_VBLANK_PIPE_A|DRM_I915_VBLANK_PIPE_B)) {
		DRM_ERROR("%s called with invalid pipe 0x%x\n", 
			  __FUNCTION__, pipe.pipe);
		return DRM_ERR(EINVAL);
	}

	dev_priv->vblank_pipe = pipe.pipe;

	return 0;
}

int i915_vblank_pipe_get(DRM_IOCTL_ARGS)
{
	DRM_DEVICE;
	drm_i915_private_t *dev_priv = dev->dev_private;
	drm_i915_vblank_pipe_t pipe;
	u16 flag;

	if (!dev_priv) {
		DRM_ERROR("%s called with no initialization\n", __FUNCTION__);
		return DRM_ERR(EINVAL);
	}

	flag = I915_READ(I915REG_INT_ENABLE_R);
	pipe.pipe = 0;
	if (flag & VSYNC_PIPEA_FLAG)
		pipe.pipe |= DRM_I915_VBLANK_PIPE_A;
	if (flag & VSYNC_PIPEB_FLAG)
		pipe.pipe |= DRM_I915_VBLANK_PIPE_B;
	DRM_COPY_TO_USER_IOCTL((drm_i915_vblank_pipe_t __user *) data, pipe,
				 sizeof(pipe));
	return 0;
}

/**
 * Schedule buffer swap at given vertical blank.
 */
int i915_vblank_swap(DRM_IOCTL_ARGS)
{
	DRM_DEVICE;
	drm_i915_private_t *dev_priv = dev->dev_private;
	drm_i915_vblank_swap_t swap;
	drm_i915_vbl_swap_t *vbl_swap;
	unsigned int pipe, seqtype, curseq;
	unsigned long irqflags;
	struct list_head *list;
	int ret;

	if (!dev_priv) {
		DRM_ERROR("%s called with no initialization\n", __func__);
		return DRM_ERR(EINVAL);
	}

	if (dev_priv->sarea_priv->rotation) {
		DRM_DEBUG("Rotation not supported\n");
		return DRM_ERR(EINVAL);
	}

	DRM_COPY_FROM_USER_IOCTL(swap, (drm_i915_vblank_swap_t __user *) data,
				 sizeof(swap));

	if (swap.seqtype & ~(_DRM_VBLANK_RELATIVE | _DRM_VBLANK_ABSOLUTE |
			     _DRM_VBLANK_SECONDARY | _DRM_VBLANK_NEXTONMISS |
			     _DRM_VBLANK_FLIP)) {
		DRM_ERROR("Invalid sequence type 0x%x\n", swap.seqtype);
		return DRM_ERR(EINVAL);
	}

	pipe = (swap.seqtype & _DRM_VBLANK_SECONDARY) ? 1 : 0;

	seqtype = swap.seqtype & (_DRM_VBLANK_RELATIVE | _DRM_VBLANK_ABSOLUTE);

	if (!(dev_priv->vblank_pipe & (1 << pipe))) {
		DRM_ERROR("Invalid pipe %d\n", pipe);
		return DRM_ERR(EINVAL);
	}

	spin_lock_irqsave(&dev->drw_lock, irqflags);

	if (!drm_get_drawable_info(dev, swap.drawable)) {
		spin_unlock_irqrestore(&dev->drw_lock, irqflags);
		DRM_DEBUG("Invalid drawable ID %d\n", swap.drawable);
		return DRM_ERR(EINVAL);
	}

	spin_unlock_irqrestore(&dev->drw_lock, irqflags);

	drm_update_vblank_count(dev, pipe);
	curseq = drm_vblank_count(dev, pipe);

	if (seqtype == _DRM_VBLANK_RELATIVE)
		swap.sequence += curseq;

	if ((curseq - swap.sequence) <= (1<<23)) {
		if (swap.seqtype & _DRM_VBLANK_NEXTONMISS) {
			swap.sequence = curseq + 1;
		} else {
			DRM_DEBUG("Missed target sequence\n");
			return DRM_ERR(EINVAL);
		}
	}

	if (swap.seqtype & _DRM_VBLANK_FLIP) {
		swap.sequence--;

		if ((curseq - swap.sequence) <= (1<<23)) {
			drm_drawable_info_t *drw;

			LOCK_TEST_WITH_RETURN(dev, filp);

			spin_lock_irqsave(&dev->drw_lock, irqflags);

			drw = drm_get_drawable_info(dev, swap.drawable);

			if (!drw) {
				spin_unlock_irqrestore(&dev->drw_lock, irqflags);
				DRM_DEBUG("Invalid drawable ID %d\n",
					  swap.drawable);
				return DRM_ERR(EINVAL);
			}

			i915_dispatch_vsync_flip(dev, drw, pipe);

			spin_unlock_irqrestore(&dev->drw_lock, irqflags);

			return 0;
		}
	}

	spin_lock_irqsave(&dev_priv->swaps_lock, irqflags);

	list_for_each(list, &dev_priv->vbl_swaps.head) {
		vbl_swap = list_entry(list, drm_i915_vbl_swap_t, head);

		if (vbl_swap->drw_id == swap.drawable &&
		    vbl_swap->pipe == pipe &&
		    vbl_swap->sequence == swap.sequence) {
			vbl_swap->flip = (swap.seqtype & _DRM_VBLANK_FLIP);
			spin_unlock_irqrestore(&dev_priv->swaps_lock, irqflags);
			DRM_DEBUG("Already scheduled\n");
			return 0;
		}
	}

	spin_unlock_irqrestore(&dev_priv->swaps_lock, irqflags);

	if (dev_priv->swaps_pending >= 100) {
		DRM_DEBUG("Too many swaps queued\n");
		return DRM_ERR(EBUSY);
	}

	vbl_swap = drm_calloc(1, sizeof(*vbl_swap), DRM_MEM_DRIVER);

	if (!vbl_swap) {
		DRM_ERROR("Failed to allocate memory to queue swap\n");
		return DRM_ERR(ENOMEM);
	}

	DRM_DEBUG("\n");

	ret = drm_vblank_get(dev, pipe);
	if (ret) {
		drm_free(vbl_swap, sizeof(*vbl_swap), DRM_MEM_DRIVER);
		return ret;
	}

	vbl_swap->drw_id = swap.drawable;
	vbl_swap->pipe = pipe;
	vbl_swap->sequence = swap.sequence;
	vbl_swap->flip = (swap.seqtype & _DRM_VBLANK_FLIP);

	if (vbl_swap->flip)
		swap.sequence++;

	spin_lock_irqsave(&dev_priv->swaps_lock, irqflags);

	list_add_tail((struct list_head *)vbl_swap, &dev_priv->vbl_swaps.head);
	dev_priv->swaps_pending++;

	spin_unlock_irqrestore(&dev_priv->swaps_lock, irqflags);

	DRM_COPY_TO_USER_IOCTL((drm_i915_vblank_swap_t __user *) data, swap,
			       sizeof(swap));

	return 0;
}

/* drm_dma.h hooks
*/
void i915_driver_irq_preinstall(drm_device_t * dev)
{
	drm_i915_private_t *dev_priv = (drm_i915_private_t *) dev->dev_private;

	I915_WRITE16(I915REG_HWSTAM, 0xeffe);
	I915_WRITE16(I915REG_INT_MASK_R, 0x0);
	I915_WRITE16(I915REG_INT_ENABLE_R, 0x0);
}

int i915_driver_irq_postinstall(drm_device_t * dev)
{
	drm_i915_private_t *dev_priv = (drm_i915_private_t *) dev->dev_private;
	int ret, num_pipes = 2;

	spin_lock_init(&dev_priv->swaps_lock);
	INIT_LIST_HEAD(&dev_priv->vbl_swaps.head);
	dev_priv->swaps_pending = 0;

	dev_priv->user_irq_lock = SPIN_LOCK_UNLOCKED;
	dev_priv->user_irq_refcount = 0;
	dev_priv->irq_enable_reg = 0;

	ret = drm_vblank_init(dev, num_pipes);
	if (ret)
		return ret;

	dev->max_vblank_count = 0xffffff; /* only 24 bits of frame count */

	if (!IS_I965G(dev)) {
		dev_priv->irq_enable_reg |= VSYNC_PIPEA_FLAG | VSYNC_PIPEB_FLAG;
		I915_WRITE16(I915REG_INT_ENABLE_R, dev_priv->irq_enable_reg);
	}

	i915_enable_interrupt(dev);
	DRM_INIT_WAITQUEUE(&dev_priv->irq_queue);

	/*
	 * Initialize the hardware status page IRQ location.
	 */

	I915_WRITE(I915REG_INSTPM, ( 1 << 5) | ( 1 << 21));
	return 0;
}

void i915_driver_irq_uninstall(drm_device_t * dev)
{
	drm_i915_private_t *dev_priv = (drm_i915_private_t *) dev->dev_private;
	u16 temp;
	if (!dev_priv)
		return;

	dev_priv->irq_enabled = 0;
	I915_WRITE16(I915REG_HWSTAM, 0xffff);
	I915_WRITE16(I915REG_INT_MASK_R, 0xffff);
	I915_WRITE16(I915REG_INT_ENABLE_R, 0x0);

	temp = I915_READ16(I915REG_INT_IDENTITY_R);
	I915_WRITE16(I915REG_INT_IDENTITY_R, temp);
}
