/* via_dma.c -- DMA support for the VIA Unichrome/Pro
 */
/**************************************************************************
 * 
 * Copyright 2003 Tungsten Graphics, Inc., Cedar Park, Texas.
 * All Rights Reserved.
 *
 * Copyright 2004 Digeo, Inc., Palo Alto, CA, U.S.A.
 * All Rights Reserved.
 * 
 **************************************************************************/

#include "via.h"
#include "drmP.h"
#include "drm.h"
#include "via_drm.h"
#include "via_drv.h"

static void via_cmdbuf_start(drm_via_private_t * dev_priv);
static void via_cmdbuf_pause(drm_via_private_t * dev_priv);
static void via_cmdbuf_reset(drm_via_private_t * dev_priv);
static void via_cmdbuf_rewind(drm_via_private_t * dev_priv);
static int  via_wait_idle(drm_via_private_t * dev_priv);

static inline int
via_cmdbuf_wait(drm_via_private_t * dev_priv, unsigned int size)
{
	uint32_t agp_base = dev_priv->dma_offset + (uint32_t) dev_priv->agpAddr;
	uint32_t cur_addr, hw_addr, next_addr;
	volatile uint32_t * hw_addr_ptr;
	uint32_t count;
	hw_addr_ptr = dev_priv->hw_addr_ptr;
	cur_addr = agp_base + dev_priv->dma_low;
	/* At high resolution (i.e. 1280x1024) and with high workload within
	 * a short commmand stream, the following test will fail. It may be
	 * that the engine is too busy to update hw_addr. Therefore, add
	 * a large 64KB window between buffer head and tail.
	 */
	next_addr = cur_addr + size + 64 * 1024;
	count = 1000000; /* How long is this? */
	do {
		hw_addr = *hw_addr_ptr;
		if (count-- == 0) {
			DRM_ERROR("via_cmdbuf_wait timed out hw %x dma_low %x\n",
					hw_addr, dev_priv->dma_low);
			return -1;
		}
	} while ((cur_addr < hw_addr) && (next_addr >= hw_addr));
	return 0;
}

/*
 * Checks whether buffer head has reach the end. Rewind the ring buffer
 * when necessary.
 *
 * Returns virtual pointer to ring buffer.
 */
static inline uint32_t *
via_check_dma(drm_via_private_t * dev_priv, unsigned int size)
{
	if ((dev_priv->dma_low + size + 0x400) > dev_priv->dma_high) {
		via_cmdbuf_rewind(dev_priv);
	}
	if (via_cmdbuf_wait(dev_priv, size) != 0) {
		return NULL;
	}

	return (uint32_t*)(dev_priv->dma_ptr + dev_priv->dma_low);
}

int via_dma_cleanup(drm_device_t *dev)
{
	if (dev->dev_private) {
		drm_via_private_t *dev_priv = 
				(drm_via_private_t *) dev->dev_private;

		if (dev_priv->ring.virtual_start) {
			via_cmdbuf_reset(dev_priv);

			drm_core_ioremapfree( &dev_priv->ring.map, dev);
			dev_priv->ring.virtual_start = NULL;
		}
	}

	return 0;
}


static int via_initialize(drm_device_t *dev, 
			       drm_via_private_t *dev_priv,
			       drm_via_dma_init_t *init)
{
	if (!dev_priv || !dev_priv->mmio) {
		DRM_ERROR("via_dma_init called before via_map_init\n");
		return DRM_ERR(EFAULT);
	}

	if (dev_priv->ring.virtual_start != NULL) {
		DRM_ERROR("%s called again without calling cleanup\n",
				__FUNCTION__);
		return DRM_ERR(EFAULT);
	}

	dev_priv->ring.map.offset = dev->agp->base + init->offset;
	dev_priv->ring.map.size = init->size;
	dev_priv->ring.map.type = 0;
	dev_priv->ring.map.flags = 0;
	dev_priv->ring.map.mtrr = 0;

	drm_core_ioremap( &dev_priv->ring.map, dev );

	if (dev_priv->ring.map.handle == NULL) {
		via_dma_cleanup(dev);
		DRM_ERROR("can not ioremap virtual address for"
				" ring buffer\n");
		return DRM_ERR(ENOMEM);
	}

	dev_priv->ring.virtual_start = dev_priv->ring.map.handle;

	dev_priv->dma_ptr = dev_priv->ring.virtual_start;
	dev_priv->dma_low = 0;
	dev_priv->dma_high = init->size;
	dev_priv->dma_offset = init->offset;
	dev_priv->last_pause_ptr = NULL;
	dev_priv->hw_addr_ptr = dev_priv->mmio->handle + init->reg_pause_addr;

	via_cmdbuf_start(dev_priv);

	return 0;
}


