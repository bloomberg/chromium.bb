/* savage_state.c -- State and drawing support for Savage
 *
 * Copyright 2004  Felix Kuehling
 * All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sub license,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial portions
 * of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NON-INFRINGEMENT. IN NO EVENT SHALL FELIX KUEHLING BE LIABLE FOR
 * ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF
 * CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */
#include "drmP.h"
#include "savage_drm.h"
#include "savage_drv.h"

void savage_emit_cliprect_s3d(drm_savage_private_t *dev_priv,
			      drm_clip_rect_t *pbox)
{
	
}

void savage_emit_cliprect_s4(drm_savage_private_t *dev_priv,
			     drm_clip_rect_t *pbox)
{
	
}

static int savage_verify_texaddr(drm_savage_private_t *dev_priv, int unit,
				 uint32_t addr)
{
	if ((addr & 6) != 2) { /* reserved bits */
		DRM_ERROR("bad texAddr%d %08x (reserved bits)\n", unit, addr);
		return DRM_ERR(EINVAL);
	}
	if (!(addr & 1)) { /* local */
		addr &= ~7;
		if (addr <  dev_priv->texture_offset ||
		    addr >= dev_priv->texture_offset+dev_priv->texture_size) {
			DRM_ERROR("bad texAddr%d %08x (local addr out of range)\n",
				  unit, addr);
			return DRM_ERR(EINVAL);
		}
	} else { /* AGP */
		if (!dev_priv->agp_textures) {
			DRM_ERROR("bad texAddr%d %08x (AGP not available)\n",
				  unit, addr);
			return DRM_ERR(EINVAL);
		}
		addr &= ~7;
		if (addr < dev_priv->agp_textures->offset ||
		    addr >= (dev_priv->agp_textures->offset +
			     dev_priv->agp_textures->size)) {
			DRM_ERROR("bad texAddr%d %08x (AGP addr out of range)\n",
				  unit, addr);
			return DRM_ERR(EINVAL);
		}
	}
	return 0;
}

static int savage_verify_state_s3d(drm_savage_private_t *dev_priv,
				   unsigned int start, unsigned int count,
				   const uint32_t __user *regs)
{
	if (start < SAVAGE_TEXPALADDR_S3D ||
	    start+count-1 > SAVAGE_DESTTEXRWWATERMARK_S3D) {
		DRM_ERROR("invalid register range (0x%04x-0x%04x)\n",
			  start, start+count-1);
		return DRM_ERR(EINVAL);
	}

	if (start <= SAVAGE_TEXCTRL_S3D && start+count > SAVAGE_TEXCTRL_S3D) {
		DRM_GET_USER_UNCHECKED(dev_priv->state.s3d.texctrl,
				       &regs[SAVAGE_TEXCTRL_S3D-start]);
	}

	if (dev_priv->state.s3d.texctrl & SAVAGE_TEXCTRL_TEXEN_MASK &&
	    start <= SAVAGE_TEXADDR_S3D && start+count > SAVAGE_TEXADDR_S3D) {
		uint32_t addr;
		DRM_GET_USER_UNCHECKED(addr, &regs[SAVAGE_TEXADDR_S3D-start]);
		return savage_verify_texaddr(dev_priv, 0, addr);
	}

	return 0;
}

