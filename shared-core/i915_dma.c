/* i915_dma.c -- DMA support for the I915 -*- linux-c -*-
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

/* Really want an OS-independent resettable timer.  Would like to have
 * this loop run for (eg) 3 sec, but have the timer reset every time
 * the head pointer changes, so that EBUSY only happens if the ring
 * actually stalls for (eg) 3 seconds.
 */
int i915_wait_ring(struct drm_device * dev, int n, const char *caller)
{
	struct drm_i915_private *dev_priv = dev->dev_private;
	struct drm_i915_ring_buffer *ring = &(dev_priv->ring);
	u32 last_head = I915_READ(LP_RING + RING_HEAD) & HEAD_ADDR;
	int i;

	for (i = 0; i < 10000; i++) {
		ring->head = I915_READ(LP_RING + RING_HEAD) & HEAD_ADDR;
		ring->space = ring->head - (ring->tail + 8);
		if (ring->space < 0)
			ring->space += ring->Size;
		if (ring->space >= n)
			return 0;

		if (ring->head != last_head)
			i = 0;

		last_head = ring->head;
		DRM_UDELAY(1);
	}

	return -EBUSY;
}

void i915_kernel_lost_context(struct drm_device * dev)
{
	struct drm_i915_private *dev_priv = dev->dev_private;
	struct drm_i915_ring_buffer *ring = &(dev_priv->ring);

	ring->head = I915_READ(LP_RING + RING_HEAD) & HEAD_ADDR;
	ring->tail = I915_READ(LP_RING + RING_TAIL) & TAIL_ADDR;
	ring->space = ring->head - (ring->tail + 8);
	if (ring->space < 0)
		ring->space += ring->Size;
}

int i915_dma_cleanup(struct drm_device * dev)
{
	/* Make sure interrupts are disabled here because the uninstall ioctl
	 * may not have been called from userspace and after dev_private
	 * is freed, it's too late.
	 */
	if (dev->irq)
		drm_irq_uninstall(dev);

	return 0;
}


#define DRI2_SAREA_BLOCK_TYPE(b) ((b) >> 16)
#define DRI2_SAREA_BLOCK_SIZE(b) ((b) & 0xffff)
#define DRI2_SAREA_BLOCK_NEXT(p)				\
	((void *) ((unsigned char *) (p) +			\
		   DRI2_SAREA_BLOCK_SIZE(*(unsigned int *) p)))

#define DRI2_SAREA_BLOCK_END		0x0000
#define DRI2_SAREA_BLOCK_LOCK		0x0001
#define DRI2_SAREA_BLOCK_EVENT_BUFFER	0x0002

static int
setup_dri2_sarea(struct drm_device * dev,
		 struct drm_file *file_priv,
		 drm_i915_init_t * init)
{
	struct drm_i915_private *dev_priv = dev->dev_private;
	int ret;
	unsigned int *p, *end, *next;

	mutex_lock(&dev->struct_mutex);
	dev_priv->sarea_bo =
		drm_lookup_buffer_object(file_priv,
					 init->sarea_handle, 1);
	mutex_unlock(&dev->struct_mutex);

	if (!dev_priv->sarea_bo) {
		DRM_ERROR("did not find sarea bo\n");
		return -EINVAL;
	}

	ret = drm_bo_kmap(dev_priv->sarea_bo, 0,
			  dev_priv->sarea_bo->num_pages,
			  &dev_priv->sarea_kmap);
	if (ret) {
		DRM_ERROR("could not map sarea bo\n");
		return ret;
	}

	p = dev_priv->sarea_kmap.virtual;
	end = (void *) p + (dev_priv->sarea_bo->num_pages << PAGE_SHIFT);
	while (p < end && DRI2_SAREA_BLOCK_TYPE(*p) != DRI2_SAREA_BLOCK_END) {
		switch (DRI2_SAREA_BLOCK_TYPE(*p)) {
		case DRI2_SAREA_BLOCK_LOCK:
			dev->primary->master->lock.hw_lock = (void *) (p + 1);
			dev->sigdata.lock = dev->primary->master->lock.hw_lock;
			break;
		}
		next = DRI2_SAREA_BLOCK_NEXT(p);
		if (next <= p || end < next) {
			DRM_ERROR("malformed dri2 sarea: next is %p should be within %p-%p\n",
				  next, p, end);
			return -EINVAL;
		}
		p = next;
	}

	return 0;
}


static int i915_initialize(struct drm_device * dev,
			   struct drm_file *file_priv,
			   drm_i915_init_t * init)
{
	struct drm_i915_private *dev_priv = dev->dev_private;
	struct drm_i915_master_private *master_priv = dev->primary->master->driver_priv;

	dev_priv->mmio_map = drm_core_findmap(dev, init->mmio_offset);
	if (!dev_priv->mmio_map) {
		i915_dma_cleanup(dev);
		DRM_ERROR("can not find mmio map!\n");
		return -EINVAL;
	}

#ifdef I915_HAVE_BUFFER
	dev_priv->max_validate_buffers = I915_MAX_VALIDATE_BUFFERS;
#endif

	if (!dev_priv->ring.Size) {
		dev_priv->ring.Start = init->ring_start;
		dev_priv->ring.End = init->ring_end;
		dev_priv->ring.Size = init->ring_size;
		dev_priv->ring.tail_mask = dev_priv->ring.Size - 1;
		
		dev_priv->ring.map.offset = init->ring_start;
		dev_priv->ring.map.size = init->ring_size;
		dev_priv->ring.map.type = 0;
		dev_priv->ring.map.flags = 0;
		dev_priv->ring.map.mtrr = 0;
		
		drm_core_ioremap(&dev_priv->ring.map, dev);
		
		if (dev_priv->ring.map.handle == NULL) {
			i915_dma_cleanup(dev);
			DRM_ERROR("can not ioremap virtual address for"
				  " ring buffer\n");
			return -ENOMEM;
		}
		dev_priv->ring.virtual_start = dev_priv->ring.map.handle;
	}


	dev_priv->cpp = init->cpp;
	master_priv->sarea_priv->pf_current_page = 0;

	/* We are using separate values as placeholders for mechanisms for
	 * private backbuffer/depthbuffer usage.
	 */
	dev_priv->use_mi_batchbuffer_start = 0;
	if (IS_I965G(dev)) /* 965 doesn't support older method */
		dev_priv->use_mi_batchbuffer_start = 1;

	/* Allow hardware batchbuffers unless told otherwise.
	 */
	dev_priv->allow_batchbuffer = 1;

	/* Enable vblank on pipe A for older X servers
	 */
	dev_priv->vblank_pipe = DRM_I915_VBLANK_PIPE_A;

	/* Program Hardware Status Page */
	if (!IS_G33(dev)) {
		dev_priv->status_page_dmah =
			drm_pci_alloc(dev, PAGE_SIZE, PAGE_SIZE, 0xffffffff);

		if (!dev_priv->status_page_dmah) {
			i915_dma_cleanup(dev);
			DRM_ERROR("Can not allocate hardware status page\n");
			return -ENOMEM;
		}
		dev_priv->hw_status_page = dev_priv->status_page_dmah->vaddr;
		dev_priv->dma_status_page = dev_priv->status_page_dmah->busaddr;

		memset(dev_priv->hw_status_page, 0, PAGE_SIZE);

		I915_WRITE(0x02080, dev_priv->dma_status_page);
	}
	DRM_DEBUG("Enabled hardware status page\n");
#ifdef I915_HAVE_BUFFER
	mutex_init(&dev_priv->cmdbuf_mutex);
#endif

	if (init->func == I915_INIT_DMA2) {
		int ret = setup_dri2_sarea(dev, file_priv, init);
		if (ret) {
			i915_dma_cleanup(dev);
			DRM_ERROR("could not set up dri2 sarea\n");
			return ret;
		}
	}

	return 0;
}

