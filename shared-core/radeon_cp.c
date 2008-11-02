/* radeon_cp.c -- CP support for Radeon -*- linux-c -*- */
/*
 * Copyright 2000 Precision Insight, Inc., Cedar Park, Texas.
 * Copyright 2000 VA Linux Systems, Inc., Fremont, California.
 * Copyright 2007 Advanced Micro Devices, Inc.
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
 *    Kevin E. Martin <martin@valinux.com>
 *    Gareth Hughes <gareth@valinux.com>
 */

#include "drmP.h"
#include "drm.h"
#include "drm_sarea.h"
#include "radeon_drm.h"
#include "radeon_drv.h"
#include "r300_reg.h"

#include "radeon_microcode.h"
#define RADEON_FIFO_DEBUG	0

static int radeon_do_cleanup_cp(struct drm_device * dev);
static void radeon_do_cp_start(drm_radeon_private_t * dev_priv);

static u32 R500_READ_MCIND(drm_radeon_private_t *dev_priv, int addr)
{
	u32 ret;
	RADEON_WRITE(R520_MC_IND_INDEX, 0x7f0000 | (addr & 0xff));
	ret = RADEON_READ(R520_MC_IND_DATA);
	RADEON_WRITE(R520_MC_IND_INDEX, 0);
	return ret;
}

static u32 RS480_READ_MCIND(drm_radeon_private_t *dev_priv, int addr)
{
	u32 ret;
	RADEON_WRITE(RS480_NB_MC_INDEX, addr & 0xff);
	ret = RADEON_READ(RS480_NB_MC_DATA);
	RADEON_WRITE(RS480_NB_MC_INDEX, 0xff);
	return ret;
}

static u32 RS690_READ_MCIND(drm_radeon_private_t *dev_priv, int addr)
{
	u32 ret;
	RADEON_WRITE(RS690_MC_INDEX, (addr & RS690_MC_INDEX_MASK));
	ret = RADEON_READ(RS690_MC_DATA);
	RADEON_WRITE(RS690_MC_INDEX, RS690_MC_INDEX_MASK);
	return ret;
}

static u32 IGP_READ_MCIND(drm_radeon_private_t *dev_priv, int addr)
{
        if ((dev_priv->flags & RADEON_FAMILY_MASK) == CHIP_RS690)
	    return RS690_READ_MCIND(dev_priv, addr);
	else
	    return RS480_READ_MCIND(dev_priv, addr);
}

u32 radeon_read_mc_reg(drm_radeon_private_t *dev_priv, int addr)
{
        if ((dev_priv->flags & RADEON_FAMILY_MASK) == CHIP_RS690)
		return IGP_READ_MCIND(dev_priv, addr);
	if ((dev_priv->flags & RADEON_FAMILY_MASK) >= CHIP_RV515)
		return R500_READ_MCIND(dev_priv, addr);
	return 0;
}

void radeon_write_mc_reg(drm_radeon_private_t *dev_priv, u32 addr, u32 val)
{
        if ((dev_priv->flags & RADEON_FAMILY_MASK) == CHIP_RS690)
		IGP_WRITE_MCIND(addr, val);
	else if ((dev_priv->flags & RADEON_FAMILY_MASK) >= CHIP_RV515)
		R500_WRITE_MCIND(addr, val);
}

u32 radeon_read_fb_location(drm_radeon_private_t *dev_priv)
{

	if ((dev_priv->flags & RADEON_FAMILY_MASK) == CHIP_RV515)
		return R500_READ_MCIND(dev_priv, RV515_MC_FB_LOCATION);
	else if ((dev_priv->flags & RADEON_FAMILY_MASK) == CHIP_RS690)
		return RS690_READ_MCIND(dev_priv, RS690_MC_FB_LOCATION);
	else if ((dev_priv->flags & RADEON_FAMILY_MASK) >= CHIP_RV770)
		return RADEON_READ(R700_MC_VM_FB_LOCATION);
	else if ((dev_priv->flags & RADEON_FAMILY_MASK) >= CHIP_R600)
		return RADEON_READ(R600_MC_VM_FB_LOCATION);
	else if ((dev_priv->flags & RADEON_FAMILY_MASK) > CHIP_RV515)
		return R500_READ_MCIND(dev_priv, R520_MC_FB_LOCATION);
	else
		return RADEON_READ(RADEON_MC_FB_LOCATION);
}

void radeon_read_agp_location(drm_radeon_private_t *dev_priv, u32 *agp_lo, u32 *agp_hi)
{
	if (dev_priv->chip_family == CHIP_RV770) {
		*agp_lo = RADEON_READ(R600_MC_VM_AGP_BOT);
		*agp_hi = RADEON_READ(R600_MC_VM_AGP_TOP);
	} else if (dev_priv->chip_family == CHIP_R600) {
		*agp_lo = RADEON_READ(R600_MC_VM_AGP_BOT);
		*agp_hi = RADEON_READ(R600_MC_VM_AGP_TOP);
	} else if (dev_priv->chip_family == CHIP_RV515) {
		*agp_lo = radeon_read_mc_reg(dev_priv, RV515_MC_AGP_LOCATION);
		*agp_hi = 0;
	} else if (dev_priv->chip_family == CHIP_RS600) {
		*agp_lo = 0;
		*agp_hi = 0;
	} else if (dev_priv->chip_family == CHIP_RS690 ||
		   dev_priv->chip_family == CHIP_RS740) {
		*agp_lo = radeon_read_mc_reg(dev_priv, RS690_MC_AGP_LOCATION);
		*agp_hi = 0;
	} else if (dev_priv->chip_family >= CHIP_R520) {
		*agp_lo = radeon_read_mc_reg(dev_priv, R520_MC_AGP_LOCATION);
		*agp_hi = 0;
	} else {
		*agp_lo = RADEON_READ(RADEON_MC_AGP_LOCATION);
		*agp_hi = 0;
	}
}

void radeon_write_fb_location(drm_radeon_private_t *dev_priv, u32 fb_loc)
{
	if ((dev_priv->flags & RADEON_FAMILY_MASK) == CHIP_RV515)
		R500_WRITE_MCIND(RV515_MC_FB_LOCATION, fb_loc);
	else if ((dev_priv->flags & RADEON_FAMILY_MASK) == CHIP_RS690)
		RS690_WRITE_MCIND(RS690_MC_FB_LOCATION, fb_loc);
	else if ((dev_priv->flags & RADEON_FAMILY_MASK) >= CHIP_RV770)
		RADEON_WRITE(R700_MC_VM_FB_LOCATION, fb_loc);
	else if ((dev_priv->flags & RADEON_FAMILY_MASK) >= CHIP_R600)
		RADEON_WRITE(R600_MC_VM_FB_LOCATION, fb_loc);
	else if ((dev_priv->flags & RADEON_FAMILY_MASK) > CHIP_RV515)
		R500_WRITE_MCIND(R520_MC_FB_LOCATION, fb_loc);
	else
		RADEON_WRITE(RADEON_MC_FB_LOCATION, fb_loc);
}

static void radeon_write_agp_location(drm_radeon_private_t *dev_priv, u32 agp_loc, u32 agp_loc_hi)
{
	if ((dev_priv->flags & RADEON_FAMILY_MASK) == CHIP_RV515)
		R500_WRITE_MCIND(RV515_MC_AGP_LOCATION, agp_loc);
	else if ((dev_priv->flags & RADEON_FAMILY_MASK) == CHIP_RS690)
		RS690_WRITE_MCIND(RS690_MC_AGP_LOCATION, agp_loc);
	else if ((dev_priv->flags & RADEON_FAMILY_MASK) >= CHIP_R600) {
		RADEON_WRITE(R600_MC_VM_AGP_BOT, agp_loc);
		RADEON_WRITE(R600_MC_VM_AGP_TOP, agp_loc_hi);
	} else if ((dev_priv->flags & RADEON_FAMILY_MASK) > CHIP_RV515)
		R500_WRITE_MCIND(R520_MC_AGP_LOCATION, agp_loc);
	else
		RADEON_WRITE(RADEON_MC_AGP_LOCATION, agp_loc);
}

static void radeon_write_agp_base(drm_radeon_private_t *dev_priv, u64 agp_base)
{
	u32 agp_base_hi = upper_32_bits(agp_base);
	u32 agp_base_lo = agp_base & 0xffffffff;

	if ((dev_priv->flags & RADEON_FAMILY_MASK) == CHIP_RV515) {
		R500_WRITE_MCIND(RV515_MC_AGP_BASE, agp_base_lo);
		R500_WRITE_MCIND(RV515_MC_AGP_BASE_2, agp_base_hi);
	} else if ((dev_priv->flags & RADEON_FAMILY_MASK) == CHIP_RS690) {
		RS690_WRITE_MCIND(RS690_MC_AGP_BASE, agp_base_lo);
		RS690_WRITE_MCIND(RS690_MC_AGP_BASE_2, agp_base_hi);
	} else if ((dev_priv->flags & RADEON_FAMILY_MASK) > CHIP_RV515) {
		R500_WRITE_MCIND(R520_MC_AGP_BASE, agp_base_lo);
		R500_WRITE_MCIND(R520_MC_AGP_BASE_2, agp_base_hi);
	} else if (((dev_priv->flags & RADEON_FAMILY_MASK) == CHIP_RS400) ||
		   ((dev_priv->flags & RADEON_FAMILY_MASK) == CHIP_RS480)) {
		RADEON_WRITE(RADEON_AGP_BASE, agp_base_lo);
		RADEON_WRITE(RS480_AGP_BASE_2, agp_base_hi);
	} else {
		RADEON_WRITE(RADEON_AGP_BASE, agp_base_lo);
		if ((dev_priv->flags & RADEON_FAMILY_MASK) >= CHIP_R200)
			RADEON_WRITE(RADEON_AGP_BASE_2, agp_base_hi);
	}
}

void radeon_enable_bm(struct drm_radeon_private *dev_priv)
{
	u32 tmp;
	/* Turn on bus mastering */
	if (((dev_priv->flags & RADEON_FAMILY_MASK) == CHIP_RS690) ||
	    ((dev_priv->flags & RADEON_FAMILY_MASK) == CHIP_RS740)) {
		/* rs600/rs690/rs740 */
		tmp = RADEON_READ(RADEON_BUS_CNTL) & ~RS600_BUS_MASTER_DIS;
		RADEON_WRITE(RADEON_BUS_CNTL, tmp);
	} else if (((dev_priv->flags & RADEON_FAMILY_MASK) <= CHIP_RV350) ||
		   ((dev_priv->flags & RADEON_FAMILY_MASK) == CHIP_R420) ||
		   ((dev_priv->flags & RADEON_FAMILY_MASK) == CHIP_RS400) ||
		   ((dev_priv->flags & RADEON_FAMILY_MASK) == CHIP_RS480)) {
		/* r1xx, r2xx, r300, r(v)350, r420/r481, rs400/rs480 */
		tmp = RADEON_READ(RADEON_BUS_CNTL) & ~RADEON_BUS_MASTER_DIS;
		RADEON_WRITE(RADEON_BUS_CNTL, tmp);
	} /* PCIE cards appears to not need this */
}

void radeon_pll_errata_after_index(struct drm_radeon_private *dev_priv)
{
	if (!(dev_priv->pll_errata & CHIP_ERRATA_PLL_DUMMYREADS))
		return;

	(void)RADEON_READ(RADEON_CLOCK_CNTL_DATA);
	(void)RADEON_READ(RADEON_CRTC_GEN_CNTL);
}

void radeon_pll_errata_after_data(struct drm_radeon_private *dev_priv)
{
	/* This workarounds is necessary on RV100, RS100 and RS200 chips
	 * or the chip could hang on a subsequent access
	 */
	if (dev_priv->pll_errata & CHIP_ERRATA_PLL_DELAY)
		udelay(5000);

	/* This function is required to workaround a hardware bug in some (all?)
	 * revisions of the R300.  This workaround should be called after every
	 * CLOCK_CNTL_INDEX register access.  If not, register reads afterward
	 * may not be correct.
	 */
	if (dev_priv->pll_errata & CHIP_ERRATA_R300_CG) {
		uint32_t save, tmp;

		save = RADEON_READ(RADEON_CLOCK_CNTL_INDEX);
		tmp = save & ~(0x3f | RADEON_PLL_WR_EN);
		RADEON_WRITE(RADEON_CLOCK_CNTL_INDEX, tmp);
		tmp = RADEON_READ(RADEON_CLOCK_CNTL_DATA);
		RADEON_WRITE(RADEON_CLOCK_CNTL_INDEX, save);
	}
}

u32 RADEON_READ_PLL(struct drm_radeon_private *dev_priv, int addr)
{
	uint32_t data;

	RADEON_WRITE8(RADEON_CLOCK_CNTL_INDEX, addr & 0x3f);
	radeon_pll_errata_after_index(dev_priv);
	data = RADEON_READ(RADEON_CLOCK_CNTL_DATA);
	radeon_pll_errata_after_data(dev_priv);
	return data;
}

void RADEON_WRITE_PLL(struct drm_radeon_private *dev_priv, int addr, uint32_t data)
{
	RADEON_WRITE8(RADEON_CLOCK_CNTL_INDEX, ((addr & 0x3f) | RADEON_PLL_WR_EN));
	radeon_pll_errata_after_index(dev_priv);
	RADEON_WRITE(RADEON_CLOCK_CNTL_DATA, data);
	radeon_pll_errata_after_data(dev_priv);
}

u32 RADEON_READ_PCIE(drm_radeon_private_t *dev_priv, int addr)
{
	RADEON_WRITE8(RADEON_PCIE_INDEX, addr & 0xff);
	return RADEON_READ(RADEON_PCIE_DATA);
}

/* ATOM accessor methods */
static uint32_t cail_mc_read(struct card_info *info, uint32_t reg)
{
	uint32_t ret = radeon_read_mc_reg(info->dev->dev_private, reg);

	//	DRM_DEBUG("(%x) = %x\n", reg, ret);
	return ret;
}

static void cail_mc_write(struct card_info *info, uint32_t reg, uint32_t val)
{
  //	DRM_DEBUG("(%x,  %x)\n", reg, val);
	radeon_write_mc_reg(info->dev->dev_private, reg, val);
}

static void cail_reg_write(struct card_info *info, uint32_t reg, uint32_t val)
{
	drm_radeon_private_t *dev_priv = info->dev->dev_private;
	
	//	DRM_DEBUG("(%x,  %x)\n", reg*4, val);
	RADEON_WRITE(reg*4, val);
}

static uint32_t cail_reg_read(struct card_info *info, uint32_t reg)
{
	uint32_t ret;
	drm_radeon_private_t *dev_priv = info->dev->dev_private;

	ret = RADEON_READ(reg*4);
	//	DRM_DEBUG("(%x) = %x\n", reg*4, ret);
	return ret;
}