static int savage_verify_state_s4(drm_savage_private_t *dev_priv,
				  unsigned int start, unsigned int count,
				  const uint32_t __user *regs)
{
	int ret = 0;

	if (start < SAVAGE_DRAWLOCALCTRL_S4 ||
	    start+count-1 > SAVAGE_TEXBLENDCOLOR_S4) {
		DRM_ERROR("invalid register range (0x%04x-0x%04x)\n",
			  start, start+count-1);
		return DRM_ERR(EINVAL);
	}

	if (start <= SAVAGE_TEXDESCR_S4 && start+count > SAVAGE_TEXDESCR_S4) {
		DRM_GET_USER_UNCHECKED(dev_priv->state.s4.texdescr,
				       &regs[SAVAGE_TEXDESCR_S4-start]);
	}

	if (dev_priv->state.s4.texdescr & SAVAGE_TEXDESCR_TEX0EN_MASK &&
	    start <= SAVAGE_TEXADDR0_S4 && start+count > SAVAGE_TEXADDR0_S4) {
		uint32_t addr;
		DRM_GET_USER_UNCHECKED(addr, &regs[SAVAGE_TEXADDR0_S4-start]);
		ret |= savage_verify_texaddr(dev_priv, 0, addr);
	}
	if (dev_priv->state.s4.texdescr & SAVAGE_TEXDESCR_TEX1EN_MASK &&
	    start <= SAVAGE_TEXADDR1_S4 && start+count > SAVAGE_TEXADDR1_S4) {
		uint32_t addr;
		DRM_GET_USER_UNCHECKED(addr, &regs[SAVAGE_TEXADDR1_S4-start]);
		ret |= savage_verify_texaddr(dev_priv, 1, addr);
	}

	return ret;
}

static int savage_dispatch_state(drm_savage_private_t *dev_priv,
				 const drm_savage_cmd_header_t *cmd_header,
				 const uint32_t __user *regs)
{
	BCI_LOCALS;
	unsigned int count = cmd_header->state.count;
	unsigned int start = cmd_header->state.start;
	unsigned int bci_size = count + (count+254)/255;
	int ret;

	if (DRM_VERIFYAREA_READ(regs, count*4))
		return DRM_ERR(EFAULT);

	if (S3_SAVAGE3D_SERIES(dev_priv->chipset)) {
		ret = savage_verify_state_s3d(dev_priv, start, count, regs);
		if (ret != 0)
			return ret;
	} else {
		ret = savage_verify_state_s4(dev_priv, start, count, regs);
		if (ret != 0)
			return ret;
	}

	if (cmd_header->state.global) {
		BEGIN_BCI(bci_size+1);
		BCI_WRITE(BCI_CMD_WAIT | BCI_CMD_WAIT_3D);
	} else {
		BEGIN_BCI(bci_size);
	}

	while (count > 0) {
		unsigned int n = count < 255 ? count : 255;
		BCI_SET_REGISTERS(start, n);
		BCI_COPY_FROM_USER(regs, n);
		count -= n;
		start += n;
		regs += n;
	}

	return 0;
}

