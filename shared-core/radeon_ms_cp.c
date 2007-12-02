/*
 * Copyright 2007 Jérôme Glisse
 * Copyright 2007 Dave Airlie
 * Copyright 2007 Alex Deucher
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
 */
/*
 * Authors:
 *    Jerome Glisse <glisse@freedesktop.org>
 */
#include "radeon_ms.h"

static int radeon_ms_test_ring_buffer(struct drm_device *dev)
{
	struct drm_radeon_private *dev_priv = dev->dev_private;
	int i, ret;
	uint32_t cmd[4];

	MMIO_W(SCRATCH_REG4, 0);
	cmd[0] = CP_PACKET0(SCRATCH_REG4, 0);
	cmd[1] = 0xdeadbeef;
	cmd[2] = CP_PACKET0(WAIT_UNTIL, 0);
	cmd[3] = WAIT_UNTIL__WAIT_2D_IDLECLEAN |
		WAIT_UNTIL__WAIT_HOST_IDLECLEAN;
	DRM_MEMORYBARRIER();
	ret = radeon_ms_ring_emit(dev, cmd, 4);
	if (ret) {
		return 0;
	}
	DRM_UDELAY(100);

	for (i = 0; i < dev_priv->usec_timeout; i++) {
		if (MMIO_R(SCRATCH_REG4) == 0xdeadbeef) {
			DRM_INFO("[radeon_ms] cp test succeeded in %d usecs\n",
				 i);
			return 1;
		}
		DRM_UDELAY(1);
	}
	DRM_INFO("[radeon_ms] cp test failed\n");
	return 0;
}

static int radeon_ms_test_write_back(struct drm_device *dev)
{
	struct drm_radeon_private *dev_priv = dev->dev_private;
	uint32_t tmp;

	if (dev_priv->ring_buffer_object == NULL ||
	    dev_priv->ring_buffer == NULL)
	    return 0;
	dev_priv->write_back_area[0] = 0x0;
	MMIO_W(SCRATCH_REG0, 0xdeadbeef);
	for (tmp = 0; tmp < dev_priv->usec_timeout; tmp++) {
		if (dev_priv->write_back_area[0] == 0xdeadbeef)
			break;
		DRM_UDELAY(1);
	}
	if (tmp < dev_priv->usec_timeout) {
		DRM_INFO("[radeon_ms] writeback test succeeded in %d usecs\n",
			 tmp);
		return 1;
	}
	MMIO_W(SCRATCH_UMSK, 0x0);
	DRM_INFO("[radeon_ms] writeback test failed\n");
	return 0;
}

static __inline__ void radeon_ms_load_mc(struct drm_device *dev)
{
	struct drm_radeon_private *dev_priv = dev->dev_private;
	int i;

	MMIO_W(CP_ME_RAM_ADDR, 0);
	for (i = 0; i < 256; i++) {
		MMIO_W(CP_ME_RAM_DATAH, dev_priv->microcode[(i * 2) + 1]);
		MMIO_W(CP_ME_RAM_DATAL, dev_priv->microcode[(i * 2) + 0]);
	}
}

int radeon_ms_cp_finish(struct drm_device *dev)
{
	struct drm_radeon_private *dev_priv = dev->dev_private;

	dev_priv->cp_ready = 0;
	radeon_ms_wait_for_idle(dev);
	DRM_INFO("[radeon_ms] cp idle\n");
	radeon_ms_cp_stop(dev);

	DRM_INFO("[radeon_ms] ring buffer %p\n", dev_priv->ring_buffer);
	if (dev_priv->ring_buffer) {
		drm_bo_kunmap(&dev_priv->ring_buffer_map);
	}
	dev_priv->ring_buffer = NULL;
	DRM_INFO("[radeon_ms] ring buffer object %p\n", dev_priv->ring_buffer_object);
	if (dev_priv->ring_buffer_object) {
		mutex_lock(&dev->struct_mutex);
		drm_bo_usage_deref_locked(&dev_priv->ring_buffer_object);
		mutex_unlock(&dev->struct_mutex);
	}
	return 0;
}