#if RADEON_FIFO_DEBUG
static void radeon_status(drm_radeon_private_t * dev_priv)
{
	printk("%s:\n", __FUNCTION__);
	printk("RBBM_STATUS = 0x%08x\n",
	       (unsigned int)RADEON_READ(RADEON_RBBM_STATUS));
	printk("CP_RB_RTPR = 0x%08x\n",
	       (unsigned int)RADEON_READ(RADEON_CP_RB_RPTR));
	printk("CP_RB_WTPR = 0x%08x\n",
	       (unsigned int)RADEON_READ(RADEON_CP_RB_WPTR));
	printk("AIC_CNTL = 0x%08x\n",
	       (unsigned int)RADEON_READ(RADEON_AIC_CNTL));
	printk("AIC_STAT = 0x%08x\n",
	       (unsigned int)RADEON_READ(RADEON_AIC_STAT));
	printk("AIC_PT_BASE = 0x%08x\n",
	       (unsigned int)RADEON_READ(RADEON_AIC_PT_BASE));
	printk("TLB_ADDR = 0x%08x\n",
	       (unsigned int)RADEON_READ(RADEON_AIC_TLB_ADDR));
	printk("TLB_DATA = 0x%08x\n",
	       (unsigned int)RADEON_READ(RADEON_AIC_TLB_DATA));
}
#endif

/* ================================================================
 * Engine, FIFO control
 */

static int radeon_do_pixcache_flush(drm_radeon_private_t * dev_priv)
{
	u32 tmp;
	int i;

	dev_priv->stats.boxes |= RADEON_BOX_WAIT_IDLE;

	if ((dev_priv->flags & RADEON_FAMILY_MASK) <= CHIP_RV280) {
		tmp = RADEON_READ(RADEON_RB3D_DSTCACHE_CTLSTAT);
		tmp |= RADEON_RB3D_DC_FLUSH_ALL;
		RADEON_WRITE(RADEON_RB3D_DSTCACHE_CTLSTAT, tmp);

		for (i = 0; i < dev_priv->usec_timeout; i++) {
			if (!(RADEON_READ(RADEON_RB3D_DSTCACHE_CTLSTAT)
			      & RADEON_RB3D_DC_BUSY)) {
				return 0;
			}
			DRM_UDELAY(1);
		}
	} else {
		/* don't flush or purge cache here or lockup */
		return 0;
	}

#if RADEON_FIFO_DEBUG
	DRM_ERROR("failed!\n");
	radeon_status(dev_priv);
#endif
	return -EBUSY;
}

static int radeon_do_wait_for_fifo(drm_radeon_private_t * dev_priv, int entries)
{
	int i;

	dev_priv->stats.boxes |= RADEON_BOX_WAIT_IDLE;

	for (i = 0; i < dev_priv->usec_timeout; i++) {
		int slots = (RADEON_READ(RADEON_RBBM_STATUS)
			     & RADEON_RBBM_FIFOCNT_MASK);
		if (slots >= entries)
			return 0;
		DRM_UDELAY(1);
	}
	DRM_INFO("wait for fifo failed status : 0x%08X 0x%08X\n",
		 RADEON_READ(RADEON_RBBM_STATUS),
		 RADEON_READ(R300_VAP_CNTL_STATUS));

#if RADEON_FIFO_DEBUG
	DRM_ERROR("failed!\n");
	radeon_status(dev_priv);
#endif
	return -EBUSY;
}

int radeon_do_wait_for_idle(drm_radeon_private_t * dev_priv)
{
	int i, ret;

	dev_priv->stats.boxes |= RADEON_BOX_WAIT_IDLE;

	ret = radeon_do_wait_for_fifo(dev_priv, 64);
	if (ret)
		return ret;

	for (i = 0; i < dev_priv->usec_timeout; i++) {
		if (!(RADEON_READ(RADEON_RBBM_STATUS)
		      & RADEON_RBBM_ACTIVE)) {
			radeon_do_pixcache_flush(dev_priv);
			return 0;
		}
		DRM_UDELAY(1);
	}
	DRM_INFO("wait idle failed status : 0x%08X 0x%08X\n",
		 RADEON_READ(RADEON_RBBM_STATUS),
		 RADEON_READ(R300_VAP_CNTL_STATUS));

#if RADEON_FIFO_DEBUG
	DRM_ERROR("failed!\n");
	radeon_status(dev_priv);
#endif
	return -EBUSY;
}

static void radeon_init_pipes(drm_radeon_private_t * dev_priv)
{
	uint32_t gb_tile_config, gb_pipe_sel = 0;

	/* RS4xx/RS6xx/R4xx/R5xx */
	if ((dev_priv->flags & RADEON_FAMILY_MASK) >= CHIP_R420) {
		gb_pipe_sel = RADEON_READ(R400_GB_PIPE_SELECT);
		dev_priv->num_gb_pipes = ((gb_pipe_sel >> 12) & 0x3) + 1;
	} else {
		/* R3xx */
		if (((dev_priv->flags & RADEON_FAMILY_MASK) == CHIP_R300) ||
		    ((dev_priv->flags & RADEON_FAMILY_MASK) == CHIP_R350)) {
			dev_priv->num_gb_pipes = 2;
		} else {
			/* R3Vxx */
			dev_priv->num_gb_pipes = 1;
		}
	}
	DRM_INFO("Num pipes: %d\n", dev_priv->num_gb_pipes);

	gb_tile_config = (R300_ENABLE_TILING | R300_TILE_SIZE_16 /*| R300_SUBPIXEL_1_16*/);

	switch(dev_priv->num_gb_pipes) {
	case 2: gb_tile_config |= R300_PIPE_COUNT_R300; break;
	case 3: gb_tile_config |= R300_PIPE_COUNT_R420_3P; break;
	case 4: gb_tile_config |= R300_PIPE_COUNT_R420; break;
	default:
	case 1: gb_tile_config |= R300_PIPE_COUNT_RV350; break;
	}

	if ((dev_priv->flags & RADEON_FAMILY_MASK) >= CHIP_RV515) {
		RADEON_WRITE_PLL(dev_priv, R500_DYN_SCLK_PWMEM_PIPE, (1 | ((gb_pipe_sel >> 8) & 0xf) << 4));
		RADEON_WRITE(R500_SU_REG_DEST, ((1 << dev_priv->num_gb_pipes) - 1));
	}
	RADEON_WRITE(R300_GB_TILE_CONFIG, gb_tile_config);
	radeon_do_wait_for_idle(dev_priv);
	RADEON_WRITE(R300_DST_PIPE_CONFIG, RADEON_READ(R300_DST_PIPE_CONFIG) | R300_PIPE_AUTO_CONFIG);
	RADEON_WRITE(R300_RB2D_DSTCACHE_MODE, (RADEON_READ(R300_RB2D_DSTCACHE_MODE) |
					       R300_DC_AUTOFLUSH_ENABLE |
					       R300_DC_DC_DISABLE_IGNORE_PE));


}

/* ================================================================
 * CP control, initialization
 */

/* Load the microcode for the CP */
static void radeon_cp_load_microcode(drm_radeon_private_t * dev_priv)
{
	int i;
	DRM_DEBUG("\n");

	radeon_do_wait_for_idle(dev_priv);

	RADEON_WRITE(RADEON_CP_ME_RAM_ADDR, 0);

	if (((dev_priv->flags & RADEON_FAMILY_MASK) == CHIP_R100) ||
	    ((dev_priv->flags & RADEON_FAMILY_MASK) == CHIP_RV100) ||
	    ((dev_priv->flags & RADEON_FAMILY_MASK) == CHIP_RV200) ||
	    ((dev_priv->flags & RADEON_FAMILY_MASK) == CHIP_RS100) ||
	    ((dev_priv->flags & RADEON_FAMILY_MASK) == CHIP_RS200)) {
		DRM_INFO("Loading R100 Microcode\n");
		for (i = 0; i < 256; i++) {
			RADEON_WRITE(RADEON_CP_ME_RAM_DATAH,
				     R100_cp_microcode[i][1]);
			RADEON_WRITE(RADEON_CP_ME_RAM_DATAL,
				     R100_cp_microcode[i][0]);
		}
	} else if (((dev_priv->flags & RADEON_FAMILY_MASK) == CHIP_R200) ||
		   ((dev_priv->flags & RADEON_FAMILY_MASK) == CHIP_RV250) ||
		   ((dev_priv->flags & RADEON_FAMILY_MASK) == CHIP_RV280) ||
		   ((dev_priv->flags & RADEON_FAMILY_MASK) == CHIP_RS300)) {
		DRM_INFO("Loading R200 Microcode\n");
		for (i = 0; i < 256; i++) {
			RADEON_WRITE(RADEON_CP_ME_RAM_DATAH,
				     R200_cp_microcode[i][1]);
			RADEON_WRITE(RADEON_CP_ME_RAM_DATAL,
				     R200_cp_microcode[i][0]);
		}
	} else if (((dev_priv->flags & RADEON_FAMILY_MASK) == CHIP_R300) ||
		   ((dev_priv->flags & RADEON_FAMILY_MASK) == CHIP_R350) ||
		   ((dev_priv->flags & RADEON_FAMILY_MASK) == CHIP_RV350) ||
		   ((dev_priv->flags & RADEON_FAMILY_MASK) == CHIP_RV380) ||
		   ((dev_priv->flags & RADEON_FAMILY_MASK) == CHIP_RS400) ||
		   ((dev_priv->flags & RADEON_FAMILY_MASK) == CHIP_RS480)) {
		DRM_INFO("Loading R300 Microcode\n");
		for (i = 0; i < 256; i++) {
			RADEON_WRITE(RADEON_CP_ME_RAM_DATAH,
				     R300_cp_microcode[i][1]);
			RADEON_WRITE(RADEON_CP_ME_RAM_DATAL,
				     R300_cp_microcode[i][0]);
		}
	} else if (((dev_priv->flags & RADEON_FAMILY_MASK) == CHIP_R420) ||
		   ((dev_priv->flags & RADEON_FAMILY_MASK) == CHIP_RV410)) {
		DRM_INFO("Loading R400 Microcode\n");
		for (i = 0; i < 256; i++) {
			RADEON_WRITE(RADEON_CP_ME_RAM_DATAH,
				     R420_cp_microcode[i][1]);
			RADEON_WRITE(RADEON_CP_ME_RAM_DATAL,
				     R420_cp_microcode[i][0]);
		}
	} else if ((dev_priv->flags & RADEON_FAMILY_MASK) == CHIP_RS690) {
		DRM_INFO("Loading RS690 Microcode\n");
		for (i = 0; i < 256; i++) {
			RADEON_WRITE(RADEON_CP_ME_RAM_DATAH,
				     RS690_cp_microcode[i][1]);
			RADEON_WRITE(RADEON_CP_ME_RAM_DATAL,
				     RS690_cp_microcode[i][0]);
		}
	} else if (((dev_priv->flags & RADEON_FAMILY_MASK) == CHIP_RV515) ||
		   ((dev_priv->flags & RADEON_FAMILY_MASK) == CHIP_R520) ||
		   ((dev_priv->flags & RADEON_FAMILY_MASK) == CHIP_RV530) ||
		   ((dev_priv->flags & RADEON_FAMILY_MASK) == CHIP_R580) ||
		   ((dev_priv->flags & RADEON_FAMILY_MASK) == CHIP_RV560) ||
		   ((dev_priv->flags & RADEON_FAMILY_MASK) == CHIP_RV570)) {
		DRM_INFO("Loading R500 Microcode\n");
		for (i = 0; i < 256; i++) {
			RADEON_WRITE(RADEON_CP_ME_RAM_DATAH,
				     R520_cp_microcode[i][1]);
			RADEON_WRITE(RADEON_CP_ME_RAM_DATAL,
				     R520_cp_microcode[i][0]);
		}
	}
}

/* Flush any pending commands to the CP.  This should only be used just
 * prior to a wait for idle, as it informs the engine that the command
 * stream is ending.
 */
static void radeon_do_cp_flush(drm_radeon_private_t * dev_priv)
{
	DRM_DEBUG("\n");
#if 0
	u32 tmp;
	tmp = RADEON_READ(RADEON_CP_RB_WPTR) | (1 << 31);
	RADEON_WRITE(RADEON_CP_RB_WPTR, tmp);
#endif
}

/* Wait for the CP to go idle.
 */
int radeon_do_cp_idle(drm_radeon_private_t * dev_priv)
{
	RING_LOCALS;
	DRM_DEBUG("\n");

	BEGIN_RING(6);

	RADEON_PURGE_CACHE();
	RADEON_PURGE_ZCACHE();
	RADEON_WAIT_UNTIL_IDLE();

	ADVANCE_RING();
	COMMIT_RING();

	return radeon_do_wait_for_idle(dev_priv);
}

/* Start the Command Processor.
 */
static void radeon_do_cp_start(drm_radeon_private_t * dev_priv)
{
	RING_LOCALS;
	DRM_DEBUG("\n");

	radeon_do_wait_for_idle(dev_priv);

	RADEON_WRITE(RADEON_CP_CSQ_CNTL, dev_priv->cp_mode);

	dev_priv->cp_running = 1;

	BEGIN_RING(8);
	/* isync can only be written through cp on r5xx write it here */
	OUT_RING(CP_PACKET0(RADEON_ISYNC_CNTL, 0));
	OUT_RING(RADEON_ISYNC_ANY2D_IDLE3D |
		 RADEON_ISYNC_ANY3D_IDLE2D |
		 RADEON_ISYNC_WAIT_IDLEGUI |
		 RADEON_ISYNC_CPSCRATCH_IDLEGUI);
	RADEON_PURGE_CACHE();
	RADEON_PURGE_ZCACHE();
	RADEON_WAIT_UNTIL_IDLE();
	ADVANCE_RING();
	COMMIT_RING();

	dev_priv->track_flush |= RADEON_FLUSH_EMITED | RADEON_PURGE_EMITED;
}

/* Reset the Command Processor.  This will not flush any pending
 * commands, so you must wait for the CP command stream to complete
 * before calling this routine.
 */
static void radeon_do_cp_reset(drm_radeon_private_t * dev_priv)
{
	u32 cur_read_ptr;
	DRM_DEBUG("\n");

	cur_read_ptr = RADEON_READ(RADEON_CP_RB_RPTR);
	RADEON_WRITE(RADEON_CP_RB_WPTR, cur_read_ptr);
	SET_RING_HEAD(dev_priv, cur_read_ptr);
	dev_priv->ring.tail = cur_read_ptr;
}

/* Stop the Command Processor.  This will not flush any pending
 * commands, so you must flush the command stream and wait for the CP
 * to go idle before calling this routine.
 */
static void radeon_do_cp_stop(drm_radeon_private_t * dev_priv)
{
	DRM_DEBUG("\n");

	RADEON_WRITE(RADEON_CP_CSQ_CNTL, RADEON_CSQ_PRIDIS_INDDIS);

	dev_priv->cp_running = 0;
}

/* Reset the engine.  This will stop the CP if it is running.
 */