static int savage_dispatch_dma_prim(drm_savage_private_t *dev_priv,
				    const drm_savage_cmd_header_t *cmd_header,
				    const drm_buf_t *dmabuf)
{
	BCI_LOCALS;
	unsigned char reorder = 0;
	unsigned char prim = cmd_header->prim.prim;
	unsigned short skip = cmd_header->prim.skip;
	unsigned int n = cmd_header->prim.count;
	unsigned int start = cmd_header->prim.start;
	unsigned int i;

	switch (prim) {
	case SAVAGE_PRIM_TRILIST_201:
		reorder = 1;
		prim = SAVAGE_PRIM_TRILIST;
	case SAVAGE_PRIM_TRILIST:
		if (n % 3 != 0) {
			DRM_ERROR("wrong number of vertices %u in TRILIST\n",
				  n);
			return DRM_ERR(EINVAL);
		}
		break;
	case SAVAGE_PRIM_TRISTRIP:
	case SAVAGE_PRIM_TRIFAN:
		if (n < 3) {
			DRM_ERROR("wrong number of vertices %u in TRIFAN/STRIP\n",
				  n);
			return DRM_ERR(EINVAL);
		}
		break;
	default:
		DRM_ERROR("invalid primitive type %u\n", prim);
		return DRM_ERR(EINVAL);
	}

	if (S3_SAVAGE3D_SERIES(dev_priv->chipset)) {
		if (skip != 0) {
			DRM_ERROR("invalid skip flags 0x%04x for DMA\n",
				  skip);
			return DRM_ERR(EINVAL);
		}
	} else {
		unsigned int size = 10 - (skip & 1) - (skip >> 1 & 1) -
			(skip >> 2 & 1) - (skip >> 3 & 1) - (skip >> 4 & 1) -
			(skip >> 5 & 1) - (skip >> 6 & 1) - (skip >> 7 & 1);
		if (skip > SAVAGE_SKIP_ALL_S4 || size != 8) {
			DRM_ERROR("invalid skip flags 0x%04x for DMA\n",
				  skip);
			return DRM_ERR(EINVAL);
		}
		if (reorder) {
			DRM_ERROR("TRILIST_201 used on Savage4 hardware\n");
			return DRM_ERR(EINVAL);
		}
	}

	if (start + n > dmabuf->total/32) {
		DRM_ERROR("vertex indices (%u-%u) out of range (0-%u)\n",
			  start, start + n - 1, dmabuf->total/32);
		return DRM_ERR(EINVAL);
	}

	if (dmabuf->bus_address != dev_priv->state.common.vbaddr) {
		BEGIN_BCI(2);
		BCI_SET_REGISTERS(SAVAGE_VERTBUFADDR, 1);
		BCI_WRITE(dmabuf->bus_address | dev_priv->dma_type);
		dev_priv->state.common.vbaddr = dmabuf->bus_address;
	}
	if (S3_SAVAGE3D_SERIES(dev_priv->chipset)) {
		/* Workaround for what looks like a hardware bug. If a
		 * WAIT_3D_IDLE was emitted some time before the
		 * indexed drawing command then the engine will lock
		 * up. There are two known workarounds:
		 * WAIT_IDLE_EMPTY or emit at least 63 NOPs. */
		BEGIN_BCI(63);
		for (i = 0; i < 63; ++i)
			BCI_WRITE(BCI_CMD_WAIT);
	}

	prim <<= 25;
	while (n != 0) {
		/* Can emit up to 255 indices (85 triangles) at once. */
		unsigned int count = n > 255 ? 255 : n;
		if (reorder) {
			/* Need to reorder indices for correct flat
			 * shading while preserving the clock sense
			 * for correct culling. Only on Savage3D. */
			int reorder[3] = {-1, -1, -1};
			reorder[start%3] = 2;

			BEGIN_BCI((count+1+1)/2);
			BCI_DRAW_INDICES_S3D(count, prim, start+2);

			for (i = start+1; i+1 < start+count; i += 2)
				BCI_WRITE((i + reorder[i % 3]) |
					  ((i+1 + reorder[(i+1) % 3]) << 16));
			if (i < start+count)
				BCI_WRITE(i + reorder[i%3]);
		} else if (S3_SAVAGE3D_SERIES(dev_priv->chipset)) {
			BEGIN_BCI((count+1+1)/2);
			BCI_DRAW_INDICES_S3D(count, prim, start);

			for (i = start+1; i+1 < start+count; i += 2)
				BCI_WRITE(i | ((i+1) << 16));
			if (i < start+count)
				BCI_WRITE(i);
		} else {
			BEGIN_BCI((count+2+1)/2);
			BCI_DRAW_INDICES_S4(count, prim, skip);

			for (i = start; i+1 < start+count; i += 2)
				BCI_WRITE(i | ((i+1) << 16));
			if (i < start+count)
				BCI_WRITE(i);
		}

		start += count;
		n -= count;

		prim |= BCI_CMD_DRAW_CONT;
	}

	return 0;
}

