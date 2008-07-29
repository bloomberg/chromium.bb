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
	struct drm_radeon_private *dev_priv = dev->dev_private;
	struct drm_radeon_cs *cs = data;
	uint32_t *packets = NULL;
	uint32_t cs_id;
	uint32_t card_offset;
	void *ib = NULL;
	long size;
	int r;
	RING_LOCALS;

	/* set command stream id to 0 which is fake id */
	cs_id = 0;
	DRM_COPY_TO_USER(&cs->cs_id, &cs_id, sizeof(uint32_t));

	if (dev_priv == NULL) {
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
	r = dev_priv->cs.ib_get(dev, &ib, cs->dwords, &card_offset);
	if (r) {
		goto out;
	}

	/* now parse command stream */
	r = dev_priv->cs.parse(dev, fpriv, ib, packets, cs->dwords);
	if (r) {
		goto out;
	}

	BEGIN_RING(4);
	OUT_RING(CP_PACKET0(RADEON_CP_IB_BASE, 1));
	OUT_RING(card_offset);
	OUT_RING(cs->dwords);
	OUT_RING(CP_PACKET2());
	ADVANCE_RING();

	/* emit cs id sequence */
	dev_priv->cs.id_emit(dev, &cs_id);
	COMMIT_RING();

	DRM_COPY_TO_USER(&cs->cs_id, &cs_id, sizeof(uint32_t));
out:
	dev_priv->cs.ib_free(dev, ib, cs->dwords);
	drm_free(packets, size, DRM_MEM_DRIVER);
	return r;
}

/* for non-mm */
static int radeon_nomm_relocate(struct drm_device *dev, struct drm_file *file_priv, uint32_t *reloc, uint32_t *offset)
{
	*offset = reloc[1];
	return 0;
}
#define RELOC_SIZE 2
#define RADEON_2D_OFFSET_MASK 0x3fffff

static __inline__ int radeon_cs_relocate_packet0(struct drm_device *dev, struct drm_file *file_priv,
						 uint32_t *packets, uint32_t offset_dw)
{
	drm_radeon_private_t *dev_priv = dev->dev_private;
	uint32_t hdr = packets[offset_dw];
	uint32_t reg = (hdr & R300_CP_PACKET0_REG_MASK) << 2;
	uint32_t val = packets[offset_dw + 1];
	uint32_t packet3_hdr = packets[offset_dw+2];
	uint32_t tmp, offset;
	int ret;

	/* this is too strict we may want to expand the length in the future and have
	 old kernels ignore it. */ 
	if (packet3_hdr != (RADEON_CP_PACKET3 | RADEON_CP_NOP | (RELOC_SIZE << 16))) {
		DRM_ERROR("Packet 3 was %x should have been %x\n", packet3_hdr, RADEON_CP_PACKET3 | RADEON_CP_NOP | (RELOC_SIZE << 16));
		return -EINVAL;
	}
	
	switch(reg) {
	case RADEON_DST_PITCH_OFFSET:
	case RADEON_SRC_PITCH_OFFSET:
		/* pass in the start of the reloc */
		ret = dev_priv->cs.relocate(dev, file_priv, packets + offset_dw + 2, &offset);
		if (ret)
			return ret;
		tmp = (val & RADEON_2D_OFFSET_MASK) << 10;
		val &= ~RADEON_2D_OFFSET_MASK;
		offset += tmp;
		offset >>= 10;
		val |= offset;
		break;
	case R300_RB3D_COLOROFFSET0:
	case R300_RB3D_DEPTHOFFSET:
	case R300_TX_OFFSET_0:
	case R300_TX_OFFSET_0+4:
	        ret = dev_priv->cs.relocate(dev, file_priv, packets + offset_dw + 2, &offset);
		if (ret)
			return ret;

		offset &= 0xffffffe0;
		val += offset;
		break;
	default:
		break;
	}


	DRM_ERROR("New offset %x %x %x\n", packets[offset_dw+1], val, offset);
	packets[offset_dw + 1] = val;

	return 0;
}

static int radeon_cs_relocate_packet3(struct drm_device *dev, struct drm_file *file_priv,
				      uint32_t *packets, uint32_t offset_dw)
{
	drm_radeon_private_t *dev_priv = dev->dev_private;
	uint32_t hdr = packets[offset_dw];
	int num_dw = (hdr & RADEON_CP_PACKET_COUNT_MASK) >> 16;
	uint32_t reg = hdr & 0xff00;
	uint32_t offset, val, tmp;
	int ret;

	switch(reg) {
	case RADEON_CNTL_HOSTDATA_BLT:
	{
		val = packets[offset_dw + 2];
		ret = dev_priv->cs.relocate(dev, file_priv, packets + offset_dw + num_dw + 2, &offset);
		if (ret)
			return ret;

		tmp = (val & RADEON_2D_OFFSET_MASK) << 10;
		val &= ~RADEON_2D_OFFSET_MASK;
		offset += tmp;
		offset >>= 10;
		val |= offset;

		DRM_ERROR("New offset %x %x %x\n", packets[offset_dw+2], val, offset);
		packets[offset_dw + 2] = val;
	}
	default:
		return -EINVAL;
	}
	return 0;
}