static int i915_dma_resume(struct drm_device * dev)
{
	struct drm_i915_private *dev_priv = (struct drm_i915_private *) dev->dev_private;

	DRM_DEBUG("\n");

	if (!dev_priv->mmio_map) {
		DRM_ERROR("can not find mmio map!\n");
		return -EINVAL;
	}

	if (dev_priv->ring.map.handle == NULL) {
		DRM_ERROR("can not ioremap virtual address for"
			  " ring buffer\n");
		return -ENOMEM;
	}

	/* Program Hardware Status Page */
	if (!dev_priv->hw_status_page) {
		DRM_ERROR("Can not find hardware status page\n");
		return -EINVAL;
	}
	DRM_DEBUG("hw status page @ %p\n", dev_priv->hw_status_page);

	if (dev_priv->status_gfx_addr != 0)
		I915_WRITE(0x02080, dev_priv->status_gfx_addr);
	else
		I915_WRITE(0x02080, dev_priv->dma_status_page);
	DRM_DEBUG("Enabled hardware status page\n");

	return 0;
}

static int i915_dma_init(struct drm_device *dev, void *data,
			 struct drm_file *file_priv)
{
	struct drm_i915_init *init = data;
	int retcode = 0;

	switch (init->func) {
	case I915_INIT_DMA:
	case I915_INIT_DMA2:
		retcode = i915_initialize(dev, file_priv, init);
		break;
	case I915_CLEANUP_DMA:
		retcode = i915_dma_cleanup(dev);
		break;
	case I915_RESUME_DMA:
		retcode = i915_dma_resume(dev);
		break;
	default:
		retcode = -EINVAL;
		break;
	}

	return retcode;
}

/* Implement basically the same security restrictions as hardware does
 * for MI_BATCH_NON_SECURE.  These can be made stricter at any time.
 *
 * Most of the calculations below involve calculating the size of a
 * particular instruction.  It's important to get the size right as
 * that tells us where the next instruction to check is.  Any illegal
 * instruction detected will be given a size of zero, which is a
 * signal to abort the rest of the buffer.
 */
static int do_validate_cmd(int cmd)
{
	switch (((cmd >> 29) & 0x7)) {
	case 0x0:
		switch ((cmd >> 23) & 0x3f) {
		case 0x0:
			return 1;	/* MI_NOOP */
		case 0x4:
			return 1;	/* MI_FLUSH */
		default:
			return 0;	/* disallow everything else */
		}
		break;
	case 0x1:
		return 0;	/* reserved */
	case 0x2:
		return (cmd & 0xff) + 2;	/* 2d commands */
	case 0x3:
		if (((cmd >> 24) & 0x1f) <= 0x18)
			return 1;

		switch ((cmd >> 24) & 0x1f) {
		case 0x1c:
			return 1;
		case 0x1d:
			switch ((cmd >> 16) & 0xff) {
			case 0x3:
				return (cmd & 0x1f) + 2;
			case 0x4:
				return (cmd & 0xf) + 2;
			default:
				return (cmd & 0xffff) + 2;
			}
		case 0x1e:
			if (cmd & (1 << 23))
				return (cmd & 0xffff) + 1;
			else
				return 1;
		case 0x1f:
			if ((cmd & (1 << 23)) == 0)	/* inline vertices */
				return (cmd & 0x1ffff) + 2;
			else if (cmd & (1 << 17))	/* indirect random */
				if ((cmd & 0xffff) == 0)
					return 0;	/* unknown length, too hard */
				else
					return (((cmd & 0xffff) + 1) / 2) + 1;
			else
				return 2;	/* indirect sequential */
		default:
			return 0;
		}
	default:
		return 0;
	}

	return 0;
}

static int validate_cmd(int cmd)
{
	int ret = do_validate_cmd(cmd);

/*	printk("validate_cmd( %x ): %d\n", cmd, ret); */

	return ret;
}

static int i915_emit_cmds(struct drm_device *dev, int __user *buffer,
			  int dwords)
{
	struct drm_i915_private *dev_priv = dev->dev_private;
	int i;
	RING_LOCALS;

	if ((dwords+1) * sizeof(int) >= dev_priv->ring.Size - 8)
		return -EINVAL;

	BEGIN_LP_RING((dwords+1)&~1);

	for (i = 0; i < dwords;) {
		int cmd, sz;

		if (DRM_COPY_FROM_USER_UNCHECKED(&cmd, &buffer[i], sizeof(cmd)))
			return -EINVAL;

		if ((sz = validate_cmd(cmd)) == 0 || i + sz > dwords)
			return -EINVAL;

		OUT_RING(cmd);

		while (++i, --sz) {
			if (DRM_COPY_FROM_USER_UNCHECKED(&cmd, &buffer[i],
							 sizeof(cmd))) {
				return -EINVAL;
			}
			OUT_RING(cmd);
		}
	}

	if (dwords & 1)
		OUT_RING(0);

	ADVANCE_LP_RING();

	return 0;
}

