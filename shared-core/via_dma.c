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
 * Copyright 2004 The Unichrome project.
 * All Rights Reserved.
 *
 **************************************************************************/

#include "drmP.h"
#include "drm.h"
#include "via_drm.h"
#include "via_drv.h"
#include "via_3d_reg.h"

#define via_flush_write_combine() DRM_MEMORYBARRIER()

#define VIA_OUT_RING_QW(w1,w2)			\
	*vb++ = (w1);				\
	*vb++ = (w2);				\
	dev_priv->dma_low += 8; 

  

static void via_cmdbuf_start(drm_via_private_t * dev_priv);
static void via_cmdbuf_pause(drm_via_private_t * dev_priv);
static void via_cmdbuf_reset(drm_via_private_t * dev_priv);
static void via_cmdbuf_rewind(drm_via_private_t * dev_priv);
static int via_wait_idle(drm_via_private_t * dev_priv);


/*
 * This function needs to be extended whenever a new command set
 * is implemented. Currently it works only for the 2D engine
 * command, which on the Unichrome allows writing to
 * at least the 2D engine and the mpeg engine, but not the
 * video engine.
 *
 * If you update this function with new commands, please also
 * consider implementing these commands in 
 * via_parse_pci_cmdbuffer below.
 *
 * Carefully review this function for security holes 
 * after an update!!!!!!!!!
 */

static int via_check_command_stream(const uint32_t * buf, unsigned int size)
{

	uint32_t offset;
	unsigned int i;

	if (size & 7) {
		DRM_ERROR("Illegal command buffer size.\n");
		return DRM_ERR(EINVAL);
	}
	size >>= 3;
	for (i = 0; i < size; ++i) {
		offset = *buf;
		buf += 2;
		if ((offset > ((0x3FF >> 2) | HALCYON_HEADER1)) &&
		    (offset < ((0xC00 >> 2) | HALCYON_HEADER1))) {
			DRM_ERROR
				("Attempt to access Burst Command / 3D Area.\n");
			return DRM_ERR(EINVAL);
		} else if (offset > ((0xDFF >> 2) | HALCYON_HEADER1)) {
			DRM_ERROR("Attempt to access DMA or VGA registers.\n");
			return DRM_ERR(EINVAL);
		}

		/*
		 * ...
		 * A volunteer should complete this to allow non-root
		 * usage of accelerated 3D OpenGL.
		 */

	}
	return 0;
}