static int savage_dispatch_vb_prim(drm_savage_private_t *dev_priv,
				   const drm_savage_cmd_header_t *cmd_header,
				   const uint32_t __user *vtxbuf,
				   unsigned int vb_size,
				   unsigned int vb_stride)
{
	BCI_LOCALS;
	unsigned char reorder = 0;
	unsigned char prim = cmd_header->prim.prim;
	unsigned short skip = cmd_header->prim.skip;
	unsigned int n = cmd_header->prim.count;
	unsigned int start = cmd_header->prim.start;
	unsigned int vtx_size;
	unsigned int i;

	switch (prim) {
	case SAVAGE_PRIM_TRILIST_201:
		reorder = 1;
		prim = SAVAGE_PRIM_TRILIST;
	case SAVAGE_PRIM_TRILIST:
		if (n % 3 != 0) {
			DRM_ERROR("wrong number of vertices %u in TRILIST\n",
				  n);
			return DRM_ERR(EINVAL);
		}
		break;
	case SAVAGE_PRIM_TRISTRIP:
	case SAVAGE_PRIM_TRIFAN:
		if (n < 3) {
			DRM_ERROR("wrong number of vertices %u in TRIFAN/STRIP\n",
				  n);
			return DRM_ERR(EINVAL);
		}
		break;
	default:
		DRM_ERROR("invalid primitive type %u\n", prim);
		return DRM_ERR(EINVAL);
	}

	if (S3_SAVAGE3D_SERIES(dev_priv->chipset)) {
		if (skip > SAVAGE_SKIP_ALL_S3D) {
			DRM_ERROR("invalid skip flags 0x%04x\n", skip);
			return DRM_ERR(EINVAL);
		}
		vtx_size = 8; /* full vertex */
	} else {
		if (skip > SAVAGE_SKIP_ALL_S4) {
			DRM_ERROR("invalid skip flags 0x%04x\n", skip);
			return DRM_ERR(EINVAL);
		}
		vtx_size = 10; /* full vertex */
	}

	vtx_size -= (skip & 1) + (skip >> 1 & 1) +
		(skip >> 2 & 1) + (skip >> 3 & 1) + (skip >> 4 & 1) +
		(skip >> 5 & 1) + (skip >> 6 & 1) + (skip >> 7 & 1);

	if (vtx_size > vb_stride) {
		DRM_ERROR("vertex size greater than vb stride (%u > %u)\n",
			  vtx_size, vb_stride);
		return DRM_ERR(EINVAL);
	}

	if (start + n > vb_size / (vtx_size*4)) {
		DRM_ERROR("vertex indices (%u-%u) out of range (0-%u)\n",
			  start, start + n - 1, vb_size / (vtx_size*4));
		return DRM_ERR(EINVAL);
	}

	prim <<= 25;
	while (n != 0) {
		/* Can emit up to 255 vertices (85 triangles) at once. */
		unsigned int count = n > 255 ? 255 : n;
		if (reorder) {
			/* Need to reorder vertices for correct flat
			 * shading while preserving the clock sense
			 * for correct culling. Only on Savage3D. */
			int reorder[3] = {-1, -1, -1};
			reorder[start%3] = 2;

			BEGIN_BCI(vtx_size+1);
			BCI_DRAW_PRIMITIVE(count, prim, skip);

			for (i = start; i < start+count; ++i) {
				unsigned int j = i + reorder[i % 3];
				BCI_COPY_FROM_USER(&vtxbuf[vb_stride*j],
						   vtx_size);
			}
		} else {
			BEGIN_BCI(vtx_size+1);
			BCI_DRAW_PRIMITIVE(count, prim, skip);

			if (vb_stride == vtx_size) {
				BCI_COPY_FROM_USER(&vtxbuf[vtx_size*start],
						   vtx_size*count);
			} else {
				for (i = start; i < start+count; ++i) {
					BCI_COPY_FROM_USER(
						&vtxbuf[vb_stride*i],
						vtx_size);
				}
			}
		}

		start += count;
		n -= count;

		prim |= BCI_CMD_DRAW_CONT;
	}

	return 0;
}