static __inline__ int radeon_cs_check_offset(struct drm_device *dev,
					     uint32_t reg, uint32_t val)
{
	uint32_t offset;

	switch(reg) {
	case RADEON_DST_PITCH_OFFSET:
	case RADEON_SRC_PITCH_OFFSET:
		offset = val & ((1 << 22) - 1);
		offset <<= 10;
		break;
	case R300_RB3D_COLOROFFSET0:
	case R300_RB3D_DEPTHOFFSET:
		offset = val;
		break;
	case R300_TX_OFFSET_0:
	case R300_TX_OFFSET_0+4:
		offset = val & 0xffffffe0;
		break;
	}
	
	DRM_ERROR("Offset check %x %x\n", reg, offset);
	return 0;
}

int radeon_cs_packet0(struct drm_device *dev, struct drm_file *file_priv,
		      uint32_t *packets, uint32_t offset_dw)
{
	drm_radeon_private_t *dev_priv = dev->dev_private;
	uint32_t hdr = packets[offset_dw];
	int num_dw = ((hdr & RADEON_CP_PACKET_COUNT_MASK) >> 16) + 2;
	int need_reloc = 0;
	int reg = (hdr & R300_CP_PACKET0_REG_MASK) << 2;
	int count_dw = 1;
	int ret;

	while (count_dw < num_dw) {
		/* need to have something like the r300 validation here - 
		   list of allowed registers */
		int flags;

		ret = r300_check_range(reg, 1);
		switch(ret) {
		case -1:
			DRM_ERROR("Illegal register %x\n", reg);
			break;
		case 0:
			break;
		case 1:
			flags = r300_get_reg_flags(reg);
			if (flags == MARK_CHECK_OFFSET) {
				if (num_dw > 2) {
					DRM_ERROR("Cannot relocate inside type stream of reg0 packets\n");
					return -EINVAL;
				}

				ret = radeon_cs_relocate_packet0(dev, file_priv, packets, offset_dw);
				if (ret)
					return ret;
				DRM_DEBUG("need to relocate %x %d\n", reg, flags);
				/* okay it should be followed by a NOP */
			} else if (flags == MARK_CHECK_SCISSOR) {
				DRM_DEBUG("need to validate scissor %x %d\n", reg, flags);
			} else {
				DRM_DEBUG("illegal register %x %d\n", reg, flags);
				return -EINVAL;
			}
			break;
		}
		count_dw++;
		reg += 4;
	}
	return 0;
}

int radeon_cs_parse(struct drm_device *dev, struct drm_file *file_priv,
		    void *ib, uint32_t *packets, uint32_t dwords)
{
	drm_radeon_private_t *dev_priv = dev->dev_private;
	volatile int rb;
	int size_dw = dwords;
	/* scan the packet for various things */
	int count_dw = 0;
	int ret = 0;

	while (count_dw < size_dw && ret == 0) {
		int hdr = packets[count_dw];
		int num_dw = (hdr & RADEON_CP_PACKET_COUNT_MASK) >> 16;
		int reg;

		switch (hdr & RADEON_CP_PACKET_MASK) {
		case RADEON_CP_PACKET0:
			ret = radeon_cs_packet0(dev, file_priv, packets, count_dw);
			break;
		case RADEON_CP_PACKET1:
		case RADEON_CP_PACKET2:
			reg = hdr & RADEON_CP_PACKET0_REG_MASK;
			DRM_DEBUG("Packet 1/2: %d  %x\n", num_dw, reg);
			break;

		case RADEON_CP_PACKET3:
			reg = hdr & 0xff00;
			
			switch(reg) {
			case RADEON_CNTL_HOSTDATA_BLT:
				radeon_cs_relocate_packet3(dev, file_priv, packets, count_dw);
				break;

			case RADEON_CNTL_BITBLT_MULTI:
			case RADEON_3D_LOAD_VBPNTR:	/* load vertex array pointers */
			case RADEON_CP_INDX_BUFFER:
				DRM_ERROR("need relocate packet 3 for %x\n", reg);
				break;

			case RADEON_CP_3D_DRAW_IMMD_2:	/* triggers drawing using in-packet vertex data */
			case RADEON_CP_3D_DRAW_VBUF_2:	/* triggers drawing of vertex buffers setup elsewhere */
			case RADEON_CP_3D_DRAW_INDX_2:	/* triggers drawing using indices to vertex buffer */
			case RADEON_WAIT_FOR_IDLE:
			case RADEON_CP_NOP:
				break;
			default:
				DRM_ERROR("unknown packet 3 %x\n", reg);
				ret = -EINVAL;
			}
			break;
		}

		count_dw += num_dw+2;
	}

	if (ret)
		return ret;
	     

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
	dev_priv->cs.relocate = radeon_nomm_relocate;
	return 0;
}