int via_dma_init( DRM_IOCTL_ARGS )
{
	DRM_DEVICE;
	drm_via_private_t *dev_priv = (drm_via_private_t *)dev->dev_private;
	drm_via_dma_init_t init;
	int retcode = 0;

	DRM_COPY_FROM_USER_IOCTL(init, (drm_via_dma_init_t *)data, sizeof(init));

	switch(init.func) {
	case VIA_INIT_DMA:
		retcode = via_initialize(dev, dev_priv, &init);
		break;
	case VIA_CLEANUP_DMA:
		retcode = via_dma_cleanup(dev);
		break;
	default:
		retcode = DRM_ERR(EINVAL);
		break;
	}

	return retcode;
}


static int via_dispatch_cmdbuffer(drm_device_t *dev, 
				   drm_via_cmdbuffer_t *cmd )
{
	drm_via_private_t *dev_priv = dev->dev_private;
	uint32_t * vb;
	vb = via_check_dma(dev_priv, cmd->size);
	if (vb == NULL) {
		return DRM_ERR(EAGAIN);
	}
	DRM_COPY_FROM_USER(vb, cmd->buf, cmd->size);
	dev_priv->dma_low += cmd->size;
	via_cmdbuf_pause(dev_priv);

	return 0;
}


static int via_quiescent(drm_device_t *dev)
{
	drm_via_private_t *dev_priv = dev->dev_private;

	if (!via_wait_idle(dev_priv)) {
		return DRM_ERR(EAGAIN);
	}
	return 0;
}


int via_flush_ioctl( DRM_IOCTL_ARGS )
{
	DRM_DEVICE;

	if(!_DRM_LOCK_IS_HELD(dev->lock.hw_lock->lock)) {
		DRM_ERROR("via_flush_ioctl called without lock held\n");
		return DRM_ERR(EINVAL);
	}

	return via_quiescent(dev); 
}


int via_cmdbuffer( DRM_IOCTL_ARGS )
{
	DRM_DEVICE;
	drm_via_cmdbuffer_t cmdbuf;
	int ret;

	DRM_COPY_FROM_USER_IOCTL( cmdbuf, (drm_via_cmdbuffer_t *)data, 
			sizeof(cmdbuf) );

	DRM_DEBUG("via cmdbuffer, buf %p size %lu\n", cmdbuf.buf, cmdbuf.size);

	if(!_DRM_LOCK_IS_HELD(dev->lock.hw_lock->lock)) {
		DRM_ERROR("via_cmdbuffer called without lock held\n");
		return DRM_ERR(EINVAL);
	}

	ret = via_dispatch_cmdbuffer( dev, &cmdbuf );
	if (ret) {
		return ret;
	}

	return 0;
}




/************************************************************************/
#include "via_3d_reg.h"

#define CMDBUF_ALIGNMENT_SIZE   (0x100)
#define CMDBUF_ALIGNMENT_MASK   (0xff)

/* defines for VIA 3D registers */
#define VIA_REG_STATUS          0x400
#define VIA_REG_TRANSET         0x43C
#define VIA_REG_TRANSPACE       0x440
                                                                                                                                                                                  
/* VIA_REG_STATUS(0x400): Engine Status */
#define VIA_CMD_RGTR_BUSY       0x00000080  /* Command Regulator is busy */
#define VIA_2D_ENG_BUSY         0x00000001  /* 2D Engine is busy */
#define VIA_3D_ENG_BUSY         0x00000002  /* 3D Engine is busy */
#define VIA_VR_QUEUE_BUSY       0x00020000 /* Virtual Queue is busy */