static int radeon_do_engine_reset(struct drm_device * dev)
{
	drm_radeon_private_t *dev_priv = dev->dev_private;
	u32 clock_cntl_index = 0, mclk_cntl = 0, rbbm_soft_reset;
	DRM_DEBUG("\n");

	radeon_do_pixcache_flush(dev_priv);

	if ((dev_priv->flags & RADEON_FAMILY_MASK) <= CHIP_RV410) {
	        /* may need something similar for newer chips */
		clock_cntl_index = RADEON_READ(RADEON_CLOCK_CNTL_INDEX);
		mclk_cntl = RADEON_READ_PLL(dev_priv, RADEON_MCLK_CNTL);

		RADEON_WRITE_PLL(dev_priv, RADEON_MCLK_CNTL, (mclk_cntl |
							      RADEON_FORCEON_MCLKA |
							      RADEON_FORCEON_MCLKB |
							      RADEON_FORCEON_YCLKA |
							      RADEON_FORCEON_YCLKB |
							      RADEON_FORCEON_MC |
							      RADEON_FORCEON_AIC));
	}

	rbbm_soft_reset = RADEON_READ(RADEON_RBBM_SOFT_RESET);

	RADEON_WRITE(RADEON_RBBM_SOFT_RESET, (rbbm_soft_reset |
					      RADEON_SOFT_RESET_CP |
					      RADEON_SOFT_RESET_HI |
					      RADEON_SOFT_RESET_SE |
					      RADEON_SOFT_RESET_RE |
					      RADEON_SOFT_RESET_PP |
					      RADEON_SOFT_RESET_E2 |
					      RADEON_SOFT_RESET_RB));
	RADEON_READ(RADEON_RBBM_SOFT_RESET);
	RADEON_WRITE(RADEON_RBBM_SOFT_RESET, (rbbm_soft_reset &
					      ~(RADEON_SOFT_RESET_CP |
						RADEON_SOFT_RESET_HI |
						RADEON_SOFT_RESET_SE |
						RADEON_SOFT_RESET_RE |
						RADEON_SOFT_RESET_PP |
						RADEON_SOFT_RESET_E2 |
						RADEON_SOFT_RESET_RB)));
	RADEON_READ(RADEON_RBBM_SOFT_RESET);

	if ((dev_priv->flags & RADEON_FAMILY_MASK) <= CHIP_RV410) {
		RADEON_WRITE_PLL(dev_priv, RADEON_MCLK_CNTL, mclk_cntl);
		RADEON_WRITE(RADEON_CLOCK_CNTL_INDEX, clock_cntl_index);
		RADEON_WRITE(RADEON_RBBM_SOFT_RESET, rbbm_soft_reset);
	}

	/* setup the raster pipes */
	if ((dev_priv->flags & RADEON_FAMILY_MASK) >= CHIP_R300)
	    radeon_init_pipes(dev_priv);

	/* Reset the CP ring */
	radeon_do_cp_reset(dev_priv);

	/* The CP is no longer running after an engine reset */
	dev_priv->cp_running = 0;

	/* Reset any pending vertex, indirect buffers */
	if (dev->dma)
		radeon_freelist_reset(dev);

	return 0;
}

static void radeon_cp_init_ring_buffer(struct drm_device * dev,
				       drm_radeon_private_t * dev_priv)
{
	u32 ring_start, cur_read_ptr;

	/* Initialize the memory controller. With new memory map, the fb location
	 * is not changed, it should have been properly initialized already. Part
	 * of the problem is that the code below is bogus, assuming the GART is
	 * always appended to the fb which is not necessarily the case
	 */
	if (!dev_priv->new_memmap)
		radeon_write_fb_location(dev_priv,
					 ((dev_priv->gart_vm_start - 1) & 0xffff0000)
					 | (dev_priv->fb_location >> 16));
	
	if (dev_priv->mm.ring.bo) {
		ring_start = dev_priv->mm.ring.bo->offset +
			dev_priv->gart_vm_start;
	} else
#if __OS_HAS_AGP
	if (dev_priv->flags & RADEON_IS_AGP) {
		radeon_write_agp_base(dev_priv, dev->agp->base);

		radeon_write_agp_location(dev_priv,
			     (((dev_priv->gart_vm_start - 1 +
				dev_priv->gart_size) & 0xffff0000) |
			      (dev_priv->gart_vm_start >> 16)), 0);

		ring_start = (dev_priv->cp_ring->offset
			      - dev->agp->base
			      + dev_priv->gart_vm_start);
	} else
#endif
		ring_start = (dev_priv->cp_ring->offset
			      - (unsigned long)dev->sg->virtual
			      + dev_priv->gart_vm_start);

	RADEON_WRITE(RADEON_CP_RB_BASE, ring_start);

	/* Set the write pointer delay */
	RADEON_WRITE(RADEON_CP_RB_WPTR_DELAY, 0);

	/* Initialize the ring buffer's read and write pointers */
	cur_read_ptr = RADEON_READ(RADEON_CP_RB_RPTR);
	RADEON_WRITE(RADEON_CP_RB_WPTR, cur_read_ptr);
	SET_RING_HEAD(dev_priv, cur_read_ptr);
	dev_priv->ring.tail = cur_read_ptr;


	if (dev_priv->mm.ring_read.bo) {
		RADEON_WRITE(RADEON_CP_RB_RPTR_ADDR,
			     dev_priv->mm.ring_read.bo->offset +
			     dev_priv->gart_vm_start);
	} else
#if __OS_HAS_AGP
	if (dev_priv->flags & RADEON_IS_AGP) {
		RADEON_WRITE(RADEON_CP_RB_RPTR_ADDR,
			     dev_priv->ring_rptr->offset
			     - dev->agp->base + dev_priv->gart_vm_start);
	} else
#endif
	{
		struct drm_sg_mem *entry = dev->sg;
		unsigned long tmp_ofs, page_ofs;

		tmp_ofs = dev_priv->ring_rptr->offset -
				(unsigned long)dev->sg->virtual;
		page_ofs = tmp_ofs >> PAGE_SHIFT;

		RADEON_WRITE(RADEON_CP_RB_RPTR_ADDR, entry->busaddr[page_ofs]);
		DRM_DEBUG("ring rptr: offset=0x%08lx handle=0x%08lx\n",
			  (unsigned long)entry->busaddr[page_ofs],
			  entry->handle + tmp_ofs);
	}

	/* Set ring buffer size */
#ifdef __BIG_ENDIAN
	RADEON_WRITE(RADEON_CP_RB_CNTL,
		     RADEON_BUF_SWAP_32BIT |
		     (dev_priv->ring.fetch_size_l2ow << 18) |
		     (dev_priv->ring.rptr_update_l2qw << 8) |
		     dev_priv->ring.size_l2qw);
#else
	RADEON_WRITE(RADEON_CP_RB_CNTL,
		     (dev_priv->ring.fetch_size_l2ow << 18) |
		     (dev_priv->ring.rptr_update_l2qw << 8) |
		     dev_priv->ring.size_l2qw);
#endif

	/* Initialize the scratch register pointer.  This will cause
	 * the scratch register values to be written out to memory
	 * whenever they are updated.
	 *
	 * We simply put this behind the ring read pointer, this works
	 * with PCI GART as well as (whatever kind of) AGP GART
	 */
	RADEON_WRITE(RADEON_SCRATCH_ADDR, RADEON_READ(RADEON_CP_RB_RPTR_ADDR)
		     + RADEON_SCRATCH_REG_OFFSET);

	if (dev_priv->mm.ring_read.bo)
		dev_priv->scratch = ((__volatile__ u32 *)
				     dev_priv->mm.ring_read.kmap.virtual +
				     (RADEON_SCRATCH_REG_OFFSET / sizeof(u32)));
	else
		dev_priv->scratch = ((__volatile__ u32 *)
				     dev_priv->ring_rptr->handle +
				     (RADEON_SCRATCH_REG_OFFSET / sizeof(u32)));

	if (dev_priv->chip_family >= CHIP_R300)
		RADEON_WRITE(RADEON_SCRATCH_UMSK, 0x7f); 
	else
		RADEON_WRITE(RADEON_SCRATCH_UMSK, 0x1f); 

 	radeon_enable_bm(dev_priv);

	dev_priv->scratch[0] = 0;
	RADEON_WRITE(RADEON_LAST_FRAME_REG, 0);

	dev_priv->scratch[1] = 0;
	RADEON_WRITE(RADEON_LAST_DISPATCH_REG, 0);

	dev_priv->scratch[2] = 0;
	RADEON_WRITE(RADEON_LAST_CLEAR_REG, 0);

	dev_priv->scratch[3] = 0;
	RADEON_WRITE(RADEON_LAST_SWI_REG, 0);

	dev_priv->scratch[4] = 0;
	RADEON_WRITE(RADEON_SCRATCH_REG4, 0);

	dev_priv->scratch[6] = 0;
	RADEON_WRITE(RADEON_SCRATCH_REG6, 0);

	radeon_do_wait_for_idle(dev_priv);

	/* Sync everything up */
	if (dev_priv->chip_family > CHIP_RV280) {
	RADEON_WRITE(RADEON_ISYNC_CNTL,
		     (RADEON_ISYNC_ANY2D_IDLE3D |
		      RADEON_ISYNC_ANY3D_IDLE2D |
		      RADEON_ISYNC_WAIT_IDLEGUI |
		      RADEON_ISYNC_CPSCRATCH_IDLEGUI));
	} else {
	RADEON_WRITE(RADEON_ISYNC_CNTL,
		     (RADEON_ISYNC_ANY2D_IDLE3D |
		      RADEON_ISYNC_ANY3D_IDLE2D |
		      RADEON_ISYNC_WAIT_IDLEGUI));
	}
}

static void radeon_test_writeback(drm_radeon_private_t * dev_priv)
{
	u32 tmp, scratch1_store;
	void *ring_read_ptr;

	if (dev_priv->mm.ring_read.bo)
		ring_read_ptr = dev_priv->mm.ring_read.kmap.virtual;
	else
		ring_read_ptr = dev_priv->ring_rptr->handle;

	scratch1_store = RADEON_READ(RADEON_SCRATCH_REG1);
	/* Writeback doesn't seem to work everywhere, test it here and possibly
	 * enable it if it appears to work
	 */
	writel(0, ring_read_ptr + RADEON_SCRATCHOFF(1));
	RADEON_WRITE(RADEON_SCRATCH_REG1, 0xdeadbeef);

	for (tmp = 0; tmp < dev_priv->usec_timeout; tmp++) {
		if (readl(ring_read_ptr + RADEON_SCRATCHOFF(1)) ==
		    0xdeadbeef)
			break;
		DRM_UDELAY(1);
	}

	if (tmp < dev_priv->usec_timeout) {
		dev_priv->writeback_works = 1;
		DRM_INFO("writeback test succeeded in %d usecs\n", tmp);
	} else {
		dev_priv->writeback_works = 0;
		DRM_INFO("writeback test failed\n");
	}
	if (radeon_no_wb == 1) {
		dev_priv->writeback_works = 0;
		DRM_INFO("writeback forced off\n");
	}

	/* write back previous value */
	RADEON_WRITE(RADEON_SCRATCH_REG1, scratch1_store);

	if (!dev_priv->writeback_works) {
		/* Disable writeback to avoid unnecessary bus master transfers */
		RADEON_WRITE(RADEON_CP_RB_CNTL, RADEON_READ(RADEON_CP_RB_CNTL) | RADEON_RB_NO_UPDATE);
		RADEON_WRITE(RADEON_SCRATCH_UMSK, 0);
	}
}

/* Enable or disable IGP GART on the chip */
static void radeon_set_igpgart(drm_radeon_private_t * dev_priv, int on)
{
	u32 temp;

	if (on) {
		DRM_DEBUG("programming igp gart %08X %08lX %08X\n",
			 dev_priv->gart_vm_start,
			 (long)dev_priv->gart_info.bus_addr,
			 dev_priv->gart_size);

		temp = IGP_READ_MCIND(dev_priv, RS480_MC_MISC_CNTL);

		if ((dev_priv->flags & RADEON_FAMILY_MASK) == CHIP_RS690)
			IGP_WRITE_MCIND(RS480_MC_MISC_CNTL, (RS480_GART_INDEX_REG_EN |
							     RS690_BLOCK_GFX_D3_EN));
		else
			IGP_WRITE_MCIND(RS480_MC_MISC_CNTL, RS480_GART_INDEX_REG_EN);

		IGP_WRITE_MCIND(RS480_AGP_ADDRESS_SPACE_SIZE, (RS480_GART_EN |
							       RS480_VA_SIZE_32MB));

		temp = IGP_READ_MCIND(dev_priv, RS480_GART_FEATURE_ID);
		IGP_WRITE_MCIND(RS480_GART_FEATURE_ID, (RS480_HANG_EN |
							RS480_TLB_ENABLE |
							RS480_GTW_LAC_EN |
							RS480_1LEVEL_GART));

		temp = dev_priv->gart_info.bus_addr & 0xfffff000;
		temp |= (upper_32_bits(dev_priv->gart_info.bus_addr) & 0xff) << 4;
		IGP_WRITE_MCIND(RS480_GART_BASE, temp);

		temp = IGP_READ_MCIND(dev_priv, RS480_AGP_MODE_CNTL);
		IGP_WRITE_MCIND(RS480_AGP_MODE_CNTL, ((1 << RS480_REQ_TYPE_SNOOP_SHIFT) |
						      RS480_REQ_TYPE_SNOOP_DIS));

		radeon_write_agp_base(dev_priv, dev_priv->gart_vm_start);

		dev_priv->gart_size = 32*1024*1024;
		temp = (((dev_priv->gart_vm_start - 1 + dev_priv->gart_size) & 
			0xffff0000) | (dev_priv->gart_vm_start >> 16));

		radeon_write_agp_location(dev_priv, temp, 0);

		temp = IGP_READ_MCIND(dev_priv, RS480_AGP_ADDRESS_SPACE_SIZE);
		IGP_WRITE_MCIND(RS480_AGP_ADDRESS_SPACE_SIZE, (RS480_GART_EN |
							       RS480_VA_SIZE_32MB));

		do {
			temp = IGP_READ_MCIND(dev_priv, RS480_GART_CACHE_CNTRL);
			if ((temp & RS480_GART_CACHE_INVALIDATE) == 0)
				break;
			DRM_UDELAY(1);
		} while(1);

		IGP_WRITE_MCIND(RS480_GART_CACHE_CNTRL,
				RS480_GART_CACHE_INVALIDATE);

		do {
			temp = IGP_READ_MCIND(dev_priv, RS480_GART_CACHE_CNTRL);
			if ((temp & RS480_GART_CACHE_INVALIDATE) == 0)
				break;
			DRM_UDELAY(1);
		} while(1);

		IGP_WRITE_MCIND(RS480_GART_CACHE_CNTRL, 0);
	} else {
		IGP_WRITE_MCIND(RS480_AGP_ADDRESS_SPACE_SIZE, 0);
	}
}

static void radeon_set_pciegart(drm_radeon_private_t * dev_priv, int on)
{
	u32 tmp = RADEON_READ_PCIE(dev_priv, RADEON_PCIE_TX_GART_CNTL);
	if (on) {

		DRM_DEBUG("programming pcie %08X %08lX %08X\n",
			  dev_priv->gart_vm_start,
			  (long)dev_priv->gart_info.bus_addr,
			  dev_priv->gart_size);
		RADEON_WRITE_PCIE(RADEON_PCIE_TX_DISCARD_RD_ADDR_LO,
				  dev_priv->gart_vm_start);
		RADEON_WRITE_PCIE(RADEON_PCIE_TX_GART_BASE,
				  dev_priv->gart_info.bus_addr);
		RADEON_WRITE_PCIE(RADEON_PCIE_TX_GART_START_LO,
				  dev_priv->gart_vm_start);
		RADEON_WRITE_PCIE(RADEON_PCIE_TX_GART_END_LO,
				  dev_priv->gart_vm_start +
				  dev_priv->gart_size - 1);

		radeon_write_agp_location(dev_priv, 0xffffffc0, 0); /* ?? */

		RADEON_WRITE_PCIE(RADEON_PCIE_TX_GART_CNTL,
				  RADEON_PCIE_TX_GART_EN);
	} else {
		RADEON_WRITE_PCIE(RADEON_PCIE_TX_GART_CNTL,
				  tmp & ~RADEON_PCIE_TX_GART_EN);
	}
}