static int i915_emit_box(struct drm_device * dev,
			 struct drm_clip_rect __user * boxes,
			 int i, int DR1, int DR4)
{
	struct drm_i915_private *dev_priv = dev->dev_private;
	struct drm_clip_rect box;
	RING_LOCALS;

	if (DRM_COPY_FROM_USER_UNCHECKED(&box, &boxes[i], sizeof(box))) {
		return -EFAULT;
	}

	if (box.y2 <= box.y1 || box.x2 <= box.x1 || box.y2 <= 0 || box.x2 <= 0) {
		DRM_ERROR("Bad box %d,%d..%d,%d\n",
			  box.x1, box.y1, box.x2, box.y2);
		return -EINVAL;
	}

	if (IS_I965G(dev)) {
		BEGIN_LP_RING(4);
		OUT_RING(GFX_OP_DRAWRECT_INFO_I965);
		OUT_RING((box.x1 & 0xffff) | (box.y1 << 16));
		OUT_RING(((box.x2 - 1) & 0xffff) | ((box.y2 - 1) << 16));
		OUT_RING(DR4);
		ADVANCE_LP_RING();
	} else {
		BEGIN_LP_RING(6);
		OUT_RING(GFX_OP_DRAWRECT_INFO);
		OUT_RING(DR1);
		OUT_RING((box.x1 & 0xffff) | (box.y1 << 16));
		OUT_RING(((box.x2 - 1) & 0xffff) | ((box.y2 - 1) << 16));
		OUT_RING(DR4);
		OUT_RING(0);
		ADVANCE_LP_RING();
	}

	return 0;
}

/* XXX: Emitting the counter should really be moved to part of the IRQ
 * emit. For now, do it in both places:
 */

void i915_emit_breadcrumb(struct drm_device *dev)
{
	struct drm_i915_private *dev_priv = dev->dev_private;
	struct drm_i915_master_private *master_priv = dev->primary->master->driver_priv;
	RING_LOCALS;

	if (++dev_priv->counter > BREADCRUMB_MASK) {
		 dev_priv->counter = 1;
		 DRM_DEBUG("Breadcrumb counter wrapped around\n");
	}

	master_priv->sarea_priv->last_enqueue = dev_priv->counter;

	BEGIN_LP_RING(4);
	OUT_RING(CMD_STORE_DWORD_IDX);
	OUT_RING(20);
	OUT_RING(dev_priv->counter);
	OUT_RING(0);
	ADVANCE_LP_RING();
}


int i915_emit_mi_flush(struct drm_device *dev, uint32_t flush)
{
	struct drm_i915_private *dev_priv = dev->dev_private;
	uint32_t flush_cmd = CMD_MI_FLUSH;
	RING_LOCALS;

	flush_cmd |= flush;

	i915_kernel_lost_context(dev);

	BEGIN_LP_RING(4);
	OUT_RING(flush_cmd);
	OUT_RING(0);
	OUT_RING(0);
	OUT_RING(0);
	ADVANCE_LP_RING();

	return 0;
}


static int i915_dispatch_cmdbuffer(struct drm_device * dev,
				   struct drm_i915_cmdbuffer * cmd)
{
#ifdef I915_HAVE_FENCE
	struct drm_i915_private *dev_priv = dev->dev_private;
#endif
	int nbox = cmd->num_cliprects;
	int i = 0, count, ret;

	if (cmd->sz & 0x3) {
		DRM_ERROR("alignment\n");
		return -EINVAL;
	}

	i915_kernel_lost_context(dev);

	count = nbox ? nbox : 1;

	for (i = 0; i < count; i++) {
		if (i < nbox) {
			ret = i915_emit_box(dev, cmd->cliprects, i,
					    cmd->DR1, cmd->DR4);
			if (ret)
				return ret;
		}

		ret = i915_emit_cmds(dev, (int __user *)cmd->buf, cmd->sz / 4);
		if (ret)
			return ret;
	}

	i915_emit_breadcrumb(dev);
#ifdef I915_HAVE_FENCE
	if (unlikely((dev_priv->counter & 0xFF) == 0))
		drm_fence_flush_old(dev, 0, dev_priv->counter);
#endif
	return 0;
}

static int i915_dispatch_batchbuffer(struct drm_device * dev,
				     drm_i915_batchbuffer_t * batch)
{
	struct drm_i915_private *dev_priv = dev->dev_private;
	struct drm_clip_rect __user *boxes = batch->cliprects;
	int nbox = batch->num_cliprects;
	int i = 0, count;
	RING_LOCALS;

	if ((batch->start | batch->used) & 0x7) {
		DRM_ERROR("alignment\n");
		return -EINVAL;
	}

	i915_kernel_lost_context(dev);

	count = nbox ? nbox : 1;

	for (i = 0; i < count; i++) {
		if (i < nbox) {
			int ret = i915_emit_box(dev, boxes, i,
						batch->DR1, batch->DR4);
			if (ret)
				return ret;
		}

		if (dev_priv->use_mi_batchbuffer_start) {
			BEGIN_LP_RING(2);
			if (IS_I965G(dev)) {
				OUT_RING(MI_BATCH_BUFFER_START | (2 << 6) | MI_BATCH_NON_SECURE_I965);
				OUT_RING(batch->start);
			} else {
				OUT_RING(MI_BATCH_BUFFER_START | (2 << 6));
				OUT_RING(batch->start | MI_BATCH_NON_SECURE);
			}
			ADVANCE_LP_RING();

		} else {
			BEGIN_LP_RING(4);
			OUT_RING(MI_BATCH_BUFFER);
			OUT_RING(batch->start | MI_BATCH_NON_SECURE);
			OUT_RING(batch->start + batch->used - 4);
			OUT_RING(0);
			ADVANCE_LP_RING();
		}
	}

	i915_emit_breadcrumb(dev);
#ifdef I915_HAVE_FENCE
	if (unlikely((dev_priv->counter & 0xFF) == 0))
		drm_fence_flush_old(dev, 0, dev_priv->counter);
#endif
	return 0;
}

static void i915_do_dispatch_flip(struct drm_device * dev, int plane, int sync)
{
	struct drm_i915_private *dev_priv = dev->dev_private;
	struct drm_i915_master_private *master_priv = dev->primary->master->driver_priv;
	u32 num_pages, current_page, next_page, dspbase;
	int shift = 2 * plane, x, y;
	RING_LOCALS;

	/* Calculate display base offset */
	num_pages = master_priv->sarea_priv->third_handle ? 3 : 2;
	current_page = (master_priv->sarea_priv->pf_current_page >> shift) & 0x3;
	next_page = (current_page + 1) % num_pages;

	switch (next_page) {
	default:
	case 0:
		dspbase = master_priv->sarea_priv->front_offset;
		break;
	case 1:
		dspbase = master_priv->sarea_priv->back_offset;
		break;
	case 2:
		dspbase = master_priv->sarea_priv->third_offset;
		break;
	}

	if (plane == 0) {
		x = master_priv->sarea_priv->planeA_x;
		y = master_priv->sarea_priv->planeA_y;
	} else {
		x = master_priv->sarea_priv->planeB_x;
		y = master_priv->sarea_priv->planeB_y;
	}

	dspbase += (y * master_priv->sarea_priv->pitch + x) * dev_priv->cpp;

	DRM_DEBUG("plane=%d current_page=%d dspbase=0x%x\n", plane, current_page,
		  dspbase);

	BEGIN_LP_RING(4);
	OUT_RING(sync ? 0 :
		 (MI_WAIT_FOR_EVENT | (plane ? MI_WAIT_FOR_PLANE_B_FLIP :
				       MI_WAIT_FOR_PLANE_A_FLIP)));
	OUT_RING(CMD_OP_DISPLAYBUFFER_INFO | (sync ? 0 : ASYNC_FLIP) |
		 (plane ? DISPLAY_PLANE_B : DISPLAY_PLANE_A));
	OUT_RING(master_priv->sarea_priv->pitch * dev_priv->cpp);
	OUT_RING(dspbase);
	ADVANCE_LP_RING();

	master_priv->sarea_priv->pf_current_page &= ~(0x3 << shift);
	master_priv->sarea_priv->pf_current_page |= next_page << shift;
}

