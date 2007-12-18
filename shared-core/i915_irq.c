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

#include "intel_drv.h"

#define USER_INT_FLAG (1<<1)
#define VSYNC_PIPEB_FLAG (1<<5)
#define VSYNC_PIPEA_FLAG (1<<7)
#define HOTPLUG_FLAG (1 << 17)

#define MAX_NOPID ((u32)~0)

/**
 * i915_get_pipe - return the the pipe associated with a given plane
 * @dev: DRM device
 * @plane: plane to look for
 *
 * We need to get the pipe associated with a given plane to correctly perform
 * vblank driven swapping, and they may not always be equal.  So look up the
 * pipe associated with @plane here.
 */
static int
i915_get_pipe(struct drm_device *dev, int plane)
{
	struct drm_i915_private *dev_priv = (struct drm_i915_private *) dev->dev_private;
	u32 dspcntr;

	dspcntr = plane ? I915_READ(DSPBCNTR) : I915_READ(DSPACNTR);

	return dspcntr & DISPPLANE_SEL_PIPE_MASK ? 1 : 0;
}

/**
 * Emit a synchronous flip.
 *
 * This function must be called with the drawable spinlock held.
 */
static void
i915_dispatch_vsync_flip(struct drm_device *dev, struct drm_drawable_info *drw,
			 int plane)
{
	struct drm_i915_private *dev_priv = (struct drm_i915_private *) dev->dev_private;
	struct drm_i915_sarea *sarea_priv = dev_priv->sarea_priv;
	u16 x1, y1, x2, y2;
	int pf_planes = 1 << plane;

	DRM_SPINLOCK_ASSERT(&dev->drw_lock);

	/* If the window is visible on the other plane, we have to flip on that
	 * plane as well.
	 */
	if (plane == 1) {
		x1 = sarea_priv->planeA_x;
		y1 = sarea_priv->planeA_y;
		x2 = x1 + sarea_priv->planeA_w;
		y2 = y1 + sarea_priv->planeA_h;
	} else {
		x1 = sarea_priv->planeB_x;
		y1 = sarea_priv->planeB_y;
		x2 = x1 + sarea_priv->planeB_w;
		y2 = y1 + sarea_priv->planeB_h;
	}

	if (x2 > 0 && y2 > 0) {
		int i, num_rects = drw->num_rects;
		struct drm_clip_rect *rect = drw->rects;

		for (i = 0; i < num_rects; i++)
			if (!(rect[i].x1 >= x2 || rect[i].y1 >= y2 ||
			      rect[i].x2 <= x1 || rect[i].y2 <= y1)) {
				pf_planes = 0x3;

				break;
			}
	}

	i915_dispatch_flip(dev, pf_planes, 1);
}

/**
 * Emit blits for scheduled buffer swaps.
 *
 * This function will be called with the HW lock held.
 */