#define SetReg2DAGP(nReg, nData) { \
	*((uint32_t *)(vb)) = ((nReg) >> 2) | 0xF0000000; \
	*((uint32_t *)(vb) + 1) = (nData); \
	vb = ((uint32_t *)vb) + 2; \
	dev_priv->dma_low +=8; \
}

static uint32_t via_swap_count = 0;

static inline uint32_t *
via_align_buffer(drm_via_private_t * dev_priv, uint32_t * vb, int qw_count)
{
	for ( ; qw_count > 0; --qw_count) {
		*vb++ = (0xcc000000 | (dev_priv->dma_low & 0xffffff));
		*vb++ = (0xdd400000 | via_swap_count);
		dev_priv->dma_low += 8;
	}
	via_swap_count = (via_swap_count + 1) & 0xffff;
	return vb;
}

/*
 * This function is used internally by ring buffer mangement code.
 *
 * Returns virtual pointer to ring buffer.
 */
static inline uint32_t * via_get_dma(drm_via_private_t * dev_priv)
{
	return (uint32_t*)(dev_priv->dma_ptr + dev_priv->dma_low);
}


static int via_wait_idle(drm_via_private_t * dev_priv)
{
	int count = 10000000;
	while (count-- && (VIA_READ(VIA_REG_STATUS) &
		(VIA_CMD_RGTR_BUSY | VIA_2D_ENG_BUSY | VIA_3D_ENG_BUSY)));
	return count;
}

static inline void
via_dummy_bitblt(drm_via_private_t * dev_priv)
{
	uint32_t * vb = via_get_dma(dev_priv);
	/* GEDST*/
	SetReg2DAGP(0x0C, (0 | (0 << 16)));
	/* GEWD*/
	SetReg2DAGP(0x10, 0 | (0 << 16));
	/* BITBLT*/
	SetReg2DAGP(0x0, 0x1 | 0x2000 | 0xAA000000);
}

static void via_cmdbuf_start(drm_via_private_t * dev_priv)
{
	uint32_t agp_base;
	uint32_t pause_addr, pause_addr_lo, pause_addr_hi;
	uint32_t start_addr, start_addr_lo;
	uint32_t end_addr, end_addr_lo;
	uint32_t qw_pad_count;
	uint32_t command;
	uint32_t * vb;

	dev_priv->dma_low = 0;
	vb = via_get_dma(dev_priv);

	agp_base = dev_priv->dma_offset + (uint32_t) dev_priv->agpAddr;
	start_addr = agp_base;
	end_addr = agp_base + dev_priv->dma_high;

	start_addr_lo = ((HC_SubA_HAGPBstL << 24) | (start_addr & 0xFFFFFF));
	end_addr_lo = ((HC_SubA_HAGPBendL << 24) | (end_addr & 0xFFFFFF));
	command = ((HC_SubA_HAGPCMNT << 24) | (start_addr >> 24) |
			((end_addr & 0xff000000) >> 16));

	*vb++ = HC_HEADER2 | ((VIA_REG_TRANSET>>2)<<12) |
			(VIA_REG_TRANSPACE>>2);
	*vb++ = (HC_ParaType_PreCR<<16);
	dev_priv->dma_low += 8;

	qw_pad_count = (CMDBUF_ALIGNMENT_SIZE>>3) -
			((dev_priv->dma_low & CMDBUF_ALIGNMENT_MASK) >> 3);

	pause_addr = agp_base + dev_priv->dma_low - 8 + (qw_pad_count<<3);
	pause_addr_lo = ((HC_SubA_HAGPBpL<<24) |
			HC_HAGPBpID_PAUSE |
			(pause_addr & 0xffffff));
	pause_addr_hi = ((HC_SubA_HAGPBpH<<24) | (pause_addr >> 24));

	vb = via_align_buffer(dev_priv, vb, qw_pad_count-1);

	*vb++ = pause_addr_hi;
	*vb++ = pause_addr_lo;
	dev_priv->dma_low += 8;
	dev_priv->last_pause_ptr = vb-1;

	VIA_WRITE(VIA_REG_TRANSET, (HC_ParaType_PreCR << 16));
	VIA_WRITE(VIA_REG_TRANSPACE, command);
	VIA_WRITE(VIA_REG_TRANSPACE, start_addr_lo);
	VIA_WRITE(VIA_REG_TRANSPACE, end_addr_lo);

	VIA_WRITE(VIA_REG_TRANSPACE, pause_addr_hi);
	VIA_WRITE(VIA_REG_TRANSPACE, pause_addr_lo);

	VIA_WRITE(VIA_REG_TRANSPACE, command | HC_HAGPCMNT_MASK);
}