void i915_dispatch_flip(struct drm_device * dev, int planes, int sync)
{
	struct drm_i915_private *dev_priv = dev->dev_private;
	struct drm_i915_master_private *master_priv = dev->primary->master->driver_priv;
	int i;

	DRM_DEBUG("planes=0x%x pfCurrentPage=%d\n",
		  planes, master_priv->sarea_priv->pf_current_page);

	i915_emit_mi_flush(dev, MI_READ_FLUSH | MI_EXE_FLUSH);

	for (i = 0; i < 2; i++)
		if (planes & (1 << i))
			i915_do_dispatch_flip(dev, i, sync);

	i915_emit_breadcrumb(dev);
#ifdef I915_HAVE_FENCE
	if (unlikely(!sync && ((dev_priv->counter & 0xFF) == 0)))
		drm_fence_flush_old(dev, 0, dev_priv->counter);
#endif
}

static int i915_quiescent(struct drm_device *dev)
{
	struct drm_i915_private *dev_priv = dev->dev_private;

	i915_kernel_lost_context(dev);
	return i915_wait_ring(dev, dev_priv->ring.Size - 8, __FUNCTION__);
}

static int i915_flush_ioctl(struct drm_device *dev, void *data,
			    struct drm_file *file_priv)
{

	LOCK_TEST_WITH_RETURN(dev, file_priv);

	return i915_quiescent(dev);
}

static int i915_batchbuffer(struct drm_device *dev, void *data,
			    struct drm_file *file_priv)
{
	struct drm_i915_private *dev_priv = (struct drm_i915_private *) dev->dev_private;
	struct drm_i915_master_private *master_priv = dev->primary->master->driver_priv;
	drm_i915_sarea_t *sarea_priv = (drm_i915_sarea_t *)
	    master_priv->sarea_priv;
	drm_i915_batchbuffer_t *batch = data;
	int ret;

	if (!dev_priv->allow_batchbuffer) {
		DRM_ERROR("Batchbuffer ioctl disabled\n");
		return -EINVAL;
	}

	DRM_DEBUG("i915 batchbuffer, start %x used %d cliprects %d\n",
		  batch->start, batch->used, batch->num_cliprects);

	LOCK_TEST_WITH_RETURN(dev, file_priv);

	if (batch->num_cliprects && DRM_VERIFYAREA_READ(batch->cliprects,
							batch->num_cliprects *
							sizeof(struct drm_clip_rect)))
		return -EFAULT;

	ret = i915_dispatch_batchbuffer(dev, batch);

	sarea_priv->last_dispatch = READ_BREADCRUMB(dev_priv);
	return ret;
}

static int i915_cmdbuffer(struct drm_device *dev, void *data,
			  struct drm_file *file_priv)
{
	struct drm_i915_private *dev_priv = (struct drm_i915_private *) dev->dev_private;
	struct drm_i915_master_private *master_priv = dev->primary->master->driver_priv;
	struct drm_i915_sarea *sarea_priv = (struct drm_i915_sarea *)
		master_priv->sarea_priv;
	struct drm_i915_cmdbuffer *cmdbuf = data;
	int ret;

	DRM_DEBUG("i915 cmdbuffer, buf %p sz %d cliprects %d\n",
		  cmdbuf->buf, cmdbuf->sz, cmdbuf->num_cliprects);

	LOCK_TEST_WITH_RETURN(dev, file_priv);

	if (cmdbuf->num_cliprects &&
	    DRM_VERIFYAREA_READ(cmdbuf->cliprects,
				cmdbuf->num_cliprects *
				sizeof(struct drm_clip_rect))) {
		DRM_ERROR("Fault accessing cliprects\n");
		return -EFAULT;
	}

	ret = i915_dispatch_cmdbuffer(dev, cmdbuf);
	if (ret) {
		DRM_ERROR("i915_dispatch_cmdbuffer failed\n");
		return ret;
	}

	sarea_priv->last_dispatch = READ_BREADCRUMB(dev_priv);
	return 0;
}

#if DRM_DEBUG_CODE
#define DRM_DEBUG_RELOCATION	(drm_debug != 0)
#else
#define DRM_DEBUG_RELOCATION	0
#endif

#ifdef I915_HAVE_BUFFER

struct i915_relocatee_info {
	struct drm_buffer_object *buf;
	unsigned long offset;
	u32 *data_page;
	unsigned page_offset;
	struct drm_bo_kmap_obj kmap;
	int is_iomem;
};

struct drm_i915_validate_buffer {
	struct drm_buffer_object *buffer;
	struct drm_bo_info_rep rep;
	int presumed_offset_correct;
	void __user *data;
	int ret;
};

static void i915_dereference_buffers_locked(struct drm_i915_validate_buffer *buffers,
					    unsigned num_buffers)
{
	while (num_buffers--)
		drm_bo_usage_deref_locked(&buffers[num_buffers].buffer);
}