/* Enable or disable PCI GART on the chip */
void radeon_set_pcigart(drm_radeon_private_t * dev_priv, int on)
{
	u32 tmp;

	if (((dev_priv->flags & RADEON_FAMILY_MASK) == CHIP_RS690) ||
	    (dev_priv->flags & RADEON_IS_IGPGART)) {
		radeon_set_igpgart(dev_priv, on);
		return;
	}

	if (dev_priv->flags & RADEON_IS_PCIE) {
		radeon_set_pciegart(dev_priv, on);
		return;
	}

	tmp = RADEON_READ(RADEON_AIC_CNTL);

	if (on) {
		RADEON_WRITE(RADEON_AIC_CNTL,
			     tmp | RADEON_PCIGART_TRANSLATE_EN);

		/* set PCI GART page-table base address
		 */
		RADEON_WRITE(RADEON_AIC_PT_BASE, dev_priv->gart_info.bus_addr);

		/* set address range for PCI address translate
		 */
		RADEON_WRITE(RADEON_AIC_LO_ADDR, dev_priv->gart_vm_start);
		RADEON_WRITE(RADEON_AIC_HI_ADDR, dev_priv->gart_vm_start
			     + dev_priv->gart_size - 1);

		/* Turn off AGP aperture -- is this required for PCI GART?
		 */
		radeon_write_agp_location(dev_priv, 0xffffffc0, 0);
		RADEON_WRITE(RADEON_AGP_COMMAND, 0);	/* clear AGP_COMMAND */
	} else {
		RADEON_WRITE(RADEON_AIC_CNTL,
			     tmp & ~RADEON_PCIGART_TRANSLATE_EN);
	}
}

static int radeon_do_init_cp(struct drm_device *dev, drm_radeon_init_t *init,
			     struct drm_file *file_priv)
{
	drm_radeon_private_t *dev_priv = dev->dev_private;
	struct drm_radeon_master_private *master_priv = file_priv->master->driver_priv;

	DRM_DEBUG("\n");

	/* if we require new memory map but we don't have it fail */
	if ((dev_priv->flags & RADEON_NEW_MEMMAP) && !dev_priv->new_memmap) {
		DRM_ERROR("Cannot initialise DRM on this card\nThis card requires a new X.org DDX for 3D\n");
		radeon_do_cleanup_cp(dev);
		return -EINVAL;
	}

	if (init->is_pci && (dev_priv->flags & RADEON_IS_AGP))
	{
		DRM_DEBUG("Forcing AGP card to PCI mode\n");
		dev_priv->flags &= ~RADEON_IS_AGP;
	}
	else if (!(dev_priv->flags & (RADEON_IS_AGP | RADEON_IS_PCI | RADEON_IS_PCIE))
		 && !init->is_pci)
	{
		DRM_DEBUG("Restoring AGP flag\n");
		dev_priv->flags |= RADEON_IS_AGP;
	}

	if ((!(dev_priv->flags & RADEON_IS_AGP)) && !dev->sg) {
		DRM_ERROR("PCI GART memory not allocated!\n");
		radeon_do_cleanup_cp(dev);
		return -EINVAL;
	}

	dev_priv->usec_timeout = init->usec_timeout;
	if (dev_priv->usec_timeout < 1 ||
	    dev_priv->usec_timeout > RADEON_MAX_USEC_TIMEOUT) {
		DRM_DEBUG("TIMEOUT problem!\n");
		radeon_do_cleanup_cp(dev);
		return -EINVAL;
	}

	/* Enable vblank on CRTC1 for older X servers
	 */
	dev_priv->vblank_crtc = DRM_RADEON_VBLANK_CRTC1;

	dev_priv->do_boxes = 0;
	dev_priv->cp_mode = init->cp_mode;

	/* We don't support anything other than bus-mastering ring mode,
	 * but the ring can be in either AGP or PCI space for the ring
	 * read pointer.
	 */
	if ((init->cp_mode != RADEON_CSQ_PRIBM_INDDIS) &&
	    (init->cp_mode != RADEON_CSQ_PRIBM_INDBM)) {
		DRM_DEBUG("BAD cp_mode (%x)!\n", init->cp_mode);
		radeon_do_cleanup_cp(dev);
		return -EINVAL;
	}

	switch (init->fb_bpp) {
	case 16:
		dev_priv->color_fmt = RADEON_COLOR_FORMAT_RGB565;
		break;
	case 32:
	default:
		dev_priv->color_fmt = RADEON_COLOR_FORMAT_ARGB8888;
		break;
	}
	dev_priv->front_offset = init->front_offset;
	dev_priv->front_pitch = init->front_pitch;
	dev_priv->back_offset = init->back_offset;
	dev_priv->back_pitch = init->back_pitch;

	switch (init->depth_bpp) {
	case 16:
		dev_priv->depth_fmt = RADEON_DEPTH_FORMAT_16BIT_INT_Z;
		break;
	case 32:
	default:
		dev_priv->depth_fmt = RADEON_DEPTH_FORMAT_24BIT_INT_Z;
		break;
	}
	dev_priv->depth_offset = init->depth_offset;
	dev_priv->depth_pitch = init->depth_pitch;

	/* Hardware state for depth clears.  Remove this if/when we no
	 * longer clear the depth buffer with a 3D rectangle.  Hard-code
	 * all values to prevent unwanted 3D state from slipping through
	 * and screwing with the clear operation.
	 */
	dev_priv->depth_clear.rb3d_cntl = (RADEON_PLANE_MASK_ENABLE |
					   (dev_priv->color_fmt << 10) |
					   (dev_priv->chip_family < CHIP_R200 ? RADEON_ZBLOCK16 : 0));

	dev_priv->depth_clear.rb3d_zstencilcntl =
	    (dev_priv->depth_fmt |
	     RADEON_Z_TEST_ALWAYS |
	     RADEON_STENCIL_TEST_ALWAYS |
	     RADEON_STENCIL_S_FAIL_REPLACE |
	     RADEON_STENCIL_ZPASS_REPLACE |
	     RADEON_STENCIL_ZFAIL_REPLACE | RADEON_Z_WRITE_ENABLE);

	dev_priv->depth_clear.se_cntl = (RADEON_FFACE_CULL_CW |
					 RADEON_BFACE_SOLID |
					 RADEON_FFACE_SOLID |
					 RADEON_FLAT_SHADE_VTX_LAST |
					 RADEON_DIFFUSE_SHADE_FLAT |
					 RADEON_ALPHA_SHADE_FLAT |
					 RADEON_SPECULAR_SHADE_FLAT |
					 RADEON_FOG_SHADE_FLAT |
					 RADEON_VTX_PIX_CENTER_OGL |
					 RADEON_ROUND_MODE_TRUNC |
					 RADEON_ROUND_PREC_8TH_PIX);


	dev_priv->ring_offset = init->ring_offset;
	dev_priv->ring_rptr_offset = init->ring_rptr_offset;
	dev_priv->buffers_offset = init->buffers_offset;
	dev_priv->gart_textures_offset = init->gart_textures_offset;

	master_priv->sarea = drm_getsarea(dev);
	if (!master_priv->sarea) {
		DRM_ERROR("could not find sarea!\n");
		radeon_do_cleanup_cp(dev);
		return -EINVAL;
	}

	dev_priv->cp_ring = drm_core_findmap(dev, init->ring_offset);
	if (!dev_priv->cp_ring) {
		DRM_ERROR("could not find cp ring region!\n");
		radeon_do_cleanup_cp(dev);
		return -EINVAL;
	}
	dev_priv->ring_rptr = drm_core_findmap(dev, init->ring_rptr_offset);
	if (!dev_priv->ring_rptr) {
		DRM_ERROR("could not find ring read pointer!\n");
		radeon_do_cleanup_cp(dev);
		return -EINVAL;
	}
	dev->agp_buffer_token = init->buffers_offset;
	dev->agp_buffer_map = drm_core_findmap(dev, init->buffers_offset);
	if (!dev->agp_buffer_map) {
		DRM_ERROR("could not find dma buffer region!\n");
		radeon_do_cleanup_cp(dev);
		return -EINVAL;
	}

	if (init->gart_textures_offset) {
		dev_priv->gart_textures =
		    drm_core_findmap(dev, init->gart_textures_offset);
		if (!dev_priv->gart_textures) {
			DRM_ERROR("could not find GART texture region!\n");
			radeon_do_cleanup_cp(dev);
			return -EINVAL;
		}
	}

#if __OS_HAS_AGP
	if (dev_priv->flags & RADEON_IS_AGP) {
		drm_core_ioremap(dev_priv->cp_ring, dev);
		drm_core_ioremap(dev_priv->ring_rptr, dev);
		drm_core_ioremap(dev->agp_buffer_map, dev);
		if (!dev_priv->cp_ring->handle ||
		    !dev_priv->ring_rptr->handle ||
		    !dev->agp_buffer_map->handle) {
			DRM_ERROR("could not find ioremap agp regions!\n");
			radeon_do_cleanup_cp(dev);
			return -EINVAL;
		}
	} else
#endif
	{
		dev_priv->cp_ring->handle = (void *)dev_priv->cp_ring->offset;
		dev_priv->ring_rptr->handle =
		    (void *)dev_priv->ring_rptr->offset;
		dev->agp_buffer_map->handle =
		    (void *)dev->agp_buffer_map->offset;

		DRM_DEBUG("dev_priv->cp_ring->handle %p\n",
			  dev_priv->cp_ring->handle);
		DRM_DEBUG("dev_priv->ring_rptr->handle %p\n",
			  dev_priv->ring_rptr->handle);
		DRM_DEBUG("dev->agp_buffer_map->handle %p\n",
			  dev->agp_buffer_map->handle);
	}

	dev_priv->fb_location = (radeon_read_fb_location(dev_priv) & 0xffff) << 16;
	dev_priv->fb_size =
		((radeon_read_fb_location(dev_priv) & 0xffff0000u) + 0x10000)
		- dev_priv->fb_location;

	dev_priv->front_pitch_offset = (((dev_priv->front_pitch / 64) << 22) |
					((dev_priv->front_offset
					  + dev_priv->fb_location) >> 10));

	dev_priv->back_pitch_offset = (((dev_priv->back_pitch / 64) << 22) |
				       ((dev_priv->back_offset
					 + dev_priv->fb_location) >> 10));

	dev_priv->depth_pitch_offset = (((dev_priv->depth_pitch / 64) << 22) |
					((dev_priv->depth_offset
					  + dev_priv->fb_location) >> 10));

	dev_priv->gart_size = init->gart_size;

	/* New let's set the memory map ... */
	if (dev_priv->new_memmap) {
		u32 base = 0;

		DRM_INFO("Setting GART location based on new memory map\n");

		/* If using AGP, try to locate the AGP aperture at the same
		 * location in the card and on the bus, though we have to
		 * align it down.
		 */
#if __OS_HAS_AGP
		if (dev_priv->flags & RADEON_IS_AGP) {
			base = dev->agp->base;
			/* Check if valid */
			if ((base + dev_priv->gart_size - 1) >= dev_priv->fb_location &&
			    base < (dev_priv->fb_location + dev_priv->fb_size - 1)) {
				DRM_INFO("Can't use AGP base @0x%08lx, won't fit\n",
					 dev->agp->base);
				base = 0;
			}
		}
#endif
		/* If not or if AGP is at 0 (Macs), try to put it elsewhere */
		if (base == 0) {
			base = dev_priv->fb_location + dev_priv->fb_size;
			if (base < dev_priv->fb_location ||
			    ((base + dev_priv->gart_size) & 0xfffffffful) < base)
				base = dev_priv->fb_location
					- dev_priv->gart_size;
		}
		dev_priv->gart_vm_start = base & 0xffc00000u;
		if (dev_priv->gart_vm_start != base)
			DRM_INFO("GART aligned down from 0x%08x to 0x%08x\n",
				 base, dev_priv->gart_vm_start);
	} else {
		DRM_INFO("Setting GART location based on old memory map\n");
		dev_priv->gart_vm_start = dev_priv->fb_location +
			RADEON_READ(RADEON_CONFIG_APER_SIZE);
	}

#if __OS_HAS_AGP
	if (dev_priv->flags & RADEON_IS_AGP)
		dev_priv->gart_buffers_offset = (dev->agp_buffer_map->offset
						 - dev->agp->base
						 + dev_priv->gart_vm_start);
	else
#endif
		dev_priv->gart_buffers_offset = (dev->agp_buffer_map->offset
					- (unsigned long)dev->sg->virtual
					+ dev_priv->gart_vm_start);

	DRM_DEBUG("dev_priv->gart_size %d\n", dev_priv->gart_size);
	DRM_DEBUG("dev_priv->gart_vm_start 0x%x\n", dev_priv->gart_vm_start);
	DRM_DEBUG("dev_priv->gart_buffers_offset 0x%lx\n",
		  dev_priv->gart_buffers_offset);

	dev_priv->ring.start = (u32 *) dev_priv->cp_ring->handle;
	dev_priv->ring.end = ((u32 *) dev_priv->cp_ring->handle
			      + init->ring_size / sizeof(u32));
	dev_priv->ring.size = init->ring_size;
	dev_priv->ring.size_l2qw = drm_order(init->ring_size / 8);

	dev_priv->ring.rptr_update = /* init->rptr_update */ 4096;
	dev_priv->ring.rptr_update_l2qw = drm_order( /* init->rptr_update */ 4096 / 8);

	dev_priv->ring.fetch_size = /* init->fetch_size */ 32;
	dev_priv->ring.fetch_size_l2ow = drm_order( /* init->fetch_size */ 32 / 16);

	dev_priv->ring.tail_mask = (dev_priv->ring.size / sizeof(u32)) - 1;