int radeon_ms_cp_init(struct drm_device *dev)
{
	struct drm_radeon_private *dev_priv = dev->dev_private;
	struct radeon_state *state = &dev_priv->driver_state;
	int ret = 0;

	if (dev_priv->microcode == NULL) {
		DRM_INFO("[radeon_ms] no microcode not starting cp");
		return 0;
	}
	/* we allocate an extra page for all write back stuff */
	ret = drm_buffer_object_create(dev,
			dev_priv->ring_buffer_size +
			dev_priv->write_back_area_size,
			drm_bo_type_kernel,
			DRM_BO_FLAG_READ |
			DRM_BO_FLAG_WRITE |
			DRM_BO_FLAG_MEM_TT |
			DRM_BO_FLAG_NO_EVICT,
			DRM_BO_HINT_DONT_FENCE,
			1,
			0,
			&dev_priv->ring_buffer_object);
	if (ret) {
		return ret;
	}
	memset(&dev_priv->ring_buffer_map, 0, sizeof(struct drm_bo_kmap_obj));
	ret = drm_bo_kmap(dev_priv->ring_buffer_object,
			dev_priv->ring_buffer_object->mem.mm_node->start,
			dev_priv->ring_buffer_object->mem.num_pages,
			&dev_priv->ring_buffer_map);
	if (ret) {
		DRM_ERROR("[radeon_ms] error mapping ring buffer: %d\n", ret);
		return ret;
	}
	dev_priv->ring_buffer = dev_priv->ring_buffer_map.virtual; 
	dev_priv->write_back_area =
		&dev_priv->ring_buffer[dev_priv->ring_buffer_size >> 2];
	/* setup write back offset */
	state->scratch_umsk = 0x7; 
	state->scratch_addr = 
		REG_S(SCRATCH_ADDR, SCRATCH_ADDR,
				(dev_priv->ring_buffer_object->offset +
				 dev_priv->ring_buffer_size +
				 dev_priv->gpu_gart_start) >> 5);
	MMIO_W(SCRATCH_ADDR, state->scratch_addr);
	MMIO_W(SCRATCH_UMSK, REG_S(SCRATCH_UMSK, SCRATCH_UMSK, 0x7));
	DRM_INFO("[radeon_ms] write back at 0x%08X in gpu space\n",
		 MMIO_R(SCRATCH_ADDR));
	dev_priv->write_back = radeon_ms_test_write_back(dev);

	/* stop cp so it's in know state */
	radeon_ms_cp_stop(dev);
	if (dev_priv->ring_rptr) {
		DRM_INFO("[radeon_ms] failed to set cp read ptr to 0\n");
	} else {
		DRM_INFO("[radeon_ms] set cp read ptr to 0\n");
	}
	dev_priv->ring_mask = (dev_priv->ring_buffer_size / 4) - 1;

	/* load microcode */
	DRM_INFO("[radeon_ms] load microcode\n");
	radeon_ms_load_mc(dev);
	/* initialize CP registers */
	state->cp_rb_cntl =
		REG_S(CP_RB_CNTL, RB_BUFSZ,
				drm_order(dev_priv->ring_buffer_size / 8)) |
		REG_S(CP_RB_CNTL, RB_BLKSZ, drm_order(4096 / 8)) |
		REG_S(CP_RB_CNTL, MAX_FETCH, 2);
	if (!dev_priv->write_back) {
		state->cp_rb_cntl |= CP_RB_CNTL__RB_NO_UPDATE;
	}
	state->cp_rb_base =
		REG_S(CP_RB_BASE, RB_BASE,
				(dev_priv->ring_buffer_object->offset +
				 dev_priv->gpu_gart_start) >> 2);
	/* read ptr writeback just after the
	 * 8 scratch registers 32 = 8*4 */
	state->cp_rb_rptr_addr =
		REG_S(CP_RB_RPTR_ADDR, RB_RPTR_ADDR,
				(dev_priv->ring_buffer_object->offset +
				 dev_priv->ring_buffer_size + 32 +
				 dev_priv->gpu_gart_start) >> 2);
	state->cp_rb_wptr = dev_priv->ring_wptr;
	state->cp_rb_wptr_delay =
		REG_S(CP_RB_WPTR_DELAY, PRE_WRITE_TIMER, 64) |
		REG_S(CP_RB_WPTR_DELAY, PRE_WRITE_LIMIT, 8);
	state->cp_rb_wptr_delay = 0; 

	radeon_ms_cp_restore(dev, state);
	DRM_INFO("[radeon_ms] ring buffer at 0x%08X in gpu space\n",
		 MMIO_R(CP_RB_BASE));

	/* compute free space */
	dev_priv->ring_free = 0;
	ret = radeon_ms_cp_wait(dev, 64);
	if (ret) {
		/* we shouldn't fail here */
		DRM_INFO("[radeon_ms] failed to get ring free space\n");
		return ret;
	}
	DRM_INFO("[radeon_ms] free ring size: %d\n", dev_priv->ring_free * 4);

	MMIO_W(CP_CSQ_CNTL, REG_S(CP_CSQ_CNTL, CSQ_MODE,
				CSQ_MODE__CSQ_PRIBM_INDBM));
	if (!radeon_ms_test_ring_buffer(dev)) {
		DRM_INFO("[radeon_ms] cp doesn't work\n");
		/* disable ring should wait idle before */
		radeon_ms_cp_stop(dev);
		return -EBUSY;
	}
	/* waaooo the cp is ready & working */
	DRM_INFO("[radeon_ms] cp ready, enjoy\n");
	dev_priv->cp_ready = 1;
	return 0;
}