static int savage_dispatch_clear(drm_savage_private_t *dev_priv,
				 const drm_savage_cmd_header_t *cmd_header,
				 const drm_savage_cmd_header_t __user *data,
				 unsigned int nbox,
				 const drm_clip_rect_t __user *usr_boxes)
{
	BCI_LOCALS;
	unsigned int flags = cmd_header->clear0.flags, mask, value;
	unsigned int clear_cmd;
	unsigned int i;

	if (nbox == 0)
		return 0;

	DRM_GET_USER_UNCHECKED(mask, &((drm_savage_cmd_header_t*)data)
			       ->clear1.mask);
	DRM_GET_USER_UNCHECKED(value, &((drm_savage_cmd_header_t*)data)
			       ->clear1.value);

	clear_cmd = BCI_CMD_RECT | BCI_CMD_RECT_XP | BCI_CMD_RECT_YP |
		BCI_CMD_SEND_COLOR | BCI_CMD_DEST_PBD_NEW;
	BCI_CMD_SET_ROP(clear_cmd,0xCC);

	i = ((flags & SAVAGE_FRONT) ? 1 : 0) +
		((flags & SAVAGE_BACK) ? 1 : 0) +
		((flags & SAVAGE_DEPTH) ? 1 : 0);
	if (i == 0)
		return 0;

	BEGIN_BCI(nbox * i * 6 + (mask != 0xffffffff ? 4 : 0));

	if (mask != 0xffffffff) {
		/* set mask */
		BCI_SET_REGISTERS(SAVAGE_BITPLANEWTMASK, 1);
		BCI_WRITE(mask);
	}
	for (i = 0; i < nbox; ++i) {
		drm_clip_rect_t box;
		unsigned int x, y, w, h;
		unsigned int buf;
		DRM_COPY_FROM_USER_UNCHECKED(&box, &usr_boxes[i], sizeof(box));
		x = box.x1, y = box.y1;
		w = box.x2 - box.x1;
		h = box.y2 - box.y1;
		for (buf = SAVAGE_FRONT; buf <= SAVAGE_DEPTH; buf <<= 1) {
			if (!(flags & buf))
				continue;
			BCI_WRITE(clear_cmd);
			switch(buf) {
			case SAVAGE_FRONT:
				BCI_WRITE(dev_priv->front_offset);
				BCI_WRITE(dev_priv->front_bd);
				break;
			case SAVAGE_BACK:
				BCI_WRITE(dev_priv->back_offset);
				BCI_WRITE(dev_priv->back_bd);
				break;
			case SAVAGE_DEPTH:
				BCI_WRITE(dev_priv->depth_offset);
				BCI_WRITE(dev_priv->depth_bd);
				break;
			}
			BCI_WRITE(value);
			BCI_WRITE(BCI_X_Y(x, y));
			BCI_WRITE(BCI_W_H(w, h));
		}
	}
	if (mask != 0xffffffff) {
		/* reset mask */
		BCI_SET_REGISTERS(SAVAGE_BITPLANEWTMASK, 1);
		BCI_WRITE(0xffffffff);
	}

	return 0;
}

static int savage_dispatch_swap(drm_savage_private_t *dev_priv,
				unsigned int nbox,
				const drm_clip_rect_t __user *usr_boxes)
{
	BCI_LOCALS;
	unsigned int swap_cmd;
	unsigned int i;

	if (nbox == 0)
		return 0;

	swap_cmd = BCI_CMD_RECT | BCI_CMD_RECT_XP | BCI_CMD_RECT_YP |
		BCI_CMD_SRC_PBD_COLOR_NEW | BCI_CMD_DEST_GBD;
	BCI_CMD_SET_ROP(swap_cmd,0xCC);

	BEGIN_BCI(nbox*6);
	for (i = 0; i < nbox; ++i) {
		drm_clip_rect_t box;
		DRM_COPY_FROM_USER_UNCHECKED(&box, &usr_boxes[i], sizeof(box));

		BCI_WRITE(swap_cmd);
		BCI_WRITE(dev_priv->back_offset);
		BCI_WRITE(dev_priv->back_bd);
		BCI_WRITE(BCI_X_Y(box.x1, box.y1));
		BCI_WRITE(BCI_X_Y(box.x1, box.y1));
		BCI_WRITE(BCI_W_H(box.x2-box.x1, box.y2-box.y1));
	}

	return 0;
}