	dev_priv->ring.high_mark = RADEON_RING_HIGH_MARK;

#if __OS_HAS_AGP
	if (dev_priv->flags & RADEON_IS_AGP) {
		/* Turn off PCI GART */
		radeon_set_pcigart(dev_priv, 0);
	} else
#endif
	{
		dev_priv->gart_info.table_mask = DMA_BIT_MASK(32);
		/* if we have an offset set from userspace */
		if (dev_priv->pcigart_offset_set) {
			/* if it came from userspace - remap it */
			if (dev_priv->pcigart_offset_set == 1) {
				dev_priv->gart_info.bus_addr =
					dev_priv->pcigart_offset + dev_priv->fb_location;
				dev_priv->gart_info.mapping.offset =
					dev_priv->pcigart_offset + dev_priv->fb_aper_offset;
				dev_priv->gart_info.mapping.size =
					dev_priv->gart_info.table_size;
				
				/* this is done by the mm now */
				drm_core_ioremap(&dev_priv->gart_info.mapping, dev);
				dev_priv->gart_info.addr =
					dev_priv->gart_info.mapping.handle;
				
				memset(dev_priv->gart_info.addr, 0, dev_priv->gart_info.table_size);
				if (dev_priv->flags & RADEON_IS_PCIE)
					dev_priv->gart_info.gart_reg_if = DRM_ATI_GART_PCIE;
				else
					dev_priv->gart_info.gart_reg_if = DRM_ATI_GART_PCI;
				dev_priv->gart_info.gart_table_location =
					DRM_ATI_GART_FB;
				
				DRM_DEBUG("Setting phys_pci_gart to %p %08lX\n",
					  dev_priv->gart_info.addr,
					  dev_priv->pcigart_offset);
			}
		} else {

			if (dev_priv->flags & RADEON_IS_PCIE) {
				DRM_ERROR
				    ("Cannot use PCI Express without GART in FB memory\n");
				radeon_do_cleanup_cp(dev);
				return -EINVAL;
			}
			if (dev_priv->flags & RADEON_IS_IGPGART)
				dev_priv->gart_info.gart_reg_if = DRM_ATI_GART_IGP;
			else
				dev_priv->gart_info.gart_reg_if = DRM_ATI_GART_PCI;
			dev_priv->gart_info.gart_table_location =
			    DRM_ATI_GART_MAIN;
			dev_priv->gart_info.addr = NULL;
			dev_priv->gart_info.bus_addr = 0;

		}

		if (!drm_ati_pcigart_init(dev, &dev_priv->gart_info)) {
			DRM_ERROR("failed to init PCI GART!\n");
			radeon_do_cleanup_cp(dev);
			return -ENOMEM;
		}

		/* Turn on PCI GART */
		radeon_set_pcigart(dev_priv, 1);
	}

	/* Start with assuming that writeback doesn't work */
	dev_priv->writeback_works = 0;

	radeon_cp_load_microcode(dev_priv);
	radeon_cp_init_ring_buffer(dev, dev_priv);

	dev_priv->last_buf = 0;

	radeon_do_engine_reset(dev);
	radeon_test_writeback(dev_priv);

	return 0;
}

static int radeon_do_cleanup_cp(struct drm_device * dev)
{
	drm_radeon_private_t *dev_priv = dev->dev_private;
	DRM_DEBUG("\n");

	/* Make sure interrupts are disabled here because the uninstall ioctl
	 * may not have been called from userspace and after dev_private
	 * is freed, it's too late.
	 */
	if (dev->irq_enabled)
		drm_irq_uninstall(dev);

#if __OS_HAS_AGP
	if (dev_priv->flags & RADEON_IS_AGP) {
		if (dev_priv->cp_ring != NULL) {
			drm_core_ioremapfree(dev_priv->cp_ring, dev);
			dev_priv->cp_ring = NULL;
		}
		if (dev_priv->ring_rptr != NULL) {
			drm_core_ioremapfree(dev_priv->ring_rptr, dev);
			dev_priv->ring_rptr = NULL;
		}
		if (dev->agp_buffer_map != NULL) {
			drm_core_ioremapfree(dev->agp_buffer_map, dev);
			dev->agp_buffer_map = NULL;
		}
	} else
#endif
	{

		if (dev_priv->gart_info.bus_addr) {
			/* Turn off PCI GART */
			radeon_set_pcigart(dev_priv, 0);
			drm_ati_pcigart_cleanup(dev, &dev_priv->gart_info);
		}

		if (dev_priv->gart_info.gart_table_location == DRM_ATI_GART_FB)
		{
			if (dev_priv->pcigart_offset_set == 1) {
				drm_core_ioremapfree(&dev_priv->gart_info.mapping, dev);
				dev_priv->gart_info.addr = NULL;
				dev_priv->pcigart_offset_set = 0;
			}
		}
	}
	/* only clear to the start of flags */
	memset(dev_priv, 0, offsetof(drm_radeon_private_t, flags));

	return 0;
}

/* This code will reinit the Radeon CP hardware after a resume from disc.
 * AFAIK, it would be very difficult to pickle the state at suspend time, so
 * here we make sure that all Radeon hardware initialisation is re-done without
 * affecting running applications.
 *
 * Charl P. Botha <http://cpbotha.net>
 */
static int radeon_do_resume_cp(struct drm_device * dev)
{
	drm_radeon_private_t *dev_priv = dev->dev_private;

	if (!dev_priv) {
		DRM_ERROR("Called with no initialization\n");
		return -EINVAL;
	}

	DRM_DEBUG("Starting radeon_do_resume_cp()\n");

#if __OS_HAS_AGP
	if (dev_priv->flags & RADEON_IS_AGP) {
		/* Turn off PCI GART */
		radeon_set_pcigart(dev_priv, 0);
	} else
#endif
	{
		/* Turn on PCI GART */
		radeon_set_pcigart(dev_priv, 1);
	}

	radeon_cp_load_microcode(dev_priv);
	radeon_cp_init_ring_buffer(dev, dev_priv);

	radeon_do_engine_reset(dev);
	radeon_irq_set_state(dev, RADEON_SW_INT_ENABLE, 1);

	DRM_DEBUG("radeon_do_resume_cp() complete\n");

	return 0;
}

int radeon_cp_init(struct drm_device *dev, void *data, struct drm_file *file_priv)
{
	drm_radeon_init_t *init = data;
	
	/* on a modesetting driver ignore this stuff */
	if (drm_core_check_feature(dev, DRIVER_MODESET))
		return 0;

	LOCK_TEST_WITH_RETURN(dev, file_priv);

	if (init->func == RADEON_INIT_R300_CP)
		r300_init_reg_flags(dev);

	switch (init->func) {
	case RADEON_INIT_CP:
	case RADEON_INIT_R200_CP:
	case RADEON_INIT_R300_CP:
		return radeon_do_init_cp(dev, init, file_priv);
	case RADEON_CLEANUP_CP:
		return radeon_do_cleanup_cp(dev);
	}

	return -EINVAL;
}

int radeon_cp_start(struct drm_device *dev, void *data, struct drm_file *file_priv)
{
	drm_radeon_private_t *dev_priv = dev->dev_private;
	DRM_DEBUG("\n");

	if (drm_core_check_feature(dev, DRIVER_MODESET))
		return 0;

	LOCK_TEST_WITH_RETURN(dev, file_priv);

	if (dev_priv->cp_running) {
		DRM_DEBUG("while CP running\n");
		return 0;
	}
	if (dev_priv->cp_mode == RADEON_CSQ_PRIDIS_INDDIS) {
		DRM_DEBUG("called with bogus CP mode (%d)\n",
			  dev_priv->cp_mode);
		return 0;
	}

	radeon_do_cp_start(dev_priv);

	return 0;
}

/* Stop the CP.  The engine must have been idled before calling this
 * routine.
 */
int radeon_cp_stop(struct drm_device *dev, void *data, struct drm_file *file_priv)
{
	drm_radeon_private_t *dev_priv = dev->dev_private;
	drm_radeon_cp_stop_t *stop = data;
	int ret;
	DRM_DEBUG("\n");

	if (drm_core_check_feature(dev, DRIVER_MODESET))
		return 0;

	LOCK_TEST_WITH_RETURN(dev, file_priv);

	if (!dev_priv->cp_running)
		return 0;

	/* Flush any pending CP commands.  This ensures any outstanding
	 * commands are exectuted by the engine before we turn it off.
	 */
	if (stop->flush) {
		radeon_do_cp_flush(dev_priv);
	}

	/* If we fail to make the engine go idle, we return an error
	 * code so that the DRM ioctl wrapper can try again.
	 */
	if (stop->idle) {
		ret = radeon_do_cp_idle(dev_priv);
		if (ret)
			return ret;
	}

	/* Finally, we can turn off the CP.  If the engine isn't idle,
	 * we will get some dropped triangles as they won't be fully
	 * rendered before the CP is shut down.
	 */
	radeon_do_cp_stop(dev_priv);

	/* Reset the engine */
	radeon_do_engine_reset(dev);

	return 0;
}

void radeon_do_release(struct drm_device * dev)
{
	drm_radeon_private_t *dev_priv = dev->dev_private;
	int i, ret;

	if (drm_core_check_feature(dev, DRIVER_MODESET)) 
		return;
		
	if (dev_priv) {
		if (dev_priv->cp_running) {
			/* Stop the cp */
			while ((ret = radeon_do_cp_idle(dev_priv)) != 0) {
				DRM_DEBUG("radeon_do_cp_idle %d\n", ret);
#ifdef __linux__
				schedule();
#else
#if defined(__FreeBSD__) && __FreeBSD_version > 500000
				mtx_sleep(&ret, &dev->dev_lock, PZERO, "rdnrel",
				       1);
#else
				tsleep(&ret, PZERO, "rdnrel", 1);
#endif
#endif
			}
			radeon_do_cp_stop(dev_priv);
			radeon_do_engine_reset(dev);
		}

		/* Disable *all* interrupts */
		if (dev_priv->mmio)	/* remove this after permanent addmaps */
			RADEON_WRITE(RADEON_GEN_INT_CNTL, 0);

		if (dev_priv->mmio) {	/* remove all surfaces */
			for (i = 0; i < RADEON_MAX_SURFACES; i++) {
				RADEON_WRITE(RADEON_SURFACE0_INFO + 16 * i, 0);
				RADEON_WRITE(RADEON_SURFACE0_LOWER_BOUND +
					     16 * i, 0);
				RADEON_WRITE(RADEON_SURFACE0_UPPER_BOUND +
					     16 * i, 0);
			}
		}

		/* Free memory heap structures */
		radeon_mem_takedown(&(dev_priv->gart_heap));
		radeon_mem_takedown(&(dev_priv->fb_heap));

		if (dev_priv->user_mm_enable) {
			radeon_gem_mm_fini(dev);
			dev_priv->user_mm_enable = false;
		}

		/* deallocate kernel resources */
		radeon_do_cleanup_cp(dev);
	}
}

/* Just reset the CP ring.  Called as part of an X Server engine reset.
 */
int radeon_cp_reset(struct drm_device *dev, void *data, struct drm_file *file_priv)
{
	drm_radeon_private_t *dev_priv = dev->dev_private;
	DRM_DEBUG("\n");

	if (drm_core_check_feature(dev, DRIVER_MODESET)) 
		return 0;

	LOCK_TEST_WITH_RETURN(dev, file_priv);

	if (!dev_priv) {
		DRM_DEBUG("called before init done\n");
		return -EINVAL;
	}

	radeon_do_cp_reset(dev_priv);

	/* The CP is no longer running after an engine reset */
	dev_priv->cp_running = 0;

	return 0;
}

int radeon_cp_idle(struct drm_device *dev, void *data, struct drm_file *file_priv)
{
	drm_radeon_private_t *dev_priv = dev->dev_private;
	DRM_DEBUG("\n");

	
	if (!drm_core_check_feature(dev, DRIVER_MODESET))
		LOCK_TEST_WITH_RETURN(dev, file_priv);

	return radeon_do_cp_idle(dev_priv);
}

/* Added by Charl P. Botha to call radeon_do_resume_cp().
 */
int radeon_cp_resume(struct drm_device *dev, void *data, struct drm_file *file_priv)
{

	if (drm_core_check_feature(dev, DRIVER_MODESET)) 
		return 0;

	return radeon_do_resume_cp(dev);
}

int radeon_engine_reset(struct drm_device *dev, void *data, struct drm_file *file_priv)
{
	DRM_DEBUG("\n");

	if (drm_core_check_feature(dev, DRIVER_MODESET)) 
		return 0;

	LOCK_TEST_WITH_RETURN(dev, file_priv);

	return radeon_do_engine_reset(dev);
}

/* ================================================================
 * Fullscreen mode
 */

/* KW: Deprecated to say the least:
 */
int radeon_fullscreen(struct drm_device *dev, void *data, struct drm_file *file_priv)
{
	return 0;
}

/* ================================================================
 * Freelist management
 */

/* Original comment: FIXME: ROTATE_BUFS is a hack to cycle through
 *   bufs until freelist code is used.  Note this hides a problem with
 *   the scratch register * (used to keep track of last buffer
 *   completed) being written to before * the last buffer has actually
 *   completed rendering.
 *
 * KW:  It's also a good way to find free buffers quickly.
 *
 * KW: Ideally this loop wouldn't exist, and freelist_get wouldn't
 * sleep.  However, bugs in older versions of radeon_accel.c mean that
 * we essentially have to do this, else old clients will break.
 *
 * However, it does leave open a potential deadlock where all the
 * buffers are held by other clients, which can't release them because
 * they can't get the lock.
 */

struct drm_buf *radeon_freelist_get(struct drm_device * dev)
{
	struct drm_device_dma *dma = dev->dma;
	drm_radeon_private_t *dev_priv = dev->dev_private;
	drm_radeon_buf_priv_t *buf_priv;
	struct drm_buf *buf;
	int i, t;
	int start;

	if (++dev_priv->last_buf >= dma->buf_count)
		dev_priv->last_buf = 0;

	start = dev_priv->last_buf;

	for (t = 0; t < dev_priv->usec_timeout; t++) {
		u32 done_age = GET_SCRATCH(1);
		DRM_DEBUG("done_age = %d\n", done_age);
		for (i = start; i < dma->buf_count; i++) {
			buf = dma->buflist[i];
			buf_priv = buf->dev_private;
			if (buf->file_priv == NULL || (buf->pending &&
						       buf_priv->age <=
						       done_age)) {
				dev_priv->stats.requested_bufs++;
				buf->pending = 0;
				return buf;
			}
			start = 0;
		}

		if (t) {
			DRM_UDELAY(1);
			dev_priv->stats.freelist_loops++;
		}
	}

	DRM_DEBUG("returning NULL!\n");
	return NULL;
}

#if 0
struct drm_buf *radeon_freelist_get(struct drm_device * dev)
{
	struct drm_device_dma *dma = dev->dma;
	drm_radeon_private_t *dev_priv = dev->dev_private;
	drm_radeon_buf_priv_t *buf_priv;
	struct drm_buf *buf;
	int i, t;
	int start;
	u32 done_age = DRM_READ32(dev_priv->ring_rptr, RADEON_SCRATCHOFF(1));

	if (++dev_priv->last_buf >= dma->buf_count)
		dev_priv->last_buf = 0;

	start = dev_priv->last_buf;
	dev_priv->stats.freelist_loops++;

	for (t = 0; t < 2; t++) {
		for (i = start; i < dma->buf_count; i++) {
			buf = dma->buflist[i];
			buf_priv = buf->dev_private;
			if (buf->file_priv == 0 || (buf->pending &&
						    buf_priv->age <=
						    done_age)) {
				dev_priv->stats.requested_bufs++;
				buf->pending = 0;
				return buf;
			}
		}
		start = 0;
	}

	return NULL;
}
#endif

void radeon_freelist_reset(struct drm_device * dev)
{
	struct drm_device_dma *dma = dev->dma;
	drm_radeon_private_t *dev_priv = dev->dev_private;
	int i;

	dev_priv->last_buf = 0;
	for (i = 0; i < dma->buf_count; i++) {
		struct drm_buf *buf = dma->buflist[i];
		drm_radeon_buf_priv_t *buf_priv = buf->dev_private;
		buf_priv->age = 0;
	}
}

/* ================================================================
 * CP command submission
 */