static void via_cmdbuf_jump(drm_via_private_t * dev_priv)
{
	uint32_t agp_base;
	uint32_t pause_addr, pause_addr_lo, pause_addr_hi;
	uint32_t start_addr;
	uint32_t end_addr, end_addr_lo;
	uint32_t * vb;
	uint32_t qw_pad_count;
	uint32_t command;
	uint32_t jump_addr, jump_addr_lo, jump_addr_hi;

	/* Seems like Unichrome has bug that when the PAUSE register is
	 * set in the AGP command stream immediately after a PCI write to
	 * the same register, the command regulator goes into a looping
	 * state. Prepending a BitBLT command to stall the command
	 * regulator for a moment seems to solve the problem.
	 */
	via_cmdbuf_wait(dev_priv, 48);
	via_dummy_bitblt(dev_priv);

	via_cmdbuf_wait(dev_priv, 2*CMDBUF_ALIGNMENT_SIZE);

	/* At end of buffer, rewind with a JUMP command. */
	vb = via_get_dma(dev_priv);

	*vb++ = HC_HEADER2 | ((VIA_REG_TRANSET>>2)<<12) |
			(VIA_REG_TRANSPACE>>2);
	*vb++ = (HC_ParaType_PreCR<<16);
	dev_priv->dma_low += 8;

	qw_pad_count = (CMDBUF_ALIGNMENT_SIZE>>3) -
			((dev_priv->dma_low & CMDBUF_ALIGNMENT_MASK) >> 3);

	agp_base = dev_priv->dma_offset + (uint32_t) dev_priv->agpAddr;
	start_addr = agp_base;
	end_addr = agp_base + dev_priv->dma_low - 8 + (qw_pad_count<<3);

	jump_addr = end_addr;
	jump_addr_lo = ((HC_SubA_HAGPBpL<<24) | HC_HAGPBpID_JUMP |
			(jump_addr & 0xffffff));
	jump_addr_hi = ((HC_SubA_HAGPBpH<<24) | (jump_addr >> 24));

	end_addr_lo = ((HC_SubA_HAGPBendL << 24) | (end_addr & 0xFFFFFF));
	command = ((HC_SubA_HAGPCMNT << 24) | (start_addr >> 24) |
			((end_addr & 0xff000000) >> 16));

	*vb++ = command;
	*vb++ = end_addr_lo;
	dev_priv->dma_low += 8;

	vb = via_align_buffer(dev_priv, vb, qw_pad_count-1);


	/* Now at beginning of buffer, make sure engine will pause here. */
	dev_priv->dma_low = 0;
	if (via_cmdbuf_wait(dev_priv, CMDBUF_ALIGNMENT_SIZE) != 0) {
		DRM_ERROR("via_cmdbuf_jump failed\n");
	}
	vb = via_get_dma(dev_priv);

	end_addr = agp_base + dev_priv->dma_high;

	end_addr_lo = ((HC_SubA_HAGPBendL << 24) | (end_addr & 0xFFFFFF));
	command = ((HC_SubA_HAGPCMNT << 24) | (start_addr >> 24) |
			((end_addr & 0xff000000) >> 16));

	qw_pad_count = (CMDBUF_ALIGNMENT_SIZE>>3) -
			((dev_priv->dma_low & CMDBUF_ALIGNMENT_MASK) >> 3);

	pause_addr = agp_base + dev_priv->dma_low - 8 + (qw_pad_count<<3);
	pause_addr_lo = ((HC_SubA_HAGPBpL<<24) | HC_HAGPBpID_PAUSE |
			(pause_addr & 0xffffff));
	pause_addr_hi = ((HC_SubA_HAGPBpH<<24) | (pause_addr >> 24));

	*vb++ = HC_HEADER2 | ((VIA_REG_TRANSET>>2)<<12) |
			(VIA_REG_TRANSPACE>>2);
	*vb++ = (HC_ParaType_PreCR<<16);
	dev_priv->dma_low += 8;

	*vb++ = pause_addr_hi;
	*vb++ = pause_addr_lo;
	dev_priv->dma_low += 8;

	*vb++ = command;
	*vb++ = end_addr_lo;
	dev_priv->dma_low += 8;

	vb = via_align_buffer(dev_priv, vb, qw_pad_count - 4);

	*vb++ = pause_addr_hi;
	*vb++ = pause_addr_lo;
	dev_priv->dma_low += 8;

	*dev_priv->last_pause_ptr = jump_addr_lo;
	dev_priv->last_pause_ptr = vb-1;

	if (VIA_READ(0x41c) & 0x80000000) {
		VIA_WRITE(VIA_REG_TRANSET, (HC_ParaType_PreCR << 16));
		VIA_WRITE(VIA_REG_TRANSPACE, jump_addr_hi);
		VIA_WRITE(VIA_REG_TRANSPACE, jump_addr_lo);
	}
}