static void i915_vblank_tasklet(struct drm_device *dev)
{
	struct drm_i915_private *dev_priv = (struct drm_i915_private *) dev->dev_private;
	struct list_head *list, *tmp, hits, *hit;
	int nhits, nrects, slice[2], upper[2], lower[2], i, num_pages;
	unsigned counter[2] = { atomic_read(&dev->vbl_received),
				atomic_read(&dev->vbl_received2) };
	struct drm_drawable_info *drw;
	struct drm_i915_sarea *sarea_priv = dev_priv->sarea_priv;
	u32 cpp = dev_priv->cpp,  offsets[3];
	u32 cmd = (cpp == 4) ? (XY_SRC_COPY_BLT_CMD |
				XY_SRC_COPY_BLT_WRITE_ALPHA |
				XY_SRC_COPY_BLT_WRITE_RGB)
			     : XY_SRC_COPY_BLT_CMD;
	u32 pitchropcpp = (sarea_priv->pitch * cpp) | (0xcc << 16) |
			  (cpp << 23) | (1 << 24);
	RING_LOCALS;

	DRM_DEBUG("\n");

	INIT_LIST_HEAD(&hits);

	nhits = nrects = 0;

	/* No irqsave/restore necessary.  This tasklet may be run in an
	 * interrupt context or normal context, but we don't have to worry
	 * about getting interrupted by something acquiring the lock, because
	 * we are the interrupt context thing that acquires the lock.
	 */
	DRM_SPINLOCK(&dev_priv->swaps_lock);

	/* Find buffer swaps scheduled for this vertical blank */
	list_for_each_safe(list, tmp, &dev_priv->vbl_swaps.head) {
		struct drm_i915_vbl_swap *vbl_swap =
			list_entry(list, struct drm_i915_vbl_swap, head);
		int pipe = i915_get_pipe(dev, vbl_swap->plane);

		if ((counter[pipe] - vbl_swap->sequence) > (1<<23))
			continue;

		list_del(list);
		dev_priv->swaps_pending--;

		DRM_SPINUNLOCK(&dev_priv->swaps_lock);
		DRM_SPINLOCK(&dev->drw_lock);

		drw = drm_get_drawable_info(dev, vbl_swap->drw_id);

		if (!drw) {
			DRM_SPINUNLOCK(&dev->drw_lock);
			drm_free(vbl_swap, sizeof(*vbl_swap), DRM_MEM_DRIVER);
			DRM_SPINLOCK(&dev_priv->swaps_lock);
			continue;
		}

		list_for_each(hit, &hits) {
			struct drm_i915_vbl_swap *swap_cmp =
				list_entry(hit, struct drm_i915_vbl_swap, head);
			struct drm_drawable_info *drw_cmp =
				drm_get_drawable_info(dev, swap_cmp->drw_id);

			if (drw_cmp &&
			    drw_cmp->rects[0].y1 > drw->rects[0].y1) {
				list_add_tail(list, hit);
				break;
			}
		}

		DRM_SPINUNLOCK(&dev->drw_lock);

		/* List of hits was empty, or we reached the end of it */
		if (hit == &hits)
			list_add_tail(list, hits.prev);

		nhits++;

		DRM_SPINLOCK(&dev_priv->swaps_lock);
	}

	DRM_SPINUNLOCK(&dev_priv->swaps_lock);

	if (nhits == 0) {
		return;
	}

	i915_kernel_lost_context(dev);

	upper[0] = upper[1] = 0;
	slice[0] = max(sarea_priv->planeA_h / nhits, 1);
	slice[1] = max(sarea_priv->planeB_h / nhits, 1);
	lower[0] = sarea_priv->planeA_y + slice[0];
	lower[1] = sarea_priv->planeB_y + slice[0];

	offsets[0] = sarea_priv->front_offset;
	offsets[1] = sarea_priv->back_offset;
	offsets[2] = sarea_priv->third_offset;
	num_pages = sarea_priv->third_handle ? 3 : 2;

	DRM_SPINLOCK(&dev->drw_lock);

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
			struct drm_i915_vbl_swap *swap_hit =
				list_entry(hit, struct drm_i915_vbl_swap, head);
			struct drm_clip_rect *rect;
			int num_rects, plane, front, back;
			unsigned short top, bottom;

			drw = drm_get_drawable_info(dev, swap_hit->drw_id);

			if (!drw)
				continue;

			plane = swap_hit->plane;

			if (swap_hit->flip) {
				i915_dispatch_vsync_flip(dev, drw, plane);
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
			top = upper[plane];
			bottom = lower[plane];

			front = (dev_priv->sarea_priv->pf_current_page >>
				 (2 * plane)) & 0x3;
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

	DRM_SPINUNLOCK(&dev->drw_lock);

	list_for_each_safe(hit, tmp, &hits) {
		struct drm_i915_vbl_swap *swap_hit =
			list_entry(hit, struct drm_i915_vbl_swap, head);

		list_del(hit);

		drm_free(swap_hit, sizeof(*swap_hit), DRM_MEM_DRIVER);
	}
}

#define HOTPLUG_CMD_CRT 1
#define HOTPLUG_CMD_SDVOB 4
#define HOTPLUG_CMD_SDVOC 8

static struct drm_device *hotplug_dev;
static int hotplug_cmd = 0;
static spinlock_t hotplug_lock = SPIN_LOCK_UNLOCKED;

static void i915_hotplug_crt(struct drm_device *dev)
{
	struct drm_output *output;
	struct intel_output *iout;

	mutex_lock(&dev->mode_config.mutex);

	/* find the crt output */
	list_for_each_entry(output, &dev->mode_config.output_list, head) {
		iout = output->driver_private;
		if (iout->type == INTEL_OUTPUT_ANALOG)
			break;
		else
			iout = 0;
	}

	if (iout == 0)
		goto unlock;

	drm_hotplug_stage_two(dev, output);

unlock:
	mutex_unlock(&dev->mode_config.mutex);
}

static void i915_hotplug_sdvo(struct drm_device *dev, int sdvoB)
{
	struct drm_output *output = 0;
	enum drm_output_status status;

	mutex_lock(&dev->mode_config.mutex);

	output = intel_sdvo_find(dev, sdvoB);

	if (!output) {
		DRM_ERROR("could not find sdvo%s output\n", sdvoB ? "B" : "C");
		goto unlock;
	}

	status = output->funcs->detect(output);

	if (status != output_status_connected)
		DRM_DEBUG("disconnect or unkown we don't do anything then\n");
	else
		drm_hotplug_stage_two(dev, output);

	/* wierd hw bug, sdvo stop sending interupts */
	intel_sdvo_set_hotplug(output, 1);

unlock:
	mutex_unlock(&dev->mode_config.mutex);
}
/*
 * This code is called in a more safe envirmoent to handle the hotplugs.
 * Add code here for hotplug love to userspace.
 */
static void i915_hotplug_work_func(struct work_struct *work)
{
	struct drm_device *dev = hotplug_dev;
	int crt;
	int sdvoB;
	int sdvoC;

	spin_lock(&hotplug_lock);
	crt = hotplug_cmd & HOTPLUG_CMD_CRT;
	sdvoB = hotplug_cmd & HOTPLUG_CMD_SDVOB;
	sdvoC = hotplug_cmd & HOTPLUG_CMD_SDVOC;
	hotplug_cmd = 0;
	spin_unlock(&hotplug_lock);

	if (crt)
		i915_hotplug_crt(dev);

	if (sdvoB)
		i915_hotplug_sdvo(dev, 1);

	if (sdvoC)
		i915_hotplug_sdvo(dev, 0);

}

static int i915_run_hotplug_tasklet(struct drm_device *dev, uint32_t stat)
{
	static DECLARE_WORK(hotplug, i915_hotplug_work_func);
	struct drm_i915_private *dev_priv = dev->dev_private;

	hotplug_dev = dev;

	if (stat & CRT_HOTPLUG_INT_STATUS) {
		DRM_DEBUG("CRT event\n");

		if (stat & CRT_HOTPLUG_MONITOR_MASK) {
			spin_lock(&hotplug_lock);
			hotplug_cmd |= HOTPLUG_CMD_CRT;
			spin_unlock(&hotplug_lock);
		} else {
			/* handle crt disconnects */
		}
	}

	if (stat & SDVOB_HOTPLUG_INT_STATUS) {
		DRM_DEBUG("sDVOB event\n");

		spin_lock(&hotplug_lock);
		hotplug_cmd |= HOTPLUG_CMD_SDVOB;
		spin_unlock(&hotplug_lock);
	}

	if (stat & SDVOC_HOTPLUG_INT_STATUS) {
		DRM_DEBUG("sDVOC event\n");

		spin_lock(&hotplug_lock);
		hotplug_cmd |= HOTPLUG_CMD_SDVOC;
		spin_unlock(&hotplug_lock);
	}

	queue_work(dev_priv->wq, &hotplug);

	return 0;
}

irqreturn_t i915_driver_irq_handler(DRM_IRQ_ARGS)
{
	struct drm_device *dev = (struct drm_device *) arg;
	struct drm_i915_private *dev_priv = (struct drm_i915_private *) dev->dev_private;
	u32 temp = 0;
	u32 temp2;
	u32 pipea_stats, pipeb_stats;

	pipea_stats = I915_READ(I915REG_PIPEASTAT);
	pipeb_stats = I915_READ(I915REG_PIPEBSTAT);

	/* On i8xx hw the IIR and IER are 16bit on i9xx its 32bit */
	if (IS_I9XX(dev)) {
		temp = I915_READ(I915REG_INT_IDENTITY_R);
	} else {
		temp = I915_READ16(I915REG_INT_IDENTITY_R);
	}

	temp2 = temp;
	temp &= (dev_priv->irq_enable_reg | USER_INT_FLAG);

#if 0
	/* ugly despamification of pipeb event irq */
	if (temp & (0xFFFFFFF ^ ((1 << 5) | (1 << 7)))) {
		DRM_DEBUG("IIR %08x\n", temp2);
		DRM_DEBUG("MSK %08x\n", dev_priv->irq_enable_reg | USER_INT_FLAG);
		DRM_DEBUG("M&I %08x\n", temp);
		DRM_DEBUG("HOT %08x\n", I915_READ(PORT_HOTPLUG_STAT));
	}
#else
#if 0
	DRM_DEBUG("%s flag=%08x\n", __FUNCTION__, temp);
#endif
#endif

	if (temp == 0)
		return IRQ_NONE;

	if (IS_I9XX(dev)) {
		I915_WRITE(I915REG_INT_IDENTITY_R, temp);
		(void) I915_READ(I915REG_INT_IDENTITY_R);
	} else {
		I915_WRITE16(I915REG_INT_IDENTITY_R, temp);
		(void) I915_READ16(I915REG_INT_IDENTITY_R);
	}

	DRM_READMEMORYBARRIER();

	dev_priv->sarea_priv->last_dispatch = READ_BREADCRUMB(dev_priv);

	if (temp & USER_INT_FLAG) {
		DRM_WAKEUP(&dev_priv->irq_queue);
#ifdef I915_HAVE_FENCE
		i915_fence_handler(dev);
#endif
	}

	if (temp & (VSYNC_PIPEA_FLAG | VSYNC_PIPEB_FLAG)) {
		int vblank_pipe = dev_priv->vblank_pipe;

		if ((vblank_pipe &
		     (DRM_I915_VBLANK_PIPE_A | DRM_I915_VBLANK_PIPE_B))
		    == (DRM_I915_VBLANK_PIPE_A | DRM_I915_VBLANK_PIPE_B)) {
			if (temp & VSYNC_PIPEA_FLAG)
				atomic_inc(&dev->vbl_received);
			if (temp & VSYNC_PIPEB_FLAG)
				atomic_inc(&dev->vbl_received2);
		} else if (((temp & VSYNC_PIPEA_FLAG) &&
			    (vblank_pipe & DRM_I915_VBLANK_PIPE_A)) ||
			   ((temp & VSYNC_PIPEB_FLAG) &&
			    (vblank_pipe & DRM_I915_VBLANK_PIPE_B)))
			atomic_inc(&dev->vbl_received);

		DRM_WAKEUP(&dev->vbl_queue);
		drm_vbl_send_signals(dev);

		if (dev_priv->swaps_pending > 0)
			drm_locked_tasklet(dev, i915_vblank_tasklet);
		I915_WRITE(I915REG_PIPEASTAT,
			pipea_stats|I915_VBLANK_INTERRUPT_ENABLE|
			I915_VBLANK_CLEAR);
		I915_WRITE(I915REG_PIPEBSTAT,
			pipeb_stats|I915_VBLANK_INTERRUPT_ENABLE|
			I915_VBLANK_CLEAR);
	}

	/* for now lest just ack it */
	if (temp & (1 << 17)) {
		DRM_DEBUG("Hotplug event received\n");

		temp2 = I915_READ(PORT_HOTPLUG_STAT);

		i915_run_hotplug_tasklet(dev, temp2);

		I915_WRITE(PORT_HOTPLUG_STAT,temp2);
	}

	return IRQ_HANDLED;
}

int i915_emit_irq(struct drm_device *dev)
{
	
	struct drm_i915_private *dev_priv = dev->dev_private;
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

void i915_user_irq_on(struct drm_i915_private *dev_priv)
{
	DRM_SPINLOCK(&dev_priv->user_irq_lock);
	if (dev_priv->irq_enabled && (++dev_priv->user_irq_refcount == 1)){
		dev_priv->irq_enable_reg |= USER_INT_FLAG;
		I915_WRITE16(I915REG_INT_ENABLE_R, dev_priv->irq_enable_reg);
	}
	DRM_SPINUNLOCK(&dev_priv->user_irq_lock);

}
		
void i915_user_irq_off(struct drm_i915_private *dev_priv)
{
	DRM_SPINLOCK(&dev_priv->user_irq_lock);
	if (dev_priv->irq_enabled && (--dev_priv->user_irq_refcount == 0)) {
		//		dev_priv->irq_enable_reg &= ~USER_INT_FLAG;
		//		I915_WRITE16(I915REG_INT_ENABLE_R, dev_priv->irq_enable_reg);
	}
	DRM_SPINUNLOCK(&dev_priv->user_irq_lock);
}


static int i915_wait_irq(struct drm_device * dev, int irq_nr)
{
	struct drm_i915_private *dev_priv = (struct drm_i915_private *) dev->dev_private;
	int ret = 0;

	DRM_DEBUG("%s irq_nr=%d breadcrumb=%d\n", __FUNCTION__, irq_nr,
		  READ_BREADCRUMB(dev_priv));

	if (READ_BREADCRUMB(dev_priv) >= irq_nr)
		return 0;

	i915_user_irq_on(dev_priv);
	DRM_WAIT_ON(ret, dev_priv->irq_queue, 3 * DRM_HZ,
		    READ_BREADCRUMB(dev_priv) >= irq_nr);
	i915_user_irq_off(dev_priv);

	if (ret == -EBUSY) {
		DRM_ERROR("%s: EBUSY -- rec: %d emitted: %d\n",
			  __FUNCTION__,
			  READ_BREADCRUMB(dev_priv), (int)dev_priv->counter);
	}

	dev_priv->sarea_priv->last_dispatch = READ_BREADCRUMB(dev_priv);
	return ret;
}

static int i915_driver_vblank_do_wait(struct drm_device *dev,
				      unsigned int *sequence,
				      atomic_t *counter)
{
	struct drm_i915_private *dev_priv = dev->dev_private;
	unsigned int cur_vblank;
	int ret = 0;

	if (!dev_priv) {
		DRM_ERROR("%s called with no initialization\n", __FUNCTION__);
		return -EINVAL;
	}

	DRM_WAIT_ON(ret, dev->vbl_queue, 3 * DRM_HZ,
		    (((cur_vblank = atomic_read(counter))
			- *sequence) <= (1<<23)));

	*sequence = cur_vblank;

	return ret;
}

void i915_driver_wait_next_vblank(struct drm_device *dev, int pipe)
{
	unsigned int seq;

	seq = pipe ? atomic_read(&dev->vbl_received2) + 1 :
		atomic_read(&dev->vbl_received) + 1;

	if (!pipe)
		i915_driver_vblank_do_wait(dev, &seq, &dev->vbl_received);
	else
		i915_driver_vblank_do_wait(dev, &seq, &dev->vbl_received2);
}

int i915_driver_vblank_wait(struct drm_device *dev, unsigned int *sequence)
{
	atomic_t *counter;

	if (i915_get_pipe(dev, 0) == 0)
		counter = &dev->vbl_received;
	else
		counter = &dev->vbl_received2;
	return i915_driver_vblank_do_wait(dev, sequence, counter);
}

int i915_driver_vblank_wait2(struct drm_device *dev, unsigned int *sequence)
{
	atomic_t *counter;

	if (i915_get_pipe(dev, 1) == 0)
		counter = &dev->vbl_received;
	else
		counter = &dev->vbl_received2;

	return i915_driver_vblank_do_wait(dev, sequence, counter);
}

/* Needs the lock as it touches the ring.
 */
int i915_irq_emit(struct drm_device *dev, void *data,
			 struct drm_file *file_priv)
{
	struct drm_i915_private *dev_priv = dev->dev_private;
	struct drm_i915_irq_emit *emit = data;
	int result;

	LOCK_TEST_WITH_RETURN(dev, file_priv);

	if (!dev_priv) {
		DRM_ERROR("%s called with no initialization\n", __FUNCTION__);
		return -EINVAL;
	}

	result = i915_emit_irq(dev);

	if (DRM_COPY_TO_USER(emit->irq_seq, &result, sizeof(int))) {
		DRM_ERROR("copy_to_user\n");
		return -EFAULT;
	}

	return 0;
}

/* Doesn't need the hardware lock.
 */
int i915_irq_wait(struct drm_device *dev, void *data,
		  struct drm_file *file_priv)
{
	struct drm_i915_private *dev_priv = dev->dev_private;
	struct drm_i915_irq_wait *irqwait = data;

	if (!dev_priv) {
		DRM_ERROR("%s called with no initialization\n", __FUNCTION__);
		return -EINVAL;
	}

	return i915_wait_irq(dev, irqwait->irq_seq);
}

void i915_enable_interrupt (struct drm_device *dev)
{
	struct drm_i915_private *dev_priv = (struct drm_i915_private *) dev->dev_private;
	struct drm_output *o;
	
	dev_priv->irq_enable_reg = USER_INT_FLAG; 

	if (dev_priv->vblank_pipe & DRM_I915_VBLANK_PIPE_A)
		dev_priv->irq_enable_reg |= VSYNC_PIPEA_FLAG;
	if (dev_priv->vblank_pipe & DRM_I915_VBLANK_PIPE_B)
		dev_priv->irq_enable_reg |= VSYNC_PIPEB_FLAG;

	if (IS_I9XX(dev) && dev->mode_config.num_output) {
		dev_priv->irq_enable_reg |= HOTPLUG_FLAG;

		/* Activate the CRT */
		I915_WRITE(PORT_HOTPLUG_EN, CRT_HOTPLUG_INT_EN);

		/* SDVOB */
		o = intel_sdvo_find(dev, 1);
		if (o && intel_sdvo_supports_hotplug(o)) {
			intel_sdvo_set_hotplug(o, 1);
			I915_WRITE(PORT_HOTPLUG_EN, SDVOB_HOTPLUG_INT_EN);
		}

		/* SDVOC */
		o = intel_sdvo_find(dev, 0);
		if (o && intel_sdvo_supports_hotplug(o)) {
			intel_sdvo_set_hotplug(o, 1);
			I915_WRITE(PORT_HOTPLUG_EN, SDVOC_HOTPLUG_INT_EN);
		}

	}

	if (IS_I9XX(dev)) {
		I915_WRITE(I915REG_INT_ENABLE_R, dev_priv->irq_enable_reg);
	} else {
		I915_WRITE16(I915REG_INT_ENABLE_R, dev_priv->irq_enable_reg);
	}

	DRM_DEBUG("HEN %08x\n",I915_READ(PORT_HOTPLUG_EN));
	DRM_DEBUG("HST %08x\n",I915_READ(PORT_HOTPLUG_STAT));
	DRM_DEBUG("IER %08x\n",I915_READ(I915REG_INT_ENABLE_R));
	DRM_DEBUG("SDB %08x\n",I915_READ(SDVOB));

	I915_WRITE(PORT_HOTPLUG_STAT, I915_READ(PORT_HOTPLUG_STAT));

	dev_priv->irq_enabled = 1;
}

/* Set the vblank monitor pipe
 */
int i915_vblank_pipe_set(struct drm_device *dev, void *data,
			 struct drm_file *file_priv)
{
	struct drm_i915_private *dev_priv = dev->dev_private;
	struct drm_i915_vblank_pipe *pipe = data;

	if (!dev_priv) {
		DRM_ERROR("%s called with no initialization\n", __FUNCTION__);
		return -EINVAL;
	}

	if (pipe->pipe & ~(DRM_I915_VBLANK_PIPE_A|DRM_I915_VBLANK_PIPE_B)) {
		DRM_ERROR("%s called with invalid pipe 0x%x\n",
			  __FUNCTION__, pipe->pipe);
		return -EINVAL;
	}

	dev_priv->vblank_pipe = pipe->pipe;

	i915_enable_interrupt (dev);

	return 0;
}

int i915_vblank_pipe_get(struct drm_device *dev, void *data,
			 struct drm_file *file_priv)
{
	struct drm_i915_private *dev_priv = dev->dev_private;
	struct drm_i915_vblank_pipe *pipe = data;
	u16 flag;

	if (!dev_priv) {
		DRM_ERROR("%s called with no initialization\n", __FUNCTION__);
		return -EINVAL;
	}

	flag = I915_READ(I915REG_INT_ENABLE_R);
	pipe->pipe = 0;
	if (flag & VSYNC_PIPEA_FLAG)
		pipe->pipe |= DRM_I915_VBLANK_PIPE_A;
	if (flag & VSYNC_PIPEB_FLAG)
		pipe->pipe |= DRM_I915_VBLANK_PIPE_B;

	return 0;
}

/**
 * Schedule buffer swap at given vertical blank.
 */
int i915_vblank_swap(struct drm_device *dev, void *data,
		     struct drm_file *file_priv)
{
	struct drm_i915_private *dev_priv = dev->dev_private;
	struct drm_i915_vblank_swap *swap = data;
	struct drm_i915_vbl_swap *vbl_swap;
	unsigned int pipe, seqtype, curseq, plane;
	unsigned long irqflags;
	struct list_head *list;

	if (!dev_priv) {
		DRM_ERROR("%s called with no initialization\n", __func__);
		return -EINVAL;
	}

	if (dev_priv->sarea_priv->rotation) {
		DRM_DEBUG("Rotation not supported\n");
		return -EINVAL;
	}

	if (swap->seqtype & ~(_DRM_VBLANK_RELATIVE | _DRM_VBLANK_ABSOLUTE |
			     _DRM_VBLANK_SECONDARY | _DRM_VBLANK_NEXTONMISS |
			     _DRM_VBLANK_FLIP)) {
		DRM_ERROR("Invalid sequence type 0x%x\n", swap->seqtype);
		return -EINVAL;
	}

	plane = (swap->seqtype & _DRM_VBLANK_SECONDARY) ? 1 : 0;
	pipe = i915_get_pipe(dev, plane);

	seqtype = swap->seqtype & (_DRM_VBLANK_RELATIVE | _DRM_VBLANK_ABSOLUTE);

	if (!(dev_priv->vblank_pipe & (1 << pipe))) {
		DRM_ERROR("Invalid pipe %d\n", pipe);
		return -EINVAL;
	}

	DRM_SPINLOCK_IRQSAVE(&dev->drw_lock, irqflags);

	/* It makes no sense to schedule a swap for a drawable that doesn't have
	 * valid information at this point. E.g. this could mean that the X
	 * server is too old to push drawable information to the DRM, in which
	 * case all such swaps would become ineffective.
	 */
	if (!drm_get_drawable_info(dev, swap->drawable)) {
		DRM_SPINUNLOCK_IRQRESTORE(&dev->drw_lock, irqflags);
		DRM_DEBUG("Invalid drawable ID %d\n", swap->drawable);
		return -EINVAL;
	}

	DRM_SPINUNLOCK_IRQRESTORE(&dev->drw_lock, irqflags);

	curseq = atomic_read(pipe ? &dev->vbl_received2 : &dev->vbl_received);

	if (seqtype == _DRM_VBLANK_RELATIVE)
		swap->sequence += curseq;

	if ((curseq - swap->sequence) <= (1<<23)) {
		if (swap->seqtype & _DRM_VBLANK_NEXTONMISS) {
			swap->sequence = curseq + 1;
		} else {
			DRM_DEBUG("Missed target sequence\n");
			return -EINVAL;
		}
	}

	if (swap->seqtype & _DRM_VBLANK_FLIP) {
		swap->sequence--;

		if ((curseq - swap->sequence) <= (1<<23)) {
			struct drm_drawable_info *drw;

			LOCK_TEST_WITH_RETURN(dev, file_priv);

			DRM_SPINLOCK_IRQSAVE(&dev->drw_lock, irqflags);

			drw = drm_get_drawable_info(dev, swap->drawable);

			if (!drw) {
				DRM_SPINUNLOCK_IRQRESTORE(&dev->drw_lock,
				    irqflags);
				DRM_DEBUG("Invalid drawable ID %d\n",
					  swap->drawable);
				return -EINVAL;
			}

			i915_dispatch_vsync_flip(dev, drw, plane);

			DRM_SPINUNLOCK_IRQRESTORE(&dev->drw_lock, irqflags);

			return 0;
		}
	}

	DRM_SPINLOCK_IRQSAVE(&dev_priv->swaps_lock, irqflags);

	list_for_each(list, &dev_priv->vbl_swaps.head) {
		vbl_swap = list_entry(list, struct drm_i915_vbl_swap, head);

		if (vbl_swap->drw_id == swap->drawable &&
		    vbl_swap->plane == plane &&
		    vbl_swap->sequence == swap->sequence) {
			vbl_swap->flip = (swap->seqtype & _DRM_VBLANK_FLIP);
			DRM_SPINUNLOCK_IRQRESTORE(&dev_priv->swaps_lock, irqflags);
			DRM_DEBUG("Already scheduled\n");
			return 0;
		}
	}

	DRM_SPINUNLOCK_IRQRESTORE(&dev_priv->swaps_lock, irqflags);

	if (dev_priv->swaps_pending >= 100) {
		DRM_DEBUG("Too many swaps queued\n");
		return -EBUSY;
	}

	vbl_swap = drm_calloc(1, sizeof(*vbl_swap), DRM_MEM_DRIVER);

	if (!vbl_swap) {
		DRM_ERROR("Failed to allocate memory to queue swap\n");
		return -ENOMEM;
	}

	DRM_DEBUG("\n");

	vbl_swap->drw_id = swap->drawable;
	vbl_swap->plane = plane;
	vbl_swap->sequence = swap->sequence;
	vbl_swap->flip = (swap->seqtype & _DRM_VBLANK_FLIP);

	if (vbl_swap->flip)
		swap->sequence++;

	DRM_SPINLOCK_IRQSAVE(&dev_priv->swaps_lock, irqflags);

	list_add_tail((struct list_head *)vbl_swap, &dev_priv->vbl_swaps.head);
	dev_priv->swaps_pending++;

	DRM_SPINUNLOCK_IRQRESTORE(&dev_priv->swaps_lock, irqflags);

	return 0;
}

/* drm_dma.h hooks
*/
void i915_driver_irq_preinstall(struct drm_device * dev)
{
	struct drm_i915_private *dev_priv = (struct drm_i915_private *) dev->dev_private;

	I915_WRITE16(I915REG_HWSTAM, 0xeffe);
	if (IS_I9XX(dev)) {
		I915_WRITE(I915REG_INT_MASK_R, 0x0);
		I915_WRITE(I915REG_INT_ENABLE_R, 0x0);
	} else {
		I915_WRITE16(I915REG_INT_MASK_R, 0x0);
		I915_WRITE16(I915REG_INT_ENABLE_R, 0x0);
	}

}

void i915_driver_irq_postinstall(struct drm_device * dev)
{
	struct drm_i915_private *dev_priv = (struct drm_i915_private *) dev->dev_private;

	DRM_SPININIT(&dev_priv->swaps_lock, "swap");
	INIT_LIST_HEAD(&dev_priv->vbl_swaps.head);
	dev_priv->swaps_pending = 0;

	DRM_SPININIT(&dev_priv->user_irq_lock, "userirq");
	dev_priv->user_irq_refcount = 0;

	i915_enable_interrupt(dev);
	DRM_INIT_WAITQUEUE(&dev_priv->irq_queue);

	/*
	 * Initialize the hardware status page IRQ location.
	 */

	I915_WRITE(I915REG_INSTPM, (1 << 5) | (1 << 21));
}

void i915_driver_irq_uninstall(struct drm_device * dev)
{
	struct drm_i915_private *dev_priv = (struct drm_i915_private *) dev->dev_private;
	u32 temp;

	if (!dev_priv)
		return;

	dev_priv->irq_enabled = 0;


	if(IS_I9XX(dev)) {
		I915_WRITE(I915REG_HWSTAM, 0xffffffff);
		I915_WRITE(I915REG_INT_MASK_R, 0xffffffff);
		I915_WRITE(I915REG_INT_ENABLE_R, 0x0);

		temp = I915_READ(I915REG_INT_IDENTITY_R);
		I915_WRITE(I915REG_INT_IDENTITY_R, temp);
	} else {
		I915_WRITE16(I915REG_HWSTAM, 0xffff);
		I915_WRITE16(I915REG_INT_MASK_R, 0xffff);
		I915_WRITE16(I915REG_INT_ENABLE_R, 0x0);

		temp = I915_READ16(I915REG_INT_IDENTITY_R);
		I915_WRITE16(I915REG_INT_IDENTITY_R, temp);
	}
}