int radeon_wait_ring(drm_radeon_private_t * dev_priv, int n)
{
	drm_radeon_ring_buffer_t *ring = &dev_priv->ring;
	int i;
	u32 last_head = GET_RING_HEAD(dev_priv);

	for (i = 0; i < dev_priv->usec_timeout; i++) {
		u32 head = GET_RING_HEAD(dev_priv);

		ring->space = (head - ring->tail) * sizeof(u32);
		if (ring->space <= 0)
			ring->space += ring->size;
		if (ring->space > n)
			return 0;

		dev_priv->stats.boxes |= RADEON_BOX_WAIT_IDLE;

		if (head != last_head)
			i = 0;
		last_head = head;

		DRM_UDELAY(1);
	}

	/* FIXME: This return value is ignored in the BEGIN_RING macro! */
#if RADEON_FIFO_DEBUG
	radeon_status(dev_priv);
	DRM_ERROR("failed!\n");
#endif
	return -EBUSY;
}

static int radeon_cp_get_buffers(struct drm_device *dev,
				 struct drm_file *file_priv,
				 struct drm_dma * d)
{
	int i;
	struct drm_buf *buf;

	for (i = d->granted_count; i < d->request_count; i++) {
		buf = radeon_freelist_get(dev);
		if (!buf)
			return -EBUSY;	/* NOTE: broken client */

		buf->file_priv = file_priv;

		if (DRM_COPY_TO_USER(&d->request_indices[i], &buf->idx,
				     sizeof(buf->idx)))
			return -EFAULT;
		if (DRM_COPY_TO_USER(&d->request_sizes[i], &buf->total,
				     sizeof(buf->total)))
			return -EFAULT;

		d->granted_count++;
	}
	return 0;
}

int radeon_cp_buffers(struct drm_device *dev, void *data, struct drm_file *file_priv)
{
	struct drm_device_dma *dma = dev->dma;
	int ret = 0;
	struct drm_dma *d = data;

	LOCK_TEST_WITH_RETURN(dev, file_priv);

	/* Please don't send us buffers.
	 */
	if (d->send_count != 0) {
		DRM_ERROR("Process %d trying to send %d buffers via drmDMA\n",
			  DRM_CURRENTPID, d->send_count);
		return -EINVAL;
	}

	/* We'll send you buffers.
	 */
	if (d->request_count < 0 || d->request_count > dma->buf_count) {
		DRM_ERROR("Process %d trying to get %d buffers (of %d max)\n",
			  DRM_CURRENTPID, d->request_count, dma->buf_count);
		return -EINVAL;
	}

	d->granted_count = 0;

	if (d->request_count) {
		ret = radeon_cp_get_buffers(dev, file_priv, d);
	}

	return ret;
}

static void radeon_get_vram_type(struct drm_device *dev)
{
	struct drm_radeon_private *dev_priv = dev->dev_private;
	uint32_t tmp;

	if (dev_priv->flags & RADEON_IS_IGP || (dev_priv->chip_family >= CHIP_R300))
		dev_priv->is_ddr = true;
	else if (RADEON_READ(RADEON_MEM_SDRAM_MODE_REG) & RADEON_MEM_CFG_TYPE_DDR)
		dev_priv->is_ddr = true;
	else
		dev_priv->is_ddr = false;

	if ((dev_priv->chip_family >= CHIP_R600) &&
	    (dev_priv->chip_family <= CHIP_RV635)) {
		int chansize;
		
		tmp = RADEON_READ(R600_RAMCFG);
		if (tmp & R600_CHANSIZE_OVERRIDE)
			chansize = 16;
		else if (tmp & R600_CHANSIZE)
			chansize = 64;
		else
			chansize = 32;

		if (dev_priv->chip_family == CHIP_R600)
			dev_priv->ram_width = 8 * chansize;
		else if (dev_priv->chip_family == CHIP_RV670)
			dev_priv->ram_width = 4 * chansize;
		else if ((dev_priv->chip_family == CHIP_RV610) ||
			 (dev_priv->chip_family == CHIP_RV620))
			dev_priv->ram_width = chansize;
		else if ((dev_priv->chip_family == CHIP_RV630) ||
			 (dev_priv->chip_family == CHIP_RV635))
			dev_priv->ram_width = 2 * chansize;
	} else if (dev_priv->chip_family == CHIP_RV515) {
		tmp = radeon_read_mc_reg(dev_priv, RV515_MC_CNTL);
		tmp &= RV515_MEM_NUM_CHANNELS_MASK;
		switch (tmp) {
		case 0: dev_priv->ram_width = 64; break;
		case 1: dev_priv->ram_width = 128; break;
		default: dev_priv->ram_width = 128; break;
		}
	} else if ((dev_priv->chip_family >= CHIP_R520) &&
		   (dev_priv->chip_family <= CHIP_RV570)) {
		tmp = radeon_read_mc_reg(dev_priv, R520_MC_CNTL0);
		switch ((tmp & R520_MEM_NUM_CHANNELS_MASK) >> R520_MEM_NUM_CHANNELS_SHIFT) {
		case 0: dev_priv->ram_width = 32; break;
		case 1: dev_priv->ram_width = 64; break;
		case 2: dev_priv->ram_width = 128; break;
		case 3: dev_priv->ram_width = 256; break;
		default: dev_priv->ram_width = 128; break;
		}
	} else if ((dev_priv->chip_family == CHIP_RV100) ||
		   (dev_priv->chip_family == CHIP_RS100) ||
		   (dev_priv->chip_family == CHIP_RS200)) {
		tmp = RADEON_READ(RADEON_MEM_CNTL);
		if (tmp & RV100_HALF_MODE)
			dev_priv->ram_width = 32;
		else
			dev_priv->ram_width = 64;

		if (dev_priv->flags & RADEON_SINGLE_CRTC) {
			dev_priv->ram_width /= 4;
			dev_priv->is_ddr = true;
		}
	} else if (dev_priv->chip_family <= CHIP_RV280) {
		tmp = RADEON_READ(RADEON_MEM_CNTL);
		if (tmp & RADEON_MEM_NUM_CHANNELS_MASK)
			dev_priv->ram_width = 128;
		else
			dev_priv->ram_width = 64;
	} else {
		/* newer IGPs */
		dev_priv->ram_width = 128;
	}
	DRM_DEBUG("RAM width %d bits %cDR\n", dev_priv->ram_width, dev_priv->is_ddr ? 'D' : 'S');
}   

static void radeon_force_some_clocks(struct drm_device *dev)
{
	struct drm_radeon_private *dev_priv = dev->dev_private;
	uint32_t tmp;

	tmp = RADEON_READ_PLL(dev_priv, RADEON_SCLK_CNTL);
	tmp |= RADEON_SCLK_FORCE_CP | RADEON_SCLK_FORCE_VIP;
	RADEON_WRITE_PLL(dev_priv, RADEON_SCLK_CNTL, tmp);
}