static void via_cmdbuf_rewind(drm_via_private_t * dev_priv)
{
	via_cmdbuf_pause(dev_priv);
	via_cmdbuf_jump(dev_priv);
}

static void via_cmdbuf_flush(drm_via_private_t * dev_priv, uint32_t cmd_type)
{
	uint32_t agp_base;
	uint32_t pause_addr, pause_addr_lo, pause_addr_hi;
	uint32_t * vb;
	uint32_t qw_pad_count;

	via_cmdbuf_wait(dev_priv, 0x200);

	vb = via_get_dma(dev_priv);
	*vb++ = HC_HEADER2 | ((VIA_REG_TRANSET>>2)<<12) |
			(VIA_REG_TRANSPACE>>2);
	*vb++ = (HC_ParaType_PreCR<<16);
	dev_priv->dma_low += 8;

	agp_base = dev_priv->dma_offset + (uint32_t) dev_priv->agpAddr;
	qw_pad_count = (CMDBUF_ALIGNMENT_SIZE>>3) -
			((dev_priv->dma_low & CMDBUF_ALIGNMENT_MASK) >> 3);

	pause_addr = agp_base + dev_priv->dma_low - 8 + (qw_pad_count<<3);
	pause_addr_lo = ((HC_SubA_HAGPBpL<<24) | cmd_type |
			(pause_addr & 0xffffff));
	pause_addr_hi = ((HC_SubA_HAGPBpH<<24) | (pause_addr >> 24));

	vb = via_align_buffer(dev_priv, vb, qw_pad_count-1);

	*vb++ = pause_addr_hi;
	*vb++ = pause_addr_lo;
	dev_priv->dma_low += 8;
	*dev_priv->last_pause_ptr = pause_addr_lo;
	dev_priv->last_pause_ptr = vb-1;

	if (VIA_READ(0x41c) & 0x80000000) {
		VIA_WRITE(VIA_REG_TRANSET, (HC_ParaType_PreCR << 16));
		VIA_WRITE(VIA_REG_TRANSPACE, pause_addr_hi);
		VIA_WRITE(VIA_REG_TRANSPACE, pause_addr_lo);
	}
}

static void via_cmdbuf_pause(drm_via_private_t * dev_priv)
{
	via_cmdbuf_flush(dev_priv, HC_HAGPBpID_PAUSE);
}

static void via_cmdbuf_reset(drm_via_private_t * dev_priv)
{
	via_cmdbuf_flush(dev_priv, HC_HAGPBpID_STOP);
	via_wait_idle(dev_priv);
}

/************************************************************************/