static inline int
via_cmdbuf_wait(drm_via_private_t * dev_priv, unsigned int size)
{
	uint32_t agp_base = dev_priv->dma_offset + (uint32_t) dev_priv->agpAddr;
	uint32_t cur_addr, hw_addr, next_addr;
	volatile uint32_t *hw_addr_ptr;
	uint32_t count;
	hw_addr_ptr = dev_priv->hw_addr_ptr;
	cur_addr = dev_priv->dma_low;
	next_addr = cur_addr + size;
	count = 1000000;	/* How long is this? */
	do {
		hw_addr = *hw_addr_ptr - agp_base;
		if (count-- == 0) {
			DRM_ERROR("via_cmdbuf_wait timed out hw %x cur_addr %x next_addr %x\n",
				  hw_addr, cur_addr, next_addr);
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
static inline uint32_t *via_check_dma(drm_via_private_t * dev_priv,
				      unsigned int size)
{
	if ((dev_priv->dma_low + size + 0x400) > dev_priv->dma_high) {
		via_cmdbuf_rewind(dev_priv);
	}
	if (via_cmdbuf_wait(dev_priv, size) != 0) {
		return NULL;
	}

	return (uint32_t *) (dev_priv->dma_ptr + dev_priv->dma_low);
}

int via_dma_cleanup(drm_device_t * dev)
{
	if (dev->dev_private) {
		drm_via_private_t *dev_priv =
			(drm_via_private_t *) dev->dev_private;

		if (dev_priv->ring.virtual_start) {
			via_cmdbuf_reset(dev_priv);

			drm_core_ioremapfree(&dev_priv->ring.map, dev);
			dev_priv->ring.virtual_start = NULL;
		}
	}

	return 0;
}

static int via_initialize(drm_device_t * dev,
			  drm_via_private_t * dev_priv,
			  drm_via_dma_init_t * init)
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

	drm_core_ioremap(&dev_priv->ring.map, dev);

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

int via_dma_init(DRM_IOCTL_ARGS)
{
	DRM_DEVICE;
	drm_via_private_t *dev_priv = (drm_via_private_t *) dev->dev_private;
	drm_via_dma_init_t init;
	int retcode = 0;

	DRM_COPY_FROM_USER_IOCTL(init, (drm_via_dma_init_t *) data,
				 sizeof(init));

	switch (init.func) {
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

static int via_dispatch_cmdbuffer(drm_device_t * dev, drm_via_cmdbuffer_t * cmd)
{
	drm_via_private_t *dev_priv = dev->dev_private;
	uint32_t *vb;
	int ret;

	vb = via_check_dma(dev_priv, cmd->size);
	if (vb == NULL) {
		return DRM_ERR(EAGAIN);
	}
	if (DRM_COPY_FROM_USER(vb, cmd->buf, cmd->size)) {
		return DRM_ERR(EFAULT);
	}

	if ((ret = via_check_command_stream(vb, cmd->size)))
		return ret;

	dev_priv->dma_low += cmd->size;
	via_cmdbuf_pause(dev_priv);

	return 0;
}

static int via_quiescent(drm_device_t * dev)
{
	drm_via_private_t *dev_priv = dev->dev_private;

	if (!via_wait_idle(dev_priv)) {
		return DRM_ERR(EAGAIN);
	}
	return 0;
}

int via_flush_ioctl(DRM_IOCTL_ARGS)
{
	DRM_DEVICE;

	if (!_DRM_LOCK_IS_HELD(dev->lock.hw_lock->lock)) {
		DRM_ERROR("via_flush_ioctl called without lock held\n");
		return DRM_ERR(EINVAL);
	}

	return via_quiescent(dev);
}

int via_cmdbuffer(DRM_IOCTL_ARGS)
{
	DRM_DEVICE;
	drm_via_cmdbuffer_t cmdbuf;
	int ret;

	DRM_COPY_FROM_USER_IOCTL(cmdbuf, (drm_via_cmdbuffer_t *) data,
				 sizeof(cmdbuf));

	DRM_DEBUG("via cmdbuffer, buf %p size %lu\n", cmdbuf.buf, cmdbuf.size);

	if (!_DRM_LOCK_IS_HELD(dev->lock.hw_lock->lock)) {
		DRM_ERROR("via_cmdbuffer called without lock held\n");
		return DRM_ERR(EINVAL);
	}

	ret = via_dispatch_cmdbuffer(dev, &cmdbuf);
	if (ret) {
		return ret;
	}

	return 0;
}

static int via_parse_pci_cmdbuffer(drm_device_t * dev, const char *buf,
				   unsigned int size)
{
	drm_via_private_t *dev_priv = dev->dev_private;
	uint32_t offset, value;
	const uint32_t *regbuf = (uint32_t *) buf;
	unsigned int i;
	int ret;

	if ((ret = via_check_command_stream(regbuf, size)))
		return ret;

	size >>= 3;
	for (i = 0; i < size; ++i) {
		offset = (*regbuf++ & ~HALCYON_HEADER1) << 2;
		value = *regbuf++;
		VIA_WRITE(offset, value);
	}
	return 0;
}

static int via_dispatch_pci_cmdbuffer(drm_device_t * dev,
				      drm_via_cmdbuffer_t * cmd)
{
	drm_via_private_t *dev_priv = dev->dev_private;
	char *hugebuf;
	int ret;

	/*
	 * We must be able to parse the buffer all at a time, so as
	 * to return an error on an invalid operation without doing
	 * anything.
	 * Small buffers must, on the other hand be handled fast.
	 */

	if (cmd->size > VIA_MAX_PCI_SIZE) {
		return DRM_ERR(ENOMEM);
	} else if (cmd->size > VIA_PREALLOCATED_PCI_SIZE) {
		if (NULL == (hugebuf = (char *)kmalloc(cmd->size, GFP_KERNEL)))
			return DRM_ERR(ENOMEM);
		if (DRM_COPY_FROM_USER(hugebuf, cmd->buf, cmd->size))
			return DRM_ERR(EFAULT);
		ret = via_parse_pci_cmdbuffer(dev, hugebuf, cmd->size);
		kfree(hugebuf);
	} else {
		if (DRM_COPY_FROM_USER(dev_priv->pci_buf, cmd->buf, cmd->size))
			return DRM_ERR(EFAULT);
		ret =
			via_parse_pci_cmdbuffer(dev, dev_priv->pci_buf, cmd->size);
	}
	return ret;
}

int via_pci_cmdbuffer(DRM_IOCTL_ARGS)
{
	DRM_DEVICE;
	drm_via_cmdbuffer_t cmdbuf;
	int ret;

	DRM_COPY_FROM_USER_IOCTL(cmdbuf, (drm_via_cmdbuffer_t *) data,
				 sizeof(cmdbuf));

	DRM_DEBUG("via_pci_cmdbuffer, buf %p size %lu\n", cmdbuf.buf,
		  cmdbuf.size);

	if (!_DRM_LOCK_IS_HELD(dev->lock.hw_lock->lock)) {
		DRM_ERROR("via_pci_cmdbuffer called without lock held\n");
		return DRM_ERR(EINVAL);
	}

	ret = via_dispatch_pci_cmdbuffer(dev, &cmdbuf);
	if (ret) {
		return ret;
	}

	return 0;
}

/************************************************************************/

#define CMDBUF_ALIGNMENT_SIZE   (0x100)
#define CMDBUF_ALIGNMENT_MASK   (0xff)

/* defines for VIA 3D registers */
#define VIA_REG_STATUS          0x400
#define VIA_REG_TRANSET         0x43C
#define VIA_REG_TRANSPACE       0x440

/* VIA_REG_STATUS(0x400): Engine Status */
#define VIA_CMD_RGTR_BUSY       0x00000080	/* Command Regulator is busy */
#define VIA_2D_ENG_BUSY         0x00000001	/* 2D Engine is busy */
#define VIA_3D_ENG_BUSY         0x00000002	/* 3D Engine is busy */
#define VIA_VR_QUEUE_BUSY       0x00020000	/* Virtual Queue is busy */

#define SetReg2DAGP(nReg, nData) { \
	*((uint32_t *)(vb)) = ((nReg) >> 2) | HALCYON_HEADER1; \
	*((uint32_t *)(vb) + 1) = (nData); \
	vb = ((uint32_t *)vb) + 2; \
	dev_priv->dma_low +=8; \
}

static inline uint32_t *via_align_buffer(drm_via_private_t * dev_priv,
					 uint32_t * vb, int qw_count)
{
	for (; qw_count > 0; --qw_count) {
		VIA_OUT_RING_QW(HC_DUMMY, HC_DUMMY);
	}
	return vb;
}


/*
 * This function is used internally by ring buffer mangement code.
 *
 * Returns virtual pointer to ring buffer.
 */
static inline uint32_t *via_get_dma(drm_via_private_t * dev_priv)
{
	return (uint32_t *) (dev_priv->dma_ptr + dev_priv->dma_low);
}

/*
 * Hooks a segment of data into the tail of the ring-buffer by
 * modifying the pause address stored in the buffer itself. If
 * the regulator has already paused, restart it.
 */
static int via_hook_segment(drm_via_private_t *dev_priv,
			    uint32_t pause_addr_hi, uint32_t pause_addr_lo,
			    int no_pci_fire)
{
	int paused, count;

	via_flush_write_combine();
	while(! *(via_get_dma(dev_priv)-1));
	*dev_priv->last_pause_ptr = pause_addr_lo;
	via_flush_write_combine();

	/*
	 * The below statement is inserted to really force the flush.
	 * Not sure it is needed.
	 */

	while(! *dev_priv->last_pause_ptr);
	dev_priv->last_pause_ptr = via_get_dma(dev_priv) - 1;

	paused = 0;
	count = 3; 

	while (!(paused = (VIA_READ(0x41c) & 0x80000000)) && count--);
	if ((count < 1) && (count >= 0)) {
		uint32_t rgtr, ptr;
		rgtr = *(dev_priv->hw_addr_ptr);
		ptr = ((char *)dev_priv->last_pause_ptr - dev_priv->dma_ptr) + 
			dev_priv->dma_offset + (uint32_t) dev_priv->agpAddr + 4 - 0x100;
		if (rgtr <= ptr) {
			DRM_ERROR("Command regulator\npaused at count %d, address %x, "
				  "while current pause address is %x.\n"
				  "Please mail this message to "
				  "<unichrome-devel@lists.sourceforge.net>\n",
				  count, rgtr, ptr);
		}
	}
		
	if (paused && !no_pci_fire) {
		VIA_WRITE(VIA_REG_TRANSET, (HC_ParaType_PreCR << 16));
		VIA_WRITE(VIA_REG_TRANSPACE, pause_addr_hi);
		VIA_WRITE(VIA_REG_TRANSPACE, pause_addr_lo);
	}
	return paused;
}



static int via_wait_idle(drm_via_private_t * dev_priv)
{
	int count = 10000000;
	while (count-- && (VIA_READ(VIA_REG_STATUS) &
			   (VIA_CMD_RGTR_BUSY | VIA_2D_ENG_BUSY |
			    VIA_3D_ENG_BUSY))) ;
	return count;
}

static uint32_t *via_align_cmd(drm_via_private_t * dev_priv, uint32_t cmd_type,
			       uint32_t addr, uint32_t *cmd_addr_hi, 
			       uint32_t *cmd_addr_lo)
{
	uint32_t agp_base;
	uint32_t cmd_addr, addr_lo, addr_hi;
	uint32_t *vb;
	uint32_t qw_pad_count;

	via_cmdbuf_wait(dev_priv, 2*CMDBUF_ALIGNMENT_SIZE);

	vb = via_get_dma(dev_priv);
	VIA_OUT_RING_QW( HC_HEADER2 | ((VIA_REG_TRANSET >> 2) << 12) |
			 (VIA_REG_TRANSPACE >> 2), HC_ParaType_PreCR << 16); 
	agp_base = dev_priv->dma_offset + (uint32_t) dev_priv->agpAddr;
	qw_pad_count = (CMDBUF_ALIGNMENT_SIZE >> 3) -
		((dev_priv->dma_low & CMDBUF_ALIGNMENT_MASK) >> 3);

	
	cmd_addr = (addr) ? addr : 
		agp_base + dev_priv->dma_low - 8 + (qw_pad_count << 3);
	addr_lo = ((HC_SubA_HAGPBpL << 24) | cmd_type |
		   (cmd_addr & 0xffffff));
	addr_hi = ((HC_SubA_HAGPBpH << 24) | (cmd_addr >> 24));

	vb = via_align_buffer(dev_priv, vb, qw_pad_count - 1);
	VIA_OUT_RING_QW(*cmd_addr_hi = addr_hi, 
			*cmd_addr_lo = addr_lo);
	return vb;
}




static void via_cmdbuf_start(drm_via_private_t * dev_priv)
{
	uint32_t pause_addr_lo, pause_addr_hi;
	uint32_t start_addr, start_addr_lo;
	uint32_t end_addr, end_addr_lo;
	uint32_t command;
	uint32_t agp_base;


	dev_priv->dma_low = 0;

	agp_base = dev_priv->dma_offset + (uint32_t) dev_priv->agpAddr;
	start_addr = agp_base;
	end_addr = agp_base + dev_priv->dma_high;

	start_addr_lo = ((HC_SubA_HAGPBstL << 24) | (start_addr & 0xFFFFFF));
	end_addr_lo = ((HC_SubA_HAGPBendL << 24) | (end_addr & 0xFFFFFF));
	command = ((HC_SubA_HAGPCMNT << 24) | (start_addr >> 24) |
		   ((end_addr & 0xff000000) >> 16));

	dev_priv->last_pause_ptr = 
		via_align_cmd(dev_priv, HC_HAGPBpID_PAUSE, 0, 
			      &pause_addr_hi, & pause_addr_lo) - 1;

	via_flush_write_combine();
	while(! *dev_priv->last_pause_ptr);

	VIA_WRITE(VIA_REG_TRANSET, (HC_ParaType_PreCR << 16));
	VIA_WRITE(VIA_REG_TRANSPACE, command);
	VIA_WRITE(VIA_REG_TRANSPACE, start_addr_lo);
	VIA_WRITE(VIA_REG_TRANSPACE, end_addr_lo);

	VIA_WRITE(VIA_REG_TRANSPACE, pause_addr_hi);
	VIA_WRITE(VIA_REG_TRANSPACE, pause_addr_lo);

	VIA_WRITE(VIA_REG_TRANSPACE, command | HC_HAGPCMNT_MASK);
}
static inline void via_dummy_bitblt(drm_via_private_t * dev_priv)
{
	uint32_t *vb = via_get_dma(dev_priv);
	SetReg2DAGP(0x0C, (0 | (0 << 16)));
	SetReg2DAGP(0x10, 0 | (0 << 16));
	SetReg2DAGP(0x0, 0x1 | 0x2000 | 0xAA000000);
}

static void via_cmdbuf_jump(drm_via_private_t * dev_priv)
{
	uint32_t agp_base;
	uint32_t pause_addr_lo, pause_addr_hi;
	uint32_t jump_addr_lo, jump_addr_hi;
	volatile uint32_t *last_pause_ptr;

	agp_base = dev_priv->dma_offset + (uint32_t) dev_priv->agpAddr;
	via_align_cmd(dev_priv,  HC_HAGPBpID_JUMP, 0, &jump_addr_hi, 
		      &jump_addr_lo);
	
	dev_priv->dma_low = 0;
	if (via_cmdbuf_wait(dev_priv, CMDBUF_ALIGNMENT_SIZE) != 0) {
		DRM_ERROR("via_cmdbuf_jump failed\n");
	}

	/*
	 * The command regulator needs to stall for a while since it probably
	 * had a concussion during the jump... It will stall at the second
	 * bitblt since the 2D engine is busy with the first.
	 */

	via_dummy_bitblt(dev_priv);
	via_dummy_bitblt(dev_priv); 
	last_pause_ptr = via_align_cmd(dev_priv,  HC_HAGPBpID_PAUSE, 0, &pause_addr_hi, 
				       &pause_addr_lo) -1;

	/*
	 * The regulator may still be suffering from the shock of the jump.
	 * Add another pause command to make sure it really will get itself together
	 * and pause.
	 */

	via_align_cmd(dev_priv,  HC_HAGPBpID_PAUSE, 0, &pause_addr_hi, 
		      &pause_addr_lo);
	*last_pause_ptr = pause_addr_lo; 
	via_hook_segment( dev_priv, jump_addr_hi, jump_addr_lo, 0);
}



static void via_cmdbuf_rewind(drm_via_private_t * dev_priv)
{
	via_cmdbuf_jump(dev_priv); 
}

static void via_cmdbuf_flush(drm_via_private_t * dev_priv, uint32_t cmd_type)
{
	uint32_t pause_addr_lo, pause_addr_hi;

	via_align_cmd(dev_priv, cmd_type, 0, &pause_addr_hi, &pause_addr_lo);
	via_hook_segment( dev_priv, pause_addr_hi, pause_addr_lo, 0);
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