int i915_apply_reloc(struct drm_file *file_priv, int num_buffers,
		     struct drm_i915_validate_buffer *buffers,
		     struct i915_relocatee_info *relocatee,
		     uint32_t *reloc)
{
	unsigned index;
	unsigned long new_cmd_offset;
	u32 val;
	int ret, i;
	int buf_index = -1;

	/*
	 * FIXME: O(relocs * buffers) complexity.
	 */

	for (i = 0; i <= num_buffers; i++)
		if (buffers[i].buffer)
			if (reloc[2] == buffers[i].buffer->base.hash.key)
				buf_index = i;

	if (buf_index == -1) {
		DRM_ERROR("Illegal relocation buffer %08X\n", reloc[2]);
		return -EINVAL;
	}

	/*
	 * Short-circuit relocations that were correctly
	 * guessed by the client
	 */
	if (buffers[buf_index].presumed_offset_correct && !DRM_DEBUG_RELOCATION)
		return 0;

	new_cmd_offset = reloc[0];
	if (!relocatee->data_page ||
	    !drm_bo_same_page(relocatee->offset, new_cmd_offset)) {
		drm_bo_kunmap(&relocatee->kmap);
		relocatee->data_page = NULL;
		relocatee->offset = new_cmd_offset;
		ret = drm_bo_kmap(relocatee->buf, new_cmd_offset >> PAGE_SHIFT,
				  1, &relocatee->kmap);
		if (ret) {
			DRM_ERROR("Could not map command buffer to apply relocs\n %08lx", new_cmd_offset);
			return ret;
		}
		relocatee->data_page = drm_bmo_virtual(&relocatee->kmap,
						       &relocatee->is_iomem);
		relocatee->page_offset = (relocatee->offset & PAGE_MASK);
	}

	val = buffers[buf_index].buffer->offset;
	index = (reloc[0] - relocatee->page_offset) >> 2;

	/* add in validate */
	val = val + reloc[1];

	if (DRM_DEBUG_RELOCATION) {
		if (buffers[buf_index].presumed_offset_correct &&
		    relocatee->data_page[index] != val) {
			DRM_DEBUG ("Relocation mismatch source %d target %d buffer %d user %08x kernel %08x\n",
				   reloc[0], reloc[1], buf_index, relocatee->data_page[index], val);
		}
	}

	if (relocatee->is_iomem)
		iowrite32(val, relocatee->data_page + index);
	else
		relocatee->data_page[index] = val;
	return 0;
}

int i915_process_relocs(struct drm_file *file_priv,
			uint32_t buf_handle,
			uint32_t __user **reloc_user_ptr,
			struct i915_relocatee_info *relocatee,
			struct drm_i915_validate_buffer *buffers,
			uint32_t num_buffers)
{
	int ret, reloc_stride;
	uint32_t cur_offset;
	uint32_t reloc_count;
	uint32_t reloc_type;
	uint32_t reloc_buf_size;
	uint32_t *reloc_buf = NULL;
	int i;

	/* do a copy from user from the user ptr */
	ret = get_user(reloc_count, *reloc_user_ptr);
	if (ret) {
		DRM_ERROR("Could not map relocation buffer.\n");
		goto out;
	}

	ret = get_user(reloc_type, (*reloc_user_ptr)+1);
	if (ret) {
		DRM_ERROR("Could not map relocation buffer.\n");
		goto out;
	}

	if (reloc_type != 0) {
		DRM_ERROR("Unsupported relocation type requested\n");
		ret = -EINVAL;
		goto out;
	}

	reloc_buf_size = (I915_RELOC_HEADER + (reloc_count * I915_RELOC0_STRIDE)) * sizeof(uint32_t);
	reloc_buf = kmalloc(reloc_buf_size, GFP_KERNEL);
	if (!reloc_buf) {
		DRM_ERROR("Out of memory for reloc buffer\n");
		ret = -ENOMEM;
		goto out;
	}

	if (copy_from_user(reloc_buf, *reloc_user_ptr, reloc_buf_size)) {
		ret = -EFAULT;
		goto out;
	}

	/* get next relocate buffer handle */
	*reloc_user_ptr = (uint32_t *)*(unsigned long *)&reloc_buf[2];

	reloc_stride = I915_RELOC0_STRIDE * sizeof(uint32_t); /* may be different for other types of relocs */

	DRM_DEBUG("num relocs is %d, next is %p\n", reloc_count, *reloc_user_ptr);

	for (i = 0; i < reloc_count; i++) {
		cur_offset = I915_RELOC_HEADER + (i * I915_RELOC0_STRIDE);
		  
		ret = i915_apply_reloc(file_priv, num_buffers, buffers,
				       relocatee, reloc_buf + cur_offset);
		if (ret)
			goto out;
	}

out:
	if (reloc_buf)
		kfree(reloc_buf);

	if (relocatee->data_page) {		
		drm_bo_kunmap(&relocatee->kmap);
		relocatee->data_page = NULL;
	}

	return ret;
}

static int i915_exec_reloc(struct drm_file *file_priv, drm_handle_t buf_handle,
			   uint32_t __user *reloc_user_ptr,
			   struct drm_i915_validate_buffer *buffers,
			   uint32_t buf_count)
{
	struct drm_device *dev = file_priv->minor->dev;
	struct i915_relocatee_info relocatee;
	int ret = 0;
	int b;

	/*
	 * Short circuit relocations when all previous
	 * buffers offsets were correctly guessed by
	 * the client
	 */
	if (!DRM_DEBUG_RELOCATION) {
		for (b = 0; b < buf_count; b++)
			if (!buffers[b].presumed_offset_correct)
				break;
	
		if (b == buf_count)
			return 0;
	}

	memset(&relocatee, 0, sizeof(relocatee));

	mutex_lock(&dev->struct_mutex);
	relocatee.buf = drm_lookup_buffer_object(file_priv, buf_handle, 1);
	mutex_unlock(&dev->struct_mutex);
	if (!relocatee.buf) {
		DRM_DEBUG("relocatee buffer invalid %08x\n", buf_handle);
		ret = -EINVAL;
		goto out_err;
	}

	mutex_lock (&relocatee.buf->mutex);
	ret = drm_bo_wait (relocatee.buf, 0, 0, FALSE);
	if (ret)
		goto out_err1;

	while (reloc_user_ptr) {
		ret = i915_process_relocs(file_priv, buf_handle, &reloc_user_ptr, &relocatee, buffers, buf_count);
		if (ret) {
			DRM_ERROR("process relocs failed\n");
			goto out_err1;
		}
	}

out_err1:
	mutex_unlock (&relocatee.buf->mutex);
	drm_bo_usage_deref_unlocked(&relocatee.buf);
out_err:
	return ret;
}

static int i915_check_presumed(struct drm_i915_op_arg *arg,
			       struct drm_buffer_object *bo,
			       uint32_t __user *data,
			       int *presumed_ok)
{
	struct drm_bo_op_req *req = &arg->d.req;
	uint32_t hint_offset;
	uint32_t hint = req->bo_req.hint;

	*presumed_ok = 0;

	if (!(hint & DRM_BO_HINT_PRESUMED_OFFSET))
		return 0;
	if (bo->offset == req->bo_req.presumed_offset) {
		*presumed_ok = 1;
		return 0;
	}

	/*
	 * We need to turn off the HINT_PRESUMED_OFFSET for this buffer in
	 * the user-space IOCTL argument list, since the buffer has moved,
	 * we're about to apply relocations and we might subsequently
	 * hit an -EAGAIN. In that case the argument list will be reused by
	 * user-space, but the presumed offset is no longer valid.
	 *
	 * Needless to say, this is a bit ugly.
	 */

       	hint_offset = (uint32_t *)&req->bo_req.hint - (uint32_t *)arg;
	hint &= ~DRM_BO_HINT_PRESUMED_OFFSET;
	return __put_user(hint, data + hint_offset);
}