int savage_bci_cmdbuf(DRM_IOCTL_ARGS)
{
	DRM_DEVICE;
	drm_savage_private_t *dev_priv = dev->dev_private;
	drm_device_dma_t *dma = dev->dma;
	drm_buf_t *dmabuf;
	drm_savage_cmdbuf_t cmdbuf;
	drm_savage_cmd_header_t __user *usr_cmdbuf;
	unsigned int __user *usr_vtxbuf;
	drm_clip_rect_t __user *usr_boxes;
	unsigned int i, j;
	int ret = 0;

	DRM_DEBUG("\n");
	
	LOCK_TEST_WITH_RETURN(dev, filp);

	DRM_COPY_FROM_USER_IOCTL(cmdbuf, (drm_savage_cmdbuf_t __user *)data,
				 sizeof(cmdbuf));

	if (cmdbuf.dma_idx > dma->buf_count) {
		DRM_ERROR("vertex buffer index %u out of range (0-%u)\n",
			  cmdbuf.dma_idx, dma->buf_count-1);
		return DRM_ERR(EINVAL);
	}
	dmabuf = dma->buflist[cmdbuf.dma_idx];

	usr_cmdbuf = (drm_savage_cmd_header_t __user *)cmdbuf.cmd_addr;
	usr_vtxbuf = (unsigned int __user *)cmdbuf.vb_addr;
	usr_boxes = (drm_clip_rect_t __user *)cmdbuf.box_addr;
	if ((cmdbuf.size && DRM_VERIFYAREA_READ(usr_cmdbuf, cmdbuf.size*8)) ||
	    (cmdbuf.vb_size && DRM_VERIFYAREA_READ(
		    usr_vtxbuf, cmdbuf.vb_size)) ||
	    (cmdbuf.nbox && DRM_VERIFYAREA_READ(
		    usr_boxes, cmdbuf.nbox*sizeof(drm_clip_rect_t))))
		return DRM_ERR(EFAULT);

	/* Make sure writes to DMA buffers are finished before sending
	 * DMA commands to the graphics hardware. */
	DRM_MEMORYBARRIER();

	i = 0;
	while (i < cmdbuf.size && ret == 0) {
		drm_savage_cmd_header_t cmd_header;
		DRM_COPY_FROM_USER_UNCHECKED(&cmd_header, usr_cmdbuf,
					     sizeof(cmd_header));
		usr_cmdbuf++;
		i++;

		switch (cmd_header.cmd.cmd) {
		case SAVAGE_CMD_STATE:
			ret = savage_dispatch_state(
				dev_priv, &cmd_header,
				(uint32_t __user *)usr_cmdbuf);
			j = (cmd_header.state.count + 1) / 2;
			usr_cmdbuf += j;
			i += j;
			break;
		case SAVAGE_CMD_DMA_PRIM:
			ret = savage_dispatch_dma_prim(
				dev_priv, &cmd_header, dmabuf);
			break;
		case SAVAGE_CMD_VB_PRIM:
			ret = savage_dispatch_vb_prim(
				dev_priv, &cmd_header,
				(uint32_t __user *)usr_vtxbuf,
				cmdbuf.vb_size, cmdbuf.vb_stride);
			break;
#if 0 /* to be implemented */
		case SAVAGE_CMD_DMA_IDX:
			ret = savage_dispatch_dma_idx(
				dev_priv, &cmd_header, usr_cmdbuf, dmabuf);
			j = (cmd_header.state.count + 3) / 4;
			usr_cmdbuf += j;
			i += j;
			break;
		case SAVAGE_CMD_VB_IDX:
			ret = savage_dispatch_vtx_idx(
				dev_priv, &cmd_header, usr_cmdbuf, usr_vtxbuf,
				cmdbuf.vb_size, cmdbuf.vb_stride);
			j = (cmd_header.state.count + 3) / 4;
			usr_cmdbuf += j;
			i += j;
			break;
#endif
		case SAVAGE_CMD_CLEAR:
			ret = savage_dispatch_clear(dev_priv, &cmd_header,
						    usr_cmdbuf,
						    cmdbuf.nbox, usr_boxes);
			usr_cmdbuf++;
			i++;
			break;
		case SAVAGE_CMD_SWAP:
			ret = savage_dispatch_swap(dev_priv,
						   cmdbuf.nbox, usr_boxes);
			break;
		default:
			DRM_ERROR("invalid command 0x%x\n", cmd_header.cmd.cmd);
			return DRM_ERR(EINVAL);
		}

		if (ret != 0)
			return ret;
	}

	if (cmdbuf.discard) {
		drm_savage_buf_priv_t *buf_priv = dmabuf->dev_private;
		uint16_t event;
		event = savage_bci_emit_event(dev_priv, SAVAGE_WAIT_3D);
		SET_AGE(&buf_priv->age, event, dev_priv->event_wrap);
		savage_freelist_put(dev, dmabuf);
	}

	return 0;
}