void radeon_ms_cp_restore(struct drm_device *dev, struct radeon_state *state)
{
	struct drm_radeon_private *dev_priv = dev->dev_private;

	radeon_ms_wait_for_idle(dev);
	MMIO_W(SCRATCH_ADDR, state->scratch_addr);
	MMIO_W(SCRATCH_UMSK, state->scratch_umsk);
	MMIO_W(CP_RB_BASE, state->cp_rb_base);
	MMIO_W(CP_RB_RPTR_ADDR, state->cp_rb_rptr_addr);
	MMIO_W(CP_RB_WPTR_DELAY, state->cp_rb_wptr_delay);
	MMIO_W(CP_RB_CNTL, state->cp_rb_cntl);
	/* Sync everything up */
	MMIO_W(ISYNC_CNTL, ISYNC_CNTL__ISYNC_ANY2D_IDLE3D |
			   ISYNC_CNTL__ISYNC_ANY3D_IDLE2D |
			   ISYNC_CNTL__ISYNC_WAIT_IDLEGUI |
			   ISYNC_CNTL__ISYNC_CPSCRATCH_IDLEGUI);
}

void radeon_ms_cp_save(struct drm_device *dev, struct radeon_state *state)
{
	struct drm_radeon_private *dev_priv = dev->dev_private;

	state->scratch_addr = MMIO_R(SCRATCH_ADDR);
	state->scratch_umsk = MMIO_R(SCRATCH_UMSK);
	state->cp_rb_base = MMIO_R(CP_RB_BASE);
	state->cp_rb_rptr_addr = MMIO_R(CP_RB_RPTR_ADDR);
	state->cp_rb_wptr_delay = MMIO_R(CP_RB_WPTR_DELAY);
	state->cp_rb_wptr = MMIO_R(CP_RB_WPTR);
	state->cp_rb_cntl = MMIO_R(CP_RB_CNTL);
}

void radeon_ms_cp_stop(struct drm_device *dev)
{
	struct drm_radeon_private *dev_priv = dev->dev_private;

	MMIO_W(CP_CSQ_CNTL, REG_S(CP_CSQ_CNTL, CSQ_MODE,
				CSQ_MODE__CSQ_PRIDIS_INDDIS));
	MMIO_W(CP_RB_CNTL, CP_RB_CNTL__RB_RPTR_WR_ENA);
	MMIO_W(CP_RB_RPTR_WR, 0);
	MMIO_W(CP_RB_WPTR, 0);
	DRM_UDELAY(5);
	dev_priv->ring_wptr = dev_priv->ring_rptr = MMIO_R(CP_RB_RPTR);
	MMIO_W(CP_RB_WPTR, dev_priv->ring_wptr);
}

int radeon_ms_cp_wait(struct drm_device *dev, int n)
{
	struct drm_radeon_private *dev_priv = dev->dev_private;
	uint32_t i, last_rptr, p = 0;

	last_rptr = MMIO_R(CP_RB_RPTR);
	for (i = 0; i < dev_priv->usec_timeout; i++) {
		dev_priv->ring_rptr = MMIO_R(CP_RB_RPTR);
		if (last_rptr != dev_priv->ring_rptr) {
			/* the ring is progressing no lockup */
			p = 1;
		}
		dev_priv->ring_free = (((int)dev_priv->ring_rptr) -
				       ((int)dev_priv->ring_wptr));
		if (dev_priv->ring_free <= 0)
			dev_priv->ring_free += (dev_priv->ring_buffer_size / 4);
		if (dev_priv->ring_free > n)
			return 0;
		last_rptr = dev_priv->ring_rptr;
		DRM_UDELAY(1);
	}
	if (p) {
		DRM_INFO("[radeon_ms] timed out waiting free slot\n");
	} else {
		DRM_INFO("[radeon_ms] cp have lickely locked up\n");
	}
	return -EBUSY;
}

int radeon_ms_ring_emit(struct drm_device *dev, uint32_t *cmd, uint32_t count)
{
	static spinlock_t ring_lock = SPIN_LOCK_UNLOCKED;
	struct drm_radeon_private *dev_priv = dev->dev_private;
	uint32_t i = 0;

	if (!count)
		return -EINVAL;

	spin_lock(&ring_lock);
	if (dev_priv->ring_free <= (count)) {
		spin_unlock(&ring_lock);
		return -EBUSY;
	}
	dev_priv->ring_free -= count;
	for (i = 0; i < count; i++) {
		dev_priv->ring_buffer[dev_priv->ring_wptr] = cmd[i];
		dev_priv->ring_wptr++;
		dev_priv->ring_wptr &= dev_priv->ring_mask;
	}
	/* commit ring */
	DRM_MEMORYBARRIER();
	MMIO_W(CP_RB_WPTR, REG_S(CP_RB_WPTR, RB_WPTR, dev_priv->ring_wptr));
	/* read from PCI bus to ensure correct posting */
	MMIO_R(CP_RB_WPTR);
	spin_unlock(&ring_lock);
	return 0;
}