/*
 * Validate, add fence and relocate a block of bos from a userspace list
 */
int i915_validate_buffer_list(struct drm_file *file_priv,
			      unsigned int fence_class, uint64_t data,
			      struct drm_i915_validate_buffer *buffers,
			      uint32_t *num_buffers)
{
	struct drm_i915_op_arg arg;
	struct drm_bo_op_req *req = &arg.d.req;
	int ret = 0;
	unsigned buf_count = 0;
	uint32_t buf_handle;
	uint32_t __user *reloc_user_ptr;
	struct drm_i915_validate_buffer *item = buffers;

	do {
		if (buf_count >= *num_buffers) {
			DRM_ERROR("Buffer count exceeded %d\n.", *num_buffers);
			ret = -EINVAL;
			goto out_err;
		}
		item = buffers + buf_count;
		item->buffer = NULL;
		item->presumed_offset_correct = 0;

		buffers[buf_count].buffer = NULL;

		if (copy_from_user(&arg, (void __user *)(unsigned long)data, sizeof(arg))) {
			ret = -EFAULT;
			goto out_err;
		}

		ret = 0;
		if (req->op != drm_bo_validate) {
			DRM_ERROR
			    ("Buffer object operation wasn't \"validate\".\n");
			ret = -EINVAL;
			goto out_err;
		}
		item->ret = 0;
		item->data = (void __user *) (unsigned long) data;

		buf_handle = req->bo_req.handle;
		reloc_user_ptr = (uint32_t *)(unsigned long)arg.reloc_ptr;

		if (reloc_user_ptr) {
			ret = i915_exec_reloc(file_priv, buf_handle, reloc_user_ptr, buffers, buf_count);
			if (ret)
				goto out_err;
			DRM_MEMORYBARRIER();
		}

		ret = drm_bo_handle_validate(file_priv, req->bo_req.handle,
					     req->bo_req.flags, req->bo_req.mask,
					     req->bo_req.hint,
					     req->bo_req.fence_class, 0,
					     &item->rep,
					     &item->buffer);

		if (ret) {
			DRM_ERROR("error on handle validate %d\n", ret);
			goto out_err;
		}

		buf_count++;

		ret = i915_check_presumed(&arg, item->buffer,
					  (uint32_t __user *)
					  (unsigned long) data,
					  &item->presumed_offset_correct);
		if (ret)
			goto out_err;

		data = arg.next;
	} while (data != 0);
out_err:
	*num_buffers = buf_count;
	item->ret = (ret != -EAGAIN) ? ret : 0;
	return ret;
}


/*
 * Remove all buffers from the unfenced list.
 * If the execbuffer operation was aborted, for example due to a signal,
 * this also make sure that buffers retain their original state and
 * fence pointers.
 * Copy back buffer information to user-space unless we were interrupted
 * by a signal. In which case the IOCTL must be rerun.
 */

static int i915_handle_copyback(struct drm_device *dev,
				struct drm_i915_validate_buffer *buffers,
				unsigned int num_buffers, int ret)
{
	int err = ret;
	int i;
	struct drm_i915_op_arg arg;

	if (ret)
		drm_putback_buffer_objects(dev);

	if (ret != -EAGAIN) {
		for (i = 0; i < num_buffers; ++i) {
			arg.handled = 1;
			arg.d.rep.ret = buffers->ret;
			arg.d.rep.bo_info = buffers->rep;
			if (__copy_to_user(buffers->data, &arg, sizeof(arg)))
				err = -EFAULT;
			buffers++;
		}
	}

	return err;
}

/*
 * Create a fence object, and if that fails, pretend that everything is
 * OK and just idle the GPU.
 */

void i915_fence_or_sync(struct drm_file *file_priv,
			uint32_t fence_flags,
			struct drm_fence_arg *fence_arg,
			struct drm_fence_object **fence_p)
{
	struct drm_device *dev = file_priv->minor->dev;
	int ret;
	struct drm_fence_object *fence;

	ret = drm_fence_buffer_objects(dev, NULL, fence_flags,
			 NULL, &fence);

	if (ret) {

		/*
		 * Fence creation failed.
		 * Fall back to synchronous operation and idle the engine.
		 */

		(void) i915_emit_mi_flush(dev, MI_READ_FLUSH);
		(void) i915_quiescent(dev);

		if (!(fence_flags & DRM_FENCE_FLAG_NO_USER)) {

			/*
			 * Communicate to user-space that
			 * fence creation has failed and that
			 * the engine is idle.
			 */

			fence_arg->handle = ~0;
			fence_arg->error = ret;
		}

		drm_putback_buffer_objects(dev);
		if (fence_p)
		    *fence_p = NULL;
		return;
	}

	if (!(fence_flags & DRM_FENCE_FLAG_NO_USER)) {

		ret = drm_fence_add_user_object(file_priv, fence,
						fence_flags &
						DRM_FENCE_FLAG_SHAREABLE);
		if (!ret)
			drm_fence_fill_arg(fence, fence_arg);
		else {
			/*
			 * Fence user object creation failed.
			 * We must idle the engine here as well, as user-
			 * space expects a fence object to wait on. Since we
			 * have a fence object we wait for it to signal
			 * to indicate engine "sufficiently" idle.
			 */

			(void) drm_fence_object_wait(fence, 0, 1,
						     fence->type);
			drm_fence_usage_deref_unlocked(&fence);
			fence_arg->handle = ~0;
			fence_arg->error = ret;
		}
	}

	if (fence_p)
		*fence_p = fence;
	else if (fence)
		drm_fence_usage_deref_unlocked(&fence);
}


static int i915_execbuffer(struct drm_device *dev, void *data,
			   struct drm_file *file_priv)
{
	struct drm_i915_private *dev_priv = (struct drm_i915_private *) dev->dev_private;
	struct drm_i915_master_private *master_priv = dev->primary->master->driver_priv;
	drm_i915_sarea_t *sarea_priv = (drm_i915_sarea_t *)
		master_priv->sarea_priv;
	struct drm_i915_execbuffer *exec_buf = data;
	struct drm_i915_batchbuffer *batch = &exec_buf->batch;
	struct drm_fence_arg *fence_arg = &exec_buf->fence_arg;
	int num_buffers;
	int ret;
	struct drm_i915_validate_buffer *buffers;