static void radeon_set_dynamic_clock(struct drm_device *dev, int mode)
{
	struct drm_radeon_private *dev_priv = dev->dev_private;
	uint32_t tmp;

	switch(mode) {
	case 0:
		if (dev_priv->flags & RADEON_SINGLE_CRTC) {
			tmp = RADEON_READ_PLL(dev_priv, RADEON_SCLK_CNTL);
			tmp |= (RADEON_SCLK_FORCE_CP   | RADEON_SCLK_FORCE_HDP |
				RADEON_SCLK_FORCE_DISP1 | RADEON_SCLK_FORCE_TOP |
				RADEON_SCLK_FORCE_E2   | RADEON_SCLK_FORCE_SE  |
				RADEON_SCLK_FORCE_IDCT | RADEON_SCLK_FORCE_VIP |
				RADEON_SCLK_FORCE_RE   | RADEON_SCLK_FORCE_PB  |
				RADEON_SCLK_FORCE_TAM  | RADEON_SCLK_FORCE_TDM |
				RADEON_SCLK_FORCE_RB);
			RADEON_WRITE_PLL(dev_priv, RADEON_SCLK_CNTL, tmp);
		} else if (dev_priv->chip_family == CHIP_RV350) {
			/* for RV350/M10, no delays are required. */
			tmp = RADEON_READ_PLL(dev_priv, R300_SCLK_CNTL2);
			tmp |= (R300_SCLK_FORCE_TCL |
				R300_SCLK_FORCE_GA |
				R300_SCLK_FORCE_CBA);
			RADEON_WRITE_PLL(dev_priv, R300_SCLK_CNTL2, tmp);

			tmp = RADEON_READ_PLL(dev_priv, RADEON_SCLK_CNTL);
			tmp &= ~(RADEON_SCLK_FORCE_DISP2 | RADEON_SCLK_FORCE_CP      |
				 RADEON_SCLK_FORCE_HDP   | RADEON_SCLK_FORCE_DISP1   |
				 RADEON_SCLK_FORCE_TOP   | RADEON_SCLK_FORCE_E2      |
				 R300_SCLK_FORCE_VAP     | RADEON_SCLK_FORCE_IDCT    |
				 RADEON_SCLK_FORCE_VIP   | R300_SCLK_FORCE_SR        |
				 R300_SCLK_FORCE_PX      | R300_SCLK_FORCE_TX        |
				 R300_SCLK_FORCE_US      | RADEON_SCLK_FORCE_TV_SCLK |
				 R300_SCLK_FORCE_SU      | RADEON_SCLK_FORCE_OV0);
			tmp |=  RADEON_DYN_STOP_LAT_MASK;
			RADEON_WRITE_PLL(dev_priv, RADEON_SCLK_CNTL, tmp);

			tmp = RADEON_READ_PLL(dev_priv, RADEON_SCLK_MORE_CNTL);
			tmp &= ~RADEON_SCLK_MORE_FORCEON;
			tmp |= RADEON_SCLK_MORE_MAX_DYN_STOP_LAT;
			RADEON_WRITE_PLL(dev_priv, RADEON_SCLK_MORE_CNTL, tmp);

			tmp = RADEON_READ_PLL(dev_priv, RADEON_VCLK_ECP_CNTL);
			tmp |= (RADEON_PIXCLK_ALWAYS_ONb |
				RADEON_PIXCLK_DAC_ALWAYS_ONb);
			RADEON_WRITE_PLL(dev_priv, RADEON_VCLK_ECP_CNTL, tmp);

			tmp = RADEON_READ_PLL(dev_priv, RADEON_PIXCLKS_CNTL);
			tmp |= (RADEON_PIX2CLK_ALWAYS_ONb         |
				RADEON_PIX2CLK_DAC_ALWAYS_ONb     |
				RADEON_DISP_TVOUT_PIXCLK_TV_ALWAYS_ONb |
				R300_DVOCLK_ALWAYS_ONb            |   
				RADEON_PIXCLK_BLEND_ALWAYS_ONb    |
				RADEON_PIXCLK_GV_ALWAYS_ONb       |
				R300_PIXCLK_DVO_ALWAYS_ONb        | 
				RADEON_PIXCLK_LVDS_ALWAYS_ONb     |
				RADEON_PIXCLK_TMDS_ALWAYS_ONb     |
				R300_PIXCLK_TRANS_ALWAYS_ONb      |
				R300_PIXCLK_TVO_ALWAYS_ONb        |
				R300_P2G2CLK_ALWAYS_ONb           |
				R300_P2G2CLK_ALWAYS_ONb);
			RADEON_WRITE_PLL(dev_priv, RADEON_PIXCLKS_CNTL, tmp);
		} else {
			tmp = RADEON_READ_PLL(dev_priv, RADEON_SCLK_CNTL);
			tmp |= (RADEON_SCLK_FORCE_CP | RADEON_SCLK_FORCE_E2);
			tmp |= RADEON_SCLK_FORCE_SE;

			if ( dev_priv->flags & RADEON_SINGLE_CRTC ) {
				tmp |= ( RADEON_SCLK_FORCE_RB    |
					 RADEON_SCLK_FORCE_TDM   |
					 RADEON_SCLK_FORCE_TAM   |
					 RADEON_SCLK_FORCE_PB    |
					 RADEON_SCLK_FORCE_RE    |
					 RADEON_SCLK_FORCE_VIP   |
					 RADEON_SCLK_FORCE_IDCT  |
					 RADEON_SCLK_FORCE_TOP   |
					 RADEON_SCLK_FORCE_DISP1 |
					 RADEON_SCLK_FORCE_DISP2 |
					 RADEON_SCLK_FORCE_HDP    );
			} else if ((dev_priv->chip_family == CHIP_R300) ||
				   (dev_priv->chip_family == CHIP_R350)) {
				tmp |= ( RADEON_SCLK_FORCE_HDP   |
					 RADEON_SCLK_FORCE_DISP1 |
					 RADEON_SCLK_FORCE_DISP2 |
					 RADEON_SCLK_FORCE_TOP   |
					 RADEON_SCLK_FORCE_IDCT  |
					 RADEON_SCLK_FORCE_VIP);
			}

			RADEON_WRITE_PLL(dev_priv, RADEON_SCLK_CNTL, tmp);

			udelay(16000);
			
			if ((dev_priv->chip_family == CHIP_R300) ||
			    (dev_priv->chip_family == CHIP_R350)) {
				tmp = RADEON_READ_PLL(dev_priv, R300_SCLK_CNTL2);
				tmp |= ( R300_SCLK_FORCE_TCL |
					 R300_SCLK_FORCE_GA  |
					 R300_SCLK_FORCE_CBA);
				RADEON_WRITE_PLL(dev_priv, R300_SCLK_CNTL2, tmp);
				udelay(16000);
			}
			
			if (dev_priv->flags & RADEON_IS_IGP) {
				tmp = RADEON_READ_PLL(dev_priv, RADEON_MCLK_CNTL);
				tmp &= ~(RADEON_FORCEON_MCLKA |
					 RADEON_FORCEON_YCLKA);
				RADEON_WRITE_PLL(dev_priv, RADEON_MCLK_CNTL, tmp);
				udelay(16000);
			}
			
			if ((dev_priv->chip_family == CHIP_RV200) ||
			    (dev_priv->chip_family == CHIP_RV250) ||
			    (dev_priv->chip_family == CHIP_RV280)) {
				tmp = RADEON_READ_PLL(dev_priv, RADEON_SCLK_MORE_CNTL);
				tmp |= RADEON_SCLK_MORE_FORCEON;
				RADEON_WRITE_PLL(dev_priv, RADEON_SCLK_MORE_CNTL, tmp);
				udelay(16000);
			}
			
			tmp = RADEON_READ_PLL(dev_priv, RADEON_PIXCLKS_CNTL);
			tmp &= ~(RADEON_PIX2CLK_ALWAYS_ONb         |
				 RADEON_PIX2CLK_DAC_ALWAYS_ONb     |
				 RADEON_PIXCLK_BLEND_ALWAYS_ONb    |
				 RADEON_PIXCLK_GV_ALWAYS_ONb       |
				 RADEON_PIXCLK_DIG_TMDS_ALWAYS_ONb |
				 RADEON_PIXCLK_LVDS_ALWAYS_ONb     |
				 RADEON_PIXCLK_TMDS_ALWAYS_ONb);
			
			RADEON_WRITE_PLL(dev_priv, RADEON_PIXCLKS_CNTL, tmp);
			udelay(16000);
			
			tmp = RADEON_READ_PLL(dev_priv, RADEON_VCLK_ECP_CNTL);
			tmp &= ~(RADEON_PIXCLK_ALWAYS_ONb  |
				 RADEON_PIXCLK_DAC_ALWAYS_ONb); 
			RADEON_WRITE_PLL(dev_priv, RADEON_VCLK_ECP_CNTL, tmp);
		}
		DRM_DEBUG("Dynamic Clock Scaling Disabled\n");
		break;
        case 1:
		if (dev_priv->flags & RADEON_SINGLE_CRTC) {
			tmp = RADEON_READ_PLL(dev_priv, RADEON_SCLK_CNTL);
			if ((RADEON_READ(RADEON_CONFIG_CNTL) & RADEON_CFG_ATI_REV_ID_MASK) >
			    RADEON_CFG_ATI_REV_A13) { 
				tmp &= ~(RADEON_SCLK_FORCE_CP | RADEON_SCLK_FORCE_RB);
			}
			tmp &= ~(RADEON_SCLK_FORCE_HDP  | RADEON_SCLK_FORCE_DISP1 |
				 RADEON_SCLK_FORCE_TOP  | RADEON_SCLK_FORCE_SE   |
				 RADEON_SCLK_FORCE_IDCT | RADEON_SCLK_FORCE_RE   |
				 RADEON_SCLK_FORCE_PB   | RADEON_SCLK_FORCE_TAM  |
				 RADEON_SCLK_FORCE_TDM);
			RADEON_WRITE_PLL(dev_priv, RADEON_SCLK_CNTL, tmp);
		} else if ((dev_priv->chip_family == CHIP_R300) ||
			   (dev_priv->chip_family == CHIP_R350) ||
			   (dev_priv->chip_family == CHIP_RV350)) {
			if (dev_priv->chip_family == CHIP_RV350) {
				tmp = RADEON_READ_PLL(dev_priv, R300_SCLK_CNTL2);
				tmp &= ~(R300_SCLK_FORCE_TCL |
					 R300_SCLK_FORCE_GA  |
					 R300_SCLK_FORCE_CBA);
				tmp |=  (R300_SCLK_TCL_MAX_DYN_STOP_LAT |
					 R300_SCLK_GA_MAX_DYN_STOP_LAT  |
					 R300_SCLK_CBA_MAX_DYN_STOP_LAT);
				RADEON_WRITE_PLL(dev_priv, R300_SCLK_CNTL2, tmp);
				
				tmp = RADEON_READ_PLL(dev_priv, RADEON_SCLK_CNTL);
				tmp &= ~(RADEON_SCLK_FORCE_DISP2 | RADEON_SCLK_FORCE_CP      |
					 RADEON_SCLK_FORCE_HDP   | RADEON_SCLK_FORCE_DISP1   |
					 RADEON_SCLK_FORCE_TOP   | RADEON_SCLK_FORCE_E2      |
					 R300_SCLK_FORCE_VAP     | RADEON_SCLK_FORCE_IDCT    |
					 RADEON_SCLK_FORCE_VIP   | R300_SCLK_FORCE_SR        |
					 R300_SCLK_FORCE_PX      | R300_SCLK_FORCE_TX        |
					 R300_SCLK_FORCE_US      | RADEON_SCLK_FORCE_TV_SCLK |
					 R300_SCLK_FORCE_SU      | RADEON_SCLK_FORCE_OV0);
				tmp |=  RADEON_DYN_STOP_LAT_MASK;
				RADEON_WRITE_PLL(dev_priv, RADEON_SCLK_CNTL, tmp);

				tmp = RADEON_READ_PLL(dev_priv, RADEON_SCLK_MORE_CNTL);
				tmp &= ~RADEON_SCLK_MORE_FORCEON;
				tmp |=  RADEON_SCLK_MORE_MAX_DYN_STOP_LAT;
				RADEON_WRITE_PLL(dev_priv, RADEON_SCLK_MORE_CNTL, tmp);
				
				tmp = RADEON_READ_PLL(dev_priv, RADEON_VCLK_ECP_CNTL);
				tmp |= (RADEON_PIXCLK_ALWAYS_ONb |
					RADEON_PIXCLK_DAC_ALWAYS_ONb);   
				RADEON_WRITE_PLL(dev_priv, RADEON_VCLK_ECP_CNTL, tmp);

				tmp = RADEON_READ_PLL(dev_priv, RADEON_PIXCLKS_CNTL);
				tmp |= (RADEON_PIX2CLK_ALWAYS_ONb         |
					RADEON_PIX2CLK_DAC_ALWAYS_ONb     |
					RADEON_DISP_TVOUT_PIXCLK_TV_ALWAYS_ONb |
					R300_DVOCLK_ALWAYS_ONb            |   
					RADEON_PIXCLK_BLEND_ALWAYS_ONb    |
					RADEON_PIXCLK_GV_ALWAYS_ONb       |
					R300_PIXCLK_DVO_ALWAYS_ONb        | 
					RADEON_PIXCLK_LVDS_ALWAYS_ONb     |
					RADEON_PIXCLK_TMDS_ALWAYS_ONb     |
					R300_PIXCLK_TRANS_ALWAYS_ONb      |
					R300_PIXCLK_TVO_ALWAYS_ONb        |
					R300_P2G2CLK_ALWAYS_ONb           |
					R300_P2G2CLK_ALWAYS_ONb);
				RADEON_WRITE_PLL(dev_priv, RADEON_PIXCLKS_CNTL, tmp);

				tmp = RADEON_READ_PLL(dev_priv, RADEON_MCLK_MISC);
				tmp |= (RADEON_MC_MCLK_DYN_ENABLE |
					RADEON_IO_MCLK_DYN_ENABLE);
				RADEON_WRITE_PLL(dev_priv, RADEON_MCLK_MISC, tmp);

				tmp = RADEON_READ_PLL(dev_priv, RADEON_MCLK_CNTL);
				tmp |= (RADEON_FORCEON_MCLKA |
					RADEON_FORCEON_MCLKB);

				tmp &= ~(RADEON_FORCEON_YCLKA  |
					 RADEON_FORCEON_YCLKB  |
					 RADEON_FORCEON_MC);

				/* Some releases of vbios have set DISABLE_MC_MCLKA
				   and DISABLE_MC_MCLKB bits in the vbios table.  Setting these
				   bits will cause H/W hang when reading video memory with dynamic clocking
				   enabled. */
				if ((tmp & R300_DISABLE_MC_MCLKA) &&
				    (tmp & R300_DISABLE_MC_MCLKB)) {
					/* If both bits are set, then check the active channels */
					tmp = RADEON_READ_PLL(dev_priv, RADEON_MCLK_CNTL);
					if (dev_priv->ram_width == 64) {
						if (RADEON_READ(RADEON_MEM_CNTL) & R300_MEM_USE_CD_CH_ONLY)
							tmp &= ~R300_DISABLE_MC_MCLKB;
						else
							tmp &= ~R300_DISABLE_MC_MCLKA;
					} else {
						tmp &= ~(R300_DISABLE_MC_MCLKA |
							 R300_DISABLE_MC_MCLKB);
					}
				}
				
				RADEON_WRITE_PLL(dev_priv, RADEON_MCLK_CNTL, tmp);
			} else {
				tmp = RADEON_READ_PLL(dev_priv, RADEON_SCLK_CNTL);
				tmp &= ~(R300_SCLK_FORCE_VAP);
				tmp |= RADEON_SCLK_FORCE_CP;
				RADEON_WRITE_PLL(dev_priv, RADEON_SCLK_CNTL, tmp);
				udelay(15000);
				
				tmp = RADEON_READ_PLL(dev_priv, R300_SCLK_CNTL2);
				tmp &= ~(R300_SCLK_FORCE_TCL |
					 R300_SCLK_FORCE_GA  |
					 R300_SCLK_FORCE_CBA);
				RADEON_WRITE_PLL(dev_priv, R300_SCLK_CNTL2, tmp);
			}
		} else {
			tmp = RADEON_READ_PLL(dev_priv, RADEON_CLK_PWRMGT_CNTL);
			tmp &= ~(RADEON_ACTIVE_HILO_LAT_MASK     | 
				 RADEON_DISP_DYN_STOP_LAT_MASK   | 
				 RADEON_DYN_STOP_MODE_MASK); 
			
			tmp |= (RADEON_ENGIN_DYNCLK_MODE |
				(0x01 << RADEON_ACTIVE_HILO_LAT_SHIFT));
			RADEON_WRITE_PLL(dev_priv, RADEON_CLK_PWRMGT_CNTL, tmp);
			udelay(15000);

			tmp = RADEON_READ_PLL(dev_priv, RADEON_CLK_PIN_CNTL);
			tmp |= RADEON_SCLK_DYN_START_CNTL; 
			RADEON_WRITE_PLL(dev_priv, RADEON_CLK_PIN_CNTL, tmp);
			udelay(15000);

			/* When DRI is enabled, setting DYN_STOP_LAT to zero can cause some R200 
			   to lockup randomly, leave them as set by BIOS.
			*/
			tmp = RADEON_READ_PLL(dev_priv, RADEON_SCLK_CNTL);
			/*tmp &= RADEON_SCLK_SRC_SEL_MASK;*/
			tmp &= ~RADEON_SCLK_FORCEON_MASK;

			/*RAGE_6::A11 A12 A12N1 A13, RV250::A11 A12, R300*/
			if (((dev_priv->chip_family == CHIP_RV250) &&
			     ((RADEON_READ(RADEON_CONFIG_CNTL) & RADEON_CFG_ATI_REV_ID_MASK) <
			      RADEON_CFG_ATI_REV_A13)) || 
			    ((dev_priv->chip_family == CHIP_RV100) &&
			     ((RADEON_READ(RADEON_CONFIG_CNTL) & RADEON_CFG_ATI_REV_ID_MASK) <=
			      RADEON_CFG_ATI_REV_A13))){
				tmp |= RADEON_SCLK_FORCE_CP;
				tmp |= RADEON_SCLK_FORCE_VIP;
			}
			
			RADEON_WRITE_PLL(dev_priv, RADEON_SCLK_CNTL, tmp);

			if ((dev_priv->chip_family == CHIP_RV200) ||
			    (dev_priv->chip_family == CHIP_RV250) ||
			    (dev_priv->chip_family == CHIP_RV280)) {
				tmp = RADEON_READ_PLL(dev_priv, RADEON_SCLK_MORE_CNTL);
				tmp &= ~RADEON_SCLK_MORE_FORCEON;

				/* RV200::A11 A12 RV250::A11 A12 */
				if (((dev_priv->chip_family == CHIP_RV200) ||
				     (dev_priv->chip_family == CHIP_RV250)) &&
				    ((RADEON_READ(RADEON_CONFIG_CNTL) & RADEON_CFG_ATI_REV_ID_MASK) <
				     RADEON_CFG_ATI_REV_A13)) {
					tmp |= RADEON_SCLK_MORE_FORCEON;
				}
				RADEON_WRITE_PLL(dev_priv, RADEON_SCLK_MORE_CNTL, tmp);
				udelay(15000);
			}
			
			/* RV200::A11 A12, RV250::A11 A12 */
			if (((dev_priv->chip_family == CHIP_RV200) ||
			     (dev_priv->chip_family == CHIP_RV250)) &&
			    ((RADEON_READ(RADEON_CONFIG_CNTL) & RADEON_CFG_ATI_REV_ID_MASK) <
			     RADEON_CFG_ATI_REV_A13)) {
				tmp = RADEON_READ_PLL(dev_priv, RADEON_PLL_PWRMGT_CNTL);
				tmp |= RADEON_TCL_BYPASS_DISABLE;
				RADEON_WRITE_PLL(dev_priv, RADEON_PLL_PWRMGT_CNTL, tmp);
			}
			udelay(15000);
			
			/*enable dynamic mode for display clocks (PIXCLK and PIX2CLK)*/
			tmp = RADEON_READ_PLL(dev_priv, RADEON_PIXCLKS_CNTL);
			tmp |=  (RADEON_PIX2CLK_ALWAYS_ONb         |
				 RADEON_PIX2CLK_DAC_ALWAYS_ONb     |
				 RADEON_PIXCLK_BLEND_ALWAYS_ONb    |
				 RADEON_PIXCLK_GV_ALWAYS_ONb       |
				 RADEON_PIXCLK_DIG_TMDS_ALWAYS_ONb |
				 RADEON_PIXCLK_LVDS_ALWAYS_ONb     |
				 RADEON_PIXCLK_TMDS_ALWAYS_ONb);
			
			RADEON_WRITE_PLL(dev_priv, RADEON_PIXCLKS_CNTL, tmp);
			udelay(15000);
			
			tmp = RADEON_READ_PLL(dev_priv, RADEON_VCLK_ECP_CNTL);
			tmp |= (RADEON_PIXCLK_ALWAYS_ONb  |
				RADEON_PIXCLK_DAC_ALWAYS_ONb); 
			
			RADEON_WRITE_PLL(dev_priv, RADEON_VCLK_ECP_CNTL, tmp);
			udelay(15000);
		}    
		DRM_DEBUG("Dynamic Clock Scaling Enabled\n");
		break;
        default:
		break;
	}
	
}

int radeon_modeset_cp_suspend(struct drm_device *dev)
{
	drm_radeon_private_t *dev_priv = dev->dev_private;
	int ret;

	ret = radeon_do_cp_idle(dev_priv);
	if (ret)
		DRM_ERROR("failed to idle CP on suspend\n");

	radeon_do_cp_stop(dev_priv);
	radeon_do_engine_reset(dev);
	if (dev_priv->flags & RADEON_IS_AGP) {
	} else {
		radeon_set_pcigart(dev_priv, 0);
	}
	
	return 0;
}

int radeon_modeset_cp_resume(struct drm_device *dev)
{
	drm_radeon_private_t *dev_priv = dev->dev_private;

	radeon_do_wait_for_idle(dev_priv);
#if __OS_HAS_AGP
	if (dev_priv->flags & RADEON_IS_AGP) {
		/* Turn off PCI GART */
		radeon_set_pcigart(dev_priv, 0);
	} else
#endif
	{
		/* Turn on PCI GART */
		radeon_set_pcigart(dev_priv, 1);
	}
	radeon_gart_flush(dev);

	radeon_cp_load_microcode(dev_priv);
	radeon_cp_init_ring_buffer(dev, dev_priv);

	radeon_do_engine_reset(dev);

	radeon_test_writeback(dev_priv);

	radeon_do_cp_start(dev_priv);
	return 0;
}

