/*
 * Copyright 2008 Jerome Glisse.
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
#include "radeon_drm.h"
#include "radeon_drv.h"
#include "r300_reg.h"

int radeon_cs_ioctl(struct drm_device *dev, void *data, struct drm_file *fpriv)
{
	struct drm_radeon_private *radeon = dev->dev_private;
	struct drm_radeon_cs *cs = data;
	uint32_t *packets = NULL;
	uint32_t cs_id;
	void *ib = NULL;
	long size;
	int r;

	/* set command stream id to 0 which is fake id */
	cs_id = 0;
	DRM_COPY_TO_USER(&cs->cs_id, &cs_id, sizeof(uint32_t));

	if (radeon == NULL) {
		DRM_ERROR("called with no initialization\n");
		return -EINVAL;
	}
	if (!cs->dwords) {
		return 0;
	}
	/* limit cs to 64K ib */
	if (cs->dwords > (16 * 1024)) {
		return -EINVAL;
	}
	/* copy cs from userspace maybe we should copy into ib to save
	 * one copy but ib will be mapped wc so not good for cmd checking
	 * somethings worth testing i guess (Jerome)
	 */
	size = cs->dwords * sizeof(uint32_t);
	packets = drm_alloc(size, DRM_MEM_DRIVER);
	if (packets == NULL) {
		return -ENOMEM;
	}
	if (DRM_COPY_FROM_USER(packets, (void __user *)(unsigned long)cs->packets, size)) {
		r = -EFAULT;
		goto out;
	}
	/* get ib */
	r = radeon->cs.ib_get(dev, &ib, cs->dwords);
	if (r) {
		goto out;
	}

	/* now parse command stream */
	r = radeon->cs.parse(dev, ib, packets, cs->dwords);
	if (r) {
		goto out;
	}

	/* emit cs id sequence */
	radeon->cs.id_emit(dev, &cs_id);
	DRM_COPY_TO_USER(&cs->cs_id, &cs_id, sizeof(uint32_t));
out:
	radeon->cs.ib_free(dev, ib, cs->dwords);
	drm_free(packets, size, DRM_MEM_DRIVER);
	return r;
}

int radeon_cs_parse(struct drm_device *dev, void *ib,
		    uint32_t *packets, uint32_t dwords)
{
	drm_radeon_private_t *dev_priv = dev->dev_private;
	volatile int rb;

	/* copy the packet into the IB */
	memcpy(ib, packets, dwords * sizeof(uint32_t));

	/* read back last byte to flush WC buffers */
	rb = readl((ib + (dwords-1) * sizeof(uint32_t)));

	return 0;
}

uint32_t radeon_cs_id_get(struct drm_radeon_private *radeon)
{
	/* FIXME: protect with a spinlock */
	/* FIXME: check if wrap affect last reported wrap & sequence */
	radeon->cs.id_scnt = (radeon->cs.id_scnt + 1) & 0x00FFFFFF;
	if (!radeon->cs.id_scnt) {
		/* increment wrap counter */
		radeon->cs.id_wcnt += 0x01000000;
		/* valid sequence counter start at 1 */
		radeon->cs.id_scnt = 1;
	}
	return (radeon->cs.id_scnt | radeon->cs.id_wcnt);
}

void r100_cs_id_emit(struct drm_device *dev, uint32_t *id)
{
	drm_radeon_private_t *dev_priv = dev->dev_private;
	RING_LOCALS;

	/* ISYNC_CNTL should have CPSCRACTH bit set */
	*id = radeon_cs_id_get(dev_priv);
	/* emit id in SCRATCH4 (not used yet in old drm) */
	BEGIN_RING(2);
	OUT_RING(CP_PACKET0(RADEON_SCRATCH_REG4, 0));
	OUT_RING(*id);
	ADVANCE_RING();	
}

void r300_cs_id_emit(struct drm_device *dev, uint32_t *id)
{
	drm_radeon_private_t *dev_priv = dev->dev_private;
	RING_LOCALS;

	/* ISYNC_CNTL should not have CPSCRACTH bit set */
	*id = radeon_cs_id_get(dev_priv);
	/* emit id in SCRATCH6 */
	BEGIN_RING(6);
	OUT_RING(CP_PACKET0(R300_CP_RESYNC_ADDR, 0));
	OUT_RING(6);
	OUT_RING(CP_PACKET0(R300_CP_RESYNC_DATA, 0));
	OUT_RING(*id);
	OUT_RING(CP_PACKET0(R300_RB3D_DSTCACHE_CTLSTAT, 0));
	OUT_RING(R300_RB3D_DC_FINISH);
	ADVANCE_RING();	
}

uint32_t r100_cs_id_last_get(struct drm_device *dev)
{
	drm_radeon_private_t *dev_priv = dev->dev_private;

	return RADEON_READ(RADEON_SCRATCH_REG4);
}

uint32_t r300_cs_id_last_get(struct drm_device *dev)
{
	drm_radeon_private_t *dev_priv = dev->dev_private;

	return RADEON_READ(RADEON_SCRATCH_REG6);
}

int radeon_cs_init(struct drm_device *dev)
{
	drm_radeon_private_t *dev_priv = dev->dev_private;

	if (dev_priv->chip_family < CHIP_RV280) {
		dev_priv->cs.id_emit = r100_cs_id_emit;
		dev_priv->cs.id_last_get = r100_cs_id_last_get;
	} else if (dev_priv->chip_family < CHIP_R600) {
		dev_priv->cs.id_emit = r300_cs_id_emit;
		dev_priv->cs.id_last_get = r300_cs_id_last_get;
	}

	dev_priv->cs.parse = radeon_cs_parse;
	/* ib get depends on memory manager or not so memory manager */
	return 0;
}