	if (!dev_priv->allow_batchbuffer) {
		DRM_ERROR("Batchbuffer ioctl disabled\n");
		return -EINVAL;
	}


	if (batch->num_cliprects && DRM_VERIFYAREA_READ(batch->cliprects,
							batch->num_cliprects *
							sizeof(struct drm_clip_rect)))
		return -EFAULT;

	if (exec_buf->num_buffers > dev_priv->max_validate_buffers)
		return -EINVAL;


	ret = drm_bo_read_lock(&dev->bm.bm_lock);
	if (ret)
		return ret;

	/*
	 * The cmdbuf_mutex makes sure the validate-submit-fence
	 * operation is atomic.
	 */

	ret = mutex_lock_interruptible(&dev_priv->cmdbuf_mutex);
	if (ret) {
		drm_bo_read_unlock(&dev->bm.bm_lock);
		return -EAGAIN;
	}

	num_buffers = exec_buf->num_buffers;

	buffers = drm_calloc(num_buffers, sizeof(struct drm_i915_validate_buffer), DRM_MEM_DRIVER);
	if (!buffers) {
		drm_bo_read_unlock(&dev->bm.bm_lock);
		mutex_unlock(&dev_priv->cmdbuf_mutex);
		return -ENOMEM;
	}

	/* validate buffer list + fixup relocations */
	ret = i915_validate_buffer_list(file_priv, 0, exec_buf->ops_list,
					buffers, &num_buffers);
	if (ret)
		goto out_err0;

	/* make sure all previous memory operations have passed */
	DRM_MEMORYBARRIER();
	drm_agp_chipset_flush(dev);

	/* submit buffer */
	batch->start = buffers[num_buffers-1].buffer->offset;

	DRM_DEBUG("i915 exec batchbuffer, start %x used %d cliprects %d\n",
		  batch->start, batch->used, batch->num_cliprects);

	ret = i915_dispatch_batchbuffer(dev, batch);
	if (ret)
		goto out_err0;

	if (sarea_priv)
		sarea_priv->last_dispatch = READ_BREADCRUMB(dev_priv);

	i915_fence_or_sync(file_priv, fence_arg->flags, fence_arg, NULL);

out_err0:

	/* handle errors */
	ret = i915_handle_copyback(dev, buffers, num_buffers, ret);
	mutex_lock(&dev->struct_mutex);
	i915_dereference_buffers_locked(buffers, num_buffers);
	mutex_unlock(&dev->struct_mutex);

	drm_free(buffers, (exec_buf->num_buffers * sizeof(struct drm_buffer_object *)), DRM_MEM_DRIVER);

	mutex_unlock(&dev_priv->cmdbuf_mutex);
	drm_bo_read_unlock(&dev->bm.bm_lock);
	return ret;
}
#endif

int i915_do_cleanup_pageflip(struct drm_device * dev)
{
	struct drm_i915_master_private *master_priv = dev->primary->master->driver_priv;
	int i, planes, num_pages;

	DRM_DEBUG("\n");
	num_pages = master_priv->sarea_priv->third_handle ? 3 : 2;
	for (i = 0, planes = 0; i < 2; i++) {
		if (master_priv->sarea_priv->pf_current_page & (0x3 << (2 * i))) {
			master_priv->sarea_priv->pf_current_page =
				(master_priv->sarea_priv->pf_current_page &
				 ~(0x3 << (2 * i))) | ((num_pages - 1) << (2 * i));

			planes |= 1 << i;
		}
	}

	if (planes)
		i915_dispatch_flip(dev, planes, 0);

	return 0;
}

static int i915_flip_bufs(struct drm_device *dev, void *data, struct drm_file *file_priv)
{
	struct drm_i915_flip *param = data;

	DRM_DEBUG("\n");

	LOCK_TEST_WITH_RETURN(dev, file_priv);

	/* This is really planes */
	if (param->pipes & ~0x3) {
		DRM_ERROR("Invalid planes 0x%x, only <= 0x3 is valid\n",
			  param->pipes);
		return -EINVAL;
	}

	i915_dispatch_flip(dev, param->pipes, 0);

	return 0;
}


static int i915_getparam(struct drm_device *dev, void *data,
			 struct drm_file *file_priv)
{
	struct drm_i915_private *dev_priv = dev->dev_private;
	struct drm_i915_getparam *param = data;
	int value;

	if (!dev_priv) {
		DRM_ERROR("called with no initialization\n");
		return -EINVAL;
	}

	switch (param->param) {
	case I915_PARAM_IRQ_ACTIVE:
		value = dev->irq ? 1 : 0;
		break;
	case I915_PARAM_ALLOW_BATCHBUFFER:
		value = dev_priv->allow_batchbuffer ? 1 : 0;
		break;
	case I915_PARAM_LAST_DISPATCH:
		value = READ_BREADCRUMB(dev_priv);
		break;
	case I915_PARAM_CHIPSET_ID:
		value = dev->pci_device;
		break;
	default:
		DRM_ERROR("Unknown parameter %d\n", param->param);
		return -EINVAL;
	}

	if (DRM_COPY_TO_USER(param->value, &value, sizeof(int))) {
		DRM_ERROR("DRM_COPY_TO_USER failed\n");
		return -EFAULT;
	}

	return 0;
}

static int i915_setparam(struct drm_device *dev, void *data,
			 struct drm_file *file_priv)
{
	struct drm_i915_private *dev_priv = dev->dev_private;
	drm_i915_setparam_t *param = data;

	if (!dev_priv) {
		DRM_ERROR("called with no initialization\n");
		return -EINVAL;
	}

	switch (param->param) {
	case I915_SETPARAM_USE_MI_BATCHBUFFER_START:
		if (!IS_I965G(dev))
			dev_priv->use_mi_batchbuffer_start = param->value;
		break;
	case I915_SETPARAM_TEX_LRU_LOG_GRANULARITY:
		dev_priv->tex_lru_log_granularity = param->value;
		break;
	case I915_SETPARAM_ALLOW_BATCHBUFFER:
		dev_priv->allow_batchbuffer = param->value;
		break;
	default:
		DRM_ERROR("unknown parameter %d\n", param->param);
		return -EINVAL;
	}

	return 0;
}

drm_i915_mmio_entry_t mmio_table[] = {
	[MMIO_REGS_PS_DEPTH_COUNT] = {
		I915_MMIO_MAY_READ|I915_MMIO_MAY_WRITE,
		0x2350,
		8
	},
	[MMIO_REGS_DOVSTA] = {
		I915_MMIO_MAY_READ,
		0x30008,
		1
	},
	[MMIO_REGS_GAMMA] = {
		I915_MMIO_MAY_READ|I915_MMIO_MAY_WRITE,
		0x30010,
		6
	}
};