#if __OS_HAS_AGP
int radeon_modeset_agp_init(struct drm_device *dev)
{
	drm_radeon_private_t *dev_priv = dev->dev_private;
	struct drm_agp_mode mode;
	struct drm_agp_info info;
	int ret;
	int default_mode;
	uint32_t agp_status;
	bool is_v3;

	/* Acquire AGP. */
	ret = drm_agp_acquire(dev);
	if (ret) {
		DRM_ERROR("Unable to acquire AGP: %d\n", ret);
		return ret;
	}

	ret = drm_agp_info(dev, &info);
	if (ret) {
		DRM_ERROR("Unable to get AGP info: %d\n", ret);
		return ret;
 	}

	mode.mode = info.mode;

	agp_status = (RADEON_READ(RADEON_AGP_STATUS) | RADEON_AGPv3_MODE) & mode.mode;
	is_v3 = !!(agp_status & RADEON_AGPv3_MODE);

	if (is_v3) {
		default_mode = (agp_status & RADEON_AGPv3_8X_MODE) ? 8 : 4;
	} else {
		if (agp_status & RADEON_AGP_4X_MODE) default_mode = 4;
		else if (agp_status & RADEON_AGP_2X_MODE) default_mode = 2;
		else default_mode = 1;
	}

	if (radeon_agpmode > 0) {
		if ((radeon_agpmode < (is_v3 ? 4 : 1)) ||
		    (radeon_agpmode > (is_v3 ? 8 : 4)) ||
		    (radeon_agpmode & (radeon_agpmode - 1))) {
			DRM_ERROR("Illegal AGP Mode: %d (valid %s), leaving at %d\n",
				  radeon_agpmode, is_v3 ? "4, 8" : "1, 2, 4",
				  default_mode);
			radeon_agpmode = default_mode;
		}
		else
			DRM_INFO("AGP mode requested: %d\n", radeon_agpmode);
	} else
		radeon_agpmode = default_mode;

	mode.mode &= ~RADEON_AGP_MODE_MASK;
	if (is_v3) {
		switch(radeon_agpmode) {
		case 8:
			mode.mode |= RADEON_AGPv3_8X_MODE;
			break;
		case 4:
		default:
			mode.mode |= RADEON_AGPv3_4X_MODE;
			break;
		}
	} else {
		switch(radeon_agpmode) {
		case 4: mode.mode |= RADEON_AGP_4X_MODE;
		case 2: mode.mode |= RADEON_AGP_2X_MODE;
		case 1:
		default:
			mode.mode |= RADEON_AGP_1X_MODE;
			break;
		}
	}

	mode.mode &= ~RADEON_AGP_FW_MODE; /* disable fw */

	ret = drm_agp_enable(dev, mode);
	if (ret) {
		DRM_ERROR("Unable to enable AGP (mode = 0x%lx)\n", mode.mode);
		return ret;
	}

	/* workaround some hw issues */
	if (dev_priv->chip_family < CHIP_R200) {
		RADEON_WRITE(RADEON_AGP_CNTL, RADEON_READ(RADEON_AGP_CNTL) | 0x000e0000);
	}
	return 0;
}

void radeon_modeset_agp_destroy(struct drm_device *dev)
{
	if (dev->agp->acquired)
		drm_agp_release(dev);
}
#endif

int radeon_modeset_cp_init(struct drm_device *dev)
{
	drm_radeon_private_t *dev_priv = dev->dev_private;

	/* allocate a ring and ring rptr bits from GART space */
	/* these are allocated in GEM files */
	
	/* Start with assuming that writeback doesn't work */
	dev_priv->writeback_works = 0;

	if (dev_priv->chip_family > CHIP_R600)
		return 0;

	dev_priv->usec_timeout = RADEON_DEFAULT_CP_TIMEOUT;
	dev_priv->ring.size = RADEON_DEFAULT_RING_SIZE;
	dev_priv->cp_mode = RADEON_CSQ_PRIBM_INDBM;

	dev_priv->ring.start = (u32 *)(void *)(unsigned long)dev_priv->mm.ring.kmap.virtual;
	dev_priv->ring.end = (u32 *)(void *)(unsigned long)dev_priv->mm.ring.kmap.virtual +
		dev_priv->ring.size / sizeof(u32);
	dev_priv->ring.size_l2qw = drm_order(dev_priv->ring.size / 8);
	dev_priv->ring.rptr_update = 4096;
	dev_priv->ring.rptr_update_l2qw = drm_order(4096 / 8);
	dev_priv->ring.fetch_size = 32;
	dev_priv->ring.fetch_size_l2ow = drm_order(32 / 16);
	dev_priv->ring.tail_mask = (dev_priv->ring.size / sizeof(u32)) - 1;
	dev_priv->ring.high_mark = RADEON_RING_HIGH_MARK;

	dev_priv->new_memmap = true;

	r300_init_reg_flags(dev);

#if __OS_HAS_AGP
	if (dev_priv->flags & RADEON_IS_AGP)
		radeon_modeset_agp_init(dev);
#endif
	
	return radeon_modeset_cp_resume(dev);
}

static bool radeon_get_bios(struct drm_device *dev)
{
	drm_radeon_private_t *dev_priv = dev->dev_private;
	u8 __iomem *bios;
	size_t size;
	uint16_t tmp;

	bios = pci_map_rom(dev->pdev, &size);
	if (!bios)
		return -1;

	dev_priv->bios = kmalloc(size, GFP_KERNEL);
	if (!dev_priv->bios) {
		pci_unmap_rom(dev->pdev, bios);
		return -1;
	}

	memcpy(dev_priv->bios, bios, size);

	pci_unmap_rom(dev->pdev, bios);

	if (dev_priv->bios[0] != 0x55 || dev_priv->bios[1] != 0xaa)
		goto free_bios;

	dev_priv->bios_header_start = radeon_bios16(dev_priv, 0x48);

	if (!dev_priv->bios_header_start)
		goto free_bios;

	tmp = dev_priv->bios_header_start + 4;

	if (!memcmp(dev_priv->bios + tmp, "ATOM", 4) ||
	    !memcmp(dev_priv->bios + tmp, "MOTA", 4))
		dev_priv->is_atom_bios = true;
	else
		dev_priv->is_atom_bios = false;

	DRM_DEBUG("%sBIOS detected\n", dev_priv->is_atom_bios ? "ATOM" : "COM");
	return true;
free_bios:
	kfree(dev_priv->bios);
	dev_priv->bios = NULL;
	return false;
}

int radeon_modeset_preinit(struct drm_device *dev)
{
	drm_radeon_private_t *dev_priv = dev->dev_private;
	static struct card_info card;
	int ret;

	card.dev = dev;
	card.reg_read = cail_reg_read;
	card.reg_write = cail_reg_write;
	card.mc_read = cail_mc_read;
	card.mc_write = cail_mc_write;

	ret = radeon_get_bios(dev);
	if (!ret)
		return -1;

	if (dev_priv->is_atom_bios) {
		dev_priv->mode_info.atom_context = atom_parse(&card, dev_priv->bios);
		radeon_atom_initialize_bios_scratch_regs(dev);
	} else
		radeon_combios_initialize_bios_scratch_regs(dev);

	radeon_get_clock_info(dev);

	return 0;
}

int radeon_static_clocks_init(struct drm_device *dev)
{
	drm_radeon_private_t *dev_priv = dev->dev_private;

	if (dev_priv->chip_family == CHIP_RS400 ||
	    dev_priv->chip_family == CHIP_RS480)
		radeon_dynclks = 0;

	if ((dev_priv->flags & RADEON_IS_MOBILITY) && !radeon_is_avivo(dev_priv)) {
		radeon_set_dynamic_clock(dev, radeon_dynclks);
	} else if (radeon_is_avivo(dev_priv)) {
		if (radeon_dynclks) {
			radeon_atom_static_pwrmgt_setup(dev, 1);
			radeon_atom_dyn_clk_setup(dev, 1);
		}
	}
	radeon_force_some_clocks(dev);
	return 0;
}

int radeon_driver_load(struct drm_device *dev, unsigned long flags)
{
	drm_radeon_private_t *dev_priv;
	int ret = 0;

	dev_priv = drm_alloc(sizeof(drm_radeon_private_t), DRM_MEM_DRIVER);
	if (dev_priv == NULL)
		return -ENOMEM;

	memset(dev_priv, 0, sizeof(drm_radeon_private_t));
	dev->dev_private = (void *)dev_priv;
	dev_priv->flags = flags;

	switch (flags & RADEON_FAMILY_MASK) {
	case CHIP_R100:
	case CHIP_RV200:
	case CHIP_R200:
	case CHIP_R300:
	case CHIP_R350:
	case CHIP_R420:
	case CHIP_RV410:
	case CHIP_RV515:
	case CHIP_R520:
	case CHIP_RV570:
	case CHIP_R580:
		dev_priv->flags |= RADEON_HAS_HIERZ;
		break;
	default:
		/* all other chips have no hierarchical z buffer */
		break;
	}

	dev_priv->chip_family = flags & RADEON_FAMILY_MASK;
	if (drm_device_is_agp(dev))
		dev_priv->flags |= RADEON_IS_AGP;
	else if (drm_device_is_pcie(dev))
		dev_priv->flags |= RADEON_IS_PCIE;
	else
		dev_priv->flags |= RADEON_IS_PCI;

	DRM_DEBUG("%s card detected\n",
		  ((dev_priv->flags & RADEON_IS_AGP) ? "AGP" : (((dev_priv->flags & RADEON_IS_PCIE) ? "PCIE" : "PCI"))));

	if ((dev_priv->flags & RADEON_IS_AGP) && (radeon_agpmode == -1)) {
		DRM_INFO("Forcing AGP to PCI mode\n");
		dev_priv->flags &= ~RADEON_IS_AGP;
	}


	ret = drm_addmap(dev, drm_get_resource_start(dev, 2),
			 drm_get_resource_len(dev, 2), _DRM_REGISTERS,
			 _DRM_DRIVER | _DRM_READ_ONLY, &dev_priv->mmio);
	if (ret != 0)
		return ret;

	if (drm_core_check_feature(dev, DRIVER_MODESET))
		radeon_modeset_preinit(dev);

	radeon_get_vram_type(dev);

	dev_priv->pll_errata = 0;

	if (dev_priv->chip_family == CHIP_R300 &&
	    (RADEON_READ(RADEON_CONFIG_CNTL) & RADEON_CFG_ATI_REV_ID_MASK) == RADEON_CFG_ATI_REV_A11)
		dev_priv->pll_errata |= CHIP_ERRATA_R300_CG;

	if (dev_priv->chip_family == CHIP_RV200 ||
	    dev_priv->chip_family == CHIP_RS200)
		dev_priv->pll_errata |= CHIP_ERRATA_PLL_DUMMYREADS;


	if (dev_priv->chip_family == CHIP_RV100 ||
	    dev_priv->chip_family == CHIP_RS100 ||
	    dev_priv->chip_family == CHIP_RS200)
		dev_priv->pll_errata |= CHIP_ERRATA_PLL_DELAY;


	if (drm_core_check_feature(dev, DRIVER_MODESET))
		radeon_static_clocks_init(dev);
		
	/* init memory manager - start with all of VRAM and a 32MB GART aperture for now */
	dev_priv->fb_aper_offset = drm_get_resource_start(dev, 0);

	drm_bo_driver_init(dev);

	if (drm_core_check_feature(dev, DRIVER_MODESET)) {
	
		ret = radeon_gem_mm_init(dev);
 		if (ret)
 			goto modeset_fail;
		radeon_modeset_init(dev);

		radeon_modeset_cp_init(dev);
		dev->devname = kstrdup(DRIVER_NAME, GFP_KERNEL);

		drm_irq_install(dev);
	}


	return ret;
modeset_fail:
	dev->driver->driver_features &= ~DRIVER_MODESET;
	drm_put_minor(&dev->control);
	return ret;
}


int radeon_master_create(struct drm_device *dev, struct drm_master *master)
{
	struct drm_radeon_master_private *master_priv;
	unsigned long sareapage;
	int ret;

	master_priv = drm_calloc(1, sizeof(*master_priv), DRM_MEM_DRIVER);
	if (!master_priv)
		return -ENOMEM;

	/* prebuild the SAREA */
	sareapage = max(SAREA_MAX, PAGE_SIZE);
	ret = drm_addmap(dev, 0, sareapage, _DRM_SHM, _DRM_CONTAINS_LOCK|_DRM_DRIVER,
			 &master_priv->sarea);
	if (ret) {
		DRM_ERROR("SAREA setup failed\n");
		return ret;
	}
	master_priv->sarea_priv = master_priv->sarea->handle + sizeof(struct drm_sarea);
	master_priv->sarea_priv->pfCurrentPage = 0;

	master->driver_priv = master_priv;
	return 0;
}

void radeon_master_destroy(struct drm_device *dev, struct drm_master *master)
{
	struct drm_radeon_master_private *master_priv = master->driver_priv;

	if (!master_priv)
		return;

	if (master_priv->sarea_priv &&
	    master_priv->sarea_priv->pfCurrentPage != 0)
		radeon_cp_dispatch_flip(dev, master);

	master_priv->sarea_priv = NULL;
	if (master_priv->sarea)
		drm_rmmap_locked(dev, master_priv->sarea);
		
	drm_free(master_priv, sizeof(*master_priv), DRM_MEM_DRIVER);

	master->driver_priv = NULL;
}
/* Create mappings for registers and framebuffer so userland doesn't necessarily
 * have to find them.
 */
int radeon_driver_firstopen(struct drm_device *dev)
{
	drm_radeon_private_t *dev_priv = dev->dev_private;

	dev_priv->gart_info.table_size = RADEON_PCIGART_TABLE_SIZE;

	if (!drm_core_check_feature(dev, DRIVER_MODESET))
		radeon_gem_mm_init(dev);

	ret = drm_addmap(dev, dev_priv->fb_aper_offset,
			 drm_get_resource_len(dev, 0), _DRM_FRAME_BUFFER,
			 _DRM_WRITE_COMBINING, &map);
	if (ret != 0)
		return ret;

	return 0;
}

int radeon_driver_unload(struct drm_device *dev)
{
	drm_radeon_private_t *dev_priv = dev->dev_private;

	if (drm_core_check_feature(dev, DRIVER_MODESET)) {
		drm_irq_uninstall(dev);
		radeon_modeset_cleanup(dev);
		radeon_gem_mm_fini(dev);
#if __OS_HAS_AGP
		if (dev_priv->flags & RADEON_IS_AGP)
			radeon_modeset_agp_destroy(dev);
#endif
	}

	drm_bo_driver_finish(dev);
	drm_rmmap(dev, dev_priv->mmio);

	DRM_DEBUG("\n");
	drm_free(dev_priv, sizeof(*dev_priv), DRM_MEM_DRIVER);

	dev->dev_private = NULL;
	return 0;
}

void radeon_gart_flush(struct drm_device *dev)
{
        drm_radeon_private_t *dev_priv = dev->dev_private;
        
        if (dev_priv->flags & RADEON_IS_IGPGART) {
                IGP_READ_MCIND(dev_priv, RS480_GART_CACHE_CNTRL);
                IGP_WRITE_MCIND(RS480_GART_CACHE_CNTRL, RS480_GART_CACHE_INVALIDATE);
                IGP_READ_MCIND(dev_priv, RS480_GART_CACHE_CNTRL);
                IGP_WRITE_MCIND(RS480_GART_CACHE_CNTRL, 0);
        } else if (dev_priv->flags & RADEON_IS_PCIE) {
                u32 tmp = RADEON_READ_PCIE(dev_priv, RADEON_PCIE_TX_GART_CNTL);
                tmp |= RADEON_PCIE_TX_GART_INVALIDATE_TLB;
                RADEON_WRITE_PCIE(RADEON_PCIE_TX_GART_CNTL, tmp);
                tmp &= ~RADEON_PCIE_TX_GART_INVALIDATE_TLB;
                RADEON_WRITE_PCIE(RADEON_PCIE_TX_GART_CNTL, tmp);
        } else {


	}
	
}