static int mmio_table_size = sizeof(mmio_table)/sizeof(drm_i915_mmio_entry_t);

static int i915_mmio(struct drm_device *dev, void *data,
		     struct drm_file *file_priv)
{
	uint32_t buf[8];
	struct drm_i915_private *dev_priv = dev->dev_private;
	drm_i915_mmio_entry_t *e;	 
	drm_i915_mmio_t *mmio = data;
	void __iomem *base;
	int i;

	if (!dev_priv) {
		DRM_ERROR("called with no initialization\n");
		return -EINVAL;
	}

	if (mmio->reg >= mmio_table_size)
		return -EINVAL;

	e = &mmio_table[mmio->reg];
	base = (u8 *) dev_priv->mmio_map->handle + e->offset;

	switch (mmio->read_write) {
	case I915_MMIO_READ:
		if (!(e->flag & I915_MMIO_MAY_READ))
			return -EINVAL;
		for (i = 0; i < e->size / 4; i++)
			buf[i] = I915_READ(e->offset + i * 4);
		if (DRM_COPY_TO_USER(mmio->data, buf, e->size)) {
			DRM_ERROR("DRM_COPY_TO_USER failed\n");
			return -EFAULT;
		}
		break;
		
	case I915_MMIO_WRITE:
		if (!(e->flag & I915_MMIO_MAY_WRITE))
			return -EINVAL;
		if (DRM_COPY_FROM_USER(buf, mmio->data, e->size)) {
			DRM_ERROR("DRM_COPY_TO_USER failed\n");
			return -EFAULT;
		}
		for (i = 0; i < e->size / 4; i++)
			I915_WRITE(e->offset + i * 4, buf[i]);
		break;
	}
	return 0;
}

static int i915_set_status_page(struct drm_device *dev, void *data,
				struct drm_file *file_priv)
{
	struct drm_i915_private *dev_priv = dev->dev_private;
	drm_i915_hws_addr_t *hws = data;

	if (!dev_priv) {
		DRM_ERROR("called with no initialization\n");
		return -EINVAL;
	}
	DRM_DEBUG("set status page addr 0x%08x\n", (u32)hws->addr);

	dev_priv->status_gfx_addr = hws->addr & (0x1ffff<<12);

	dev_priv->hws_map.offset = dev->agp->base + hws->addr;
	dev_priv->hws_map.size = 4*1024;
	dev_priv->hws_map.type = 0;
	dev_priv->hws_map.flags = 0;
	dev_priv->hws_map.mtrr = 0;

	drm_core_ioremap(&dev_priv->hws_map, dev);
	if (dev_priv->hws_map.handle == NULL) {
		i915_dma_cleanup(dev);
		dev_priv->status_gfx_addr = 0;
		DRM_ERROR("can not ioremap virtual address for"
				" G33 hw status page\n");
		return -ENOMEM;
	}
	dev_priv->hw_status_page = dev_priv->hws_map.handle;

	memset(dev_priv->hw_status_page, 0, PAGE_SIZE);
	I915_WRITE(I915REG_HWS_PGA, dev_priv->status_gfx_addr);
	DRM_DEBUG("load hws 0x2080 with gfx mem 0x%x\n",
			dev_priv->status_gfx_addr);
	DRM_DEBUG("load hws at %p\n", dev_priv->hw_status_page);
	return 0;
}

struct drm_ioctl_desc i915_ioctls[] = {
	DRM_IOCTL_DEF(DRM_I915_INIT, i915_dma_init, DRM_AUTH|DRM_MASTER|DRM_ROOT_ONLY),
	DRM_IOCTL_DEF(DRM_I915_FLUSH, i915_flush_ioctl, DRM_AUTH),
	DRM_IOCTL_DEF(DRM_I915_FLIP, i915_flip_bufs, DRM_AUTH),
	DRM_IOCTL_DEF(DRM_I915_BATCHBUFFER, i915_batchbuffer, DRM_AUTH),
	DRM_IOCTL_DEF(DRM_I915_IRQ_EMIT, i915_irq_emit, DRM_AUTH),
	DRM_IOCTL_DEF(DRM_I915_IRQ_WAIT, i915_irq_wait, DRM_AUTH),
	DRM_IOCTL_DEF(DRM_I915_GETPARAM, i915_getparam, DRM_AUTH),
	DRM_IOCTL_DEF(DRM_I915_SETPARAM, i915_setparam, DRM_AUTH|DRM_MASTER|DRM_ROOT_ONLY),
	DRM_IOCTL_DEF(DRM_I915_ALLOC, i915_mem_alloc, DRM_AUTH),
	DRM_IOCTL_DEF(DRM_I915_FREE, i915_mem_free, DRM_AUTH),
	DRM_IOCTL_DEF(DRM_I915_INIT_HEAP, i915_mem_init_heap, DRM_AUTH|DRM_MASTER|DRM_ROOT_ONLY),
	DRM_IOCTL_DEF(DRM_I915_CMDBUFFER, i915_cmdbuffer, DRM_AUTH),
	DRM_IOCTL_DEF(DRM_I915_DESTROY_HEAP,  i915_mem_destroy_heap, DRM_AUTH|DRM_MASTER|DRM_ROOT_ONLY ),
	DRM_IOCTL_DEF(DRM_I915_SET_VBLANK_PIPE,  i915_vblank_pipe_set, DRM_AUTH|DRM_MASTER|DRM_ROOT_ONLY ),
	DRM_IOCTL_DEF(DRM_I915_GET_VBLANK_PIPE,  i915_vblank_pipe_get, DRM_AUTH ),
	DRM_IOCTL_DEF(DRM_I915_VBLANK_SWAP, i915_vblank_swap, DRM_AUTH),
	DRM_IOCTL_DEF(DRM_I915_MMIO, i915_mmio, DRM_AUTH),
	DRM_IOCTL_DEF(DRM_I915_HWS_ADDR, i915_set_status_page, DRM_AUTH),
#ifdef I915_HAVE_BUFFER
	DRM_IOCTL_DEF(DRM_I915_EXECBUFFER, i915_execbuffer, DRM_AUTH),
#endif
};

int i915_max_ioctl = DRM_ARRAY_SIZE(i915_ioctls);

/**
 * Determine if the device really is AGP or not.
 *
 * All Intel graphics chipsets are treated as AGP, even if they are really
 * PCI-e.
 *
 * \param dev   The device to be tested.
 *
 * \returns
 * A value of 1 is always retured to indictate every i9x5 is AGP.
 */
int i915_driver_device_is_agp(struct drm_device * dev)
{
	return 1;
}

