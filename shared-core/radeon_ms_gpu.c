/*
 * Copyright 2007 Jérôme Glisse
 * Copyright 2007 Alex Deucher
 * Copyright 2007 Dave Airlie
 * All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation on the rights to use, copy, modify, merge,
 * publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial
 * portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NON-INFRINGEMENT.  IN NO EVENT SHALL ATI, VA LINUX SYSTEMS AND/OR
 * THEIR SUPPLIERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */
#include "radeon_ms.h"

static int radeon_ms_gpu_address_space_init(struct drm_device *dev)
{
	struct drm_radeon_private *dev_priv = dev->dev_private;
	struct radeon_state *state = &dev_priv->driver_state;

	/* initialize gpu mapping */
	dev_priv->gpu_vram_start = dev_priv->vram.offset;
	dev_priv->gpu_vram_end = dev_priv->gpu_vram_start + dev_priv->vram.size;
	/* align it on 16Mo boundary (clamp memory which is then
	 * unreachable but not manufacturer should use strange
	 * memory size */
	dev_priv->gpu_vram_end = dev_priv->gpu_vram_end & (~0xFFFFFF);
	dev_priv->gpu_vram_end -= 1;
	dev_priv->gpu_vram_size = dev_priv->gpu_vram_end -
				  dev_priv->gpu_vram_start + 1;
	/* set gart size to 32Mo FIXME: should make a parameter for this */
	dev_priv->gpu_gart_size = 1024 * 1024 * 32;
	if (dev_priv->gpu_gart_size > (0xffffffffU - dev_priv->gpu_vram_end)) {
		/* align gart start to next 4Ko in gpu address space */
		dev_priv->gpu_gart_start = (dev_priv->gpu_vram_end + 1) + 0xfff;
		dev_priv->gpu_gart_start = dev_priv->gpu_gart_start & (~0xfff);
		dev_priv->gpu_gart_end = dev_priv->gpu_gart_start +
					 dev_priv->gpu_gart_size;
		dev_priv->gpu_gart_end = (dev_priv->gpu_gart_end & (~0xfff)) -
					 0x1000;
	} else {
		/* align gart start to next 4Ko in gpu address space */
		dev_priv->gpu_gart_start = (dev_priv->gpu_vram_start & ~0xfff) -
					   dev_priv->gpu_gart_size;
		dev_priv->gpu_gart_start = dev_priv->gpu_gart_start & (~0xfff);
		dev_priv->gpu_gart_end = dev_priv->gpu_gart_start +
					 dev_priv->gpu_gart_size;
		dev_priv->gpu_gart_end = (dev_priv->gpu_gart_end & (~0xfff)) -
					 0x1000;
	}
	state->mc_fb_location =
		REG_S(MC_FB_LOCATION, MC_FB_START,
				dev_priv->gpu_vram_start >> 16) |
		REG_S(MC_FB_LOCATION, MC_FB_TOP, dev_priv->gpu_vram_end >> 16);
	state->display_base_addr =
		REG_S(DISPLAY_BASE_ADDR, DISPLAY_BASE_ADDR,
				dev_priv->gpu_vram_start);
	state->config_aper_0_base = dev_priv->gpu_vram_start;
	state->config_aper_1_base = dev_priv->gpu_vram_start;
	state->config_aper_size = dev_priv->gpu_vram_size;
	DRM_INFO("[radeon_ms] gpu vram start 0x%08X\n",
		 dev_priv->gpu_vram_start);
	DRM_INFO("[radeon_ms] gpu vram end   0x%08X\n",
		 dev_priv->gpu_vram_end);
	DRM_INFO("[radeon_ms] gpu gart start 0x%08X\n",
		 dev_priv->gpu_gart_start);
	DRM_INFO("[radeon_ms] gpu gart end   0x%08X\n",
		 dev_priv->gpu_gart_end);
	return 0;
}

static void radeon_ms_gpu_reset(struct drm_device *dev)
{
	struct drm_radeon_private *dev_priv = dev->dev_private;
	uint32_t clock_cntl_index, mclk_cntl, rbbm_soft_reset;
	uint32_t reset_mask, host_path_cntl, cache_mode;

	radeon_ms_cp_stop(dev);
	radeon_ms_gpu_flush(dev);

	/* reset clock */
	clock_cntl_index = MMIO_R(CLOCK_CNTL_INDEX);
	pll_index_errata(dev_priv);
	mclk_cntl = PPLL_R(MCLK_CNTL);
	PPLL_W(MCLK_CNTL,
			mclk_cntl |
			MCLK_CNTL__FORCE_MCLKA |
			MCLK_CNTL__FORCE_MCLKB |
			MCLK_CNTL__FORCE_YCLKA |
			MCLK_CNTL__FORCE_YCLKB |
			MCLK_CNTL__FORCE_MC |
			MCLK_CNTL__FORCE_AIC);
	PPLL_W(SCLK_CNTL,
			PPLL_R(SCLK_CNTL) |
			SCLK_CNTL__FORCE_CP |
			SCLK_CNTL__FORCE_VIP);

	/* Soft resetting HDP thru RBBM_SOFT_RESET register can cause some
	 * unexpected behaviour on some machines.  Here we use
	 * RADEON_HOST_PATH_CNTL to reset it.
	 */
	host_path_cntl = MMIO_R(HOST_PATH_CNTL);
	rbbm_soft_reset = MMIO_R(RBBM_SOFT_RESET);
	reset_mask = RBBM_SOFT_RESET__SOFT_RESET_CP |
		RBBM_SOFT_RESET__SOFT_RESET_HI |
		RBBM_SOFT_RESET__SOFT_RESET_VAP |
		RBBM_SOFT_RESET__SOFT_RESET_SE |
		RBBM_SOFT_RESET__SOFT_RESET_RE |
		RBBM_SOFT_RESET__SOFT_RESET_PP |
		RBBM_SOFT_RESET__SOFT_RESET_E2 |
		RBBM_SOFT_RESET__SOFT_RESET_RB;
	MMIO_W(RBBM_SOFT_RESET, rbbm_soft_reset | reset_mask);
	MMIO_R(RBBM_SOFT_RESET);
	MMIO_W(RBBM_SOFT_RESET, 0);
	MMIO_R(RBBM_SOFT_RESET);

#if 0
	cache_mode = MMIO_R(RB2D_DSTCACHE_MODE);
	MMIO_W(RB2D_DSTCACHE_MODE,
			cache_mode | RB2D_DSTCACHE_MODE__DC_DISABLE_IGNORE_PE);
#else
	reset_mask = RBBM_SOFT_RESET__SOFT_RESET_CP |
		RBBM_SOFT_RESET__SOFT_RESET_HI |
		RBBM_SOFT_RESET__SOFT_RESET_E2;
	MMIO_W(RBBM_SOFT_RESET, rbbm_soft_reset | reset_mask);
	MMIO_R(RBBM_SOFT_RESET);
	MMIO_W(RBBM_SOFT_RESET, 0);
	cache_mode = MMIO_R(RB3D_DSTCACHE_CTLSTAT);
	MMIO_W(RB3D_DSTCACHE_CTLSTAT, cache_mode | (0xf));
#endif

	MMIO_W(HOST_PATH_CNTL, host_path_cntl | HOST_PATH_CNTL__HDP_SOFT_RESET);
	MMIO_R(HOST_PATH_CNTL);
	MMIO_W(HOST_PATH_CNTL, host_path_cntl);
	MMIO_R(HOST_PATH_CNTL);

	MMIO_W(CLOCK_CNTL_INDEX, clock_cntl_index);
	pll_index_errata(dev_priv);
	PPLL_W(MCLK_CNTL, mclk_cntl);
}

static void radeon_ms_gpu_resume(struct drm_device *dev)
{
	struct drm_radeon_private *dev_priv = dev->dev_private;
	uint32_t a;
	uint32_t i;

	/* make sure we have sane offset before restoring crtc */
	a = (MMIO_R(MC_FB_LOCATION) & MC_FB_LOCATION__MC_FB_START__MASK) << 16;
	MMIO_W(DISPLAY_BASE_ADDR, a);
	MMIO_W(CRTC2_DISPLAY_BASE_ADDR, a);
	MMIO_W(CRTC_OFFSET, 0);
	MMIO_W(CUR_OFFSET, 0);
	MMIO_W(CRTC_OFFSET_CNTL, CRTC_OFFSET_CNTL__CRTC_OFFSET_FLIP_CNTL);
	MMIO_W(CRTC2_OFFSET, 0);
	MMIO_W(CUR2_OFFSET, 0);
	MMIO_W(CRTC2_OFFSET_CNTL, CRTC2_OFFSET_CNTL__CRTC2_OFFSET_FLIP_CNTL);
	for (i = 0; i < dev_priv->usec_timeout; i++) {
		if (!(CRTC_OFFSET__CRTC_GUI_TRIG_OFFSET &
		      MMIO_R(CRTC_OFFSET))) {
			break;
		}
		DRM_UDELAY(1);
	}
	if (i >= dev_priv->usec_timeout) {
		DRM_INFO("[radeon_ms] timeout waiting for crtc...\n");
	}
	for (i = 0; i < dev_priv->usec_timeout; i++) {
		if (!(CRTC2_OFFSET__CRTC2_GUI_TRIG_OFFSET &
		      MMIO_R(CRTC2_OFFSET))) {
			break;
		}
		DRM_UDELAY(1);
	}
	if (i >= dev_priv->usec_timeout) {
		DRM_INFO("[radeon_ms] timeout waiting for crtc...\n");
	}
	DRM_UDELAY(10000);
}

static void radeon_ms_gpu_stop(struct drm_device *dev)
{
	struct drm_radeon_private *dev_priv = dev->dev_private;
	uint32_t ov0_scale_cntl, crtc_ext_cntl, crtc_gen_cntl;
	uint32_t crtc2_gen_cntl, i;

	/* Capture MC_STATUS in case things go wrong ... */
	ov0_scale_cntl = dev_priv->ov0_scale_cntl = MMIO_R(OV0_SCALE_CNTL);
	crtc_ext_cntl = dev_priv->crtc_ext_cntl = MMIO_R(CRTC_EXT_CNTL);
	crtc_gen_cntl = dev_priv->crtc_gen_cntl = MMIO_R(CRTC_GEN_CNTL);
	crtc2_gen_cntl = dev_priv->crtc2_gen_cntl = MMIO_R(CRTC2_GEN_CNTL);
	ov0_scale_cntl &= ~OV0_SCALE_CNTL__OV0_OVERLAY_EN__MASK;
	crtc_ext_cntl |= CRTC_EXT_CNTL__CRTC_DISPLAY_DIS;
	crtc_gen_cntl &= ~CRTC_GEN_CNTL__CRTC_CUR_EN;
	crtc_gen_cntl &= ~CRTC_GEN_CNTL__CRTC_ICON_EN;
	crtc_gen_cntl |= CRTC_GEN_CNTL__CRTC_EXT_DISP_EN;
	crtc_gen_cntl |= CRTC_GEN_CNTL__CRTC_DISP_REQ_EN_B;
	crtc2_gen_cntl &= ~CRTC2_GEN_CNTL__CRTC2_CUR_EN;
	crtc2_gen_cntl &= ~CRTC2_GEN_CNTL__CRTC2_ICON_EN;
	crtc2_gen_cntl |= CRTC2_GEN_CNTL__CRTC2_DISP_REQ_EN_B;
	MMIO_W(OV0_SCALE_CNTL, ov0_scale_cntl);
	MMIO_W(CRTC_EXT_CNTL, crtc_ext_cntl);
	MMIO_W(CRTC_GEN_CNTL, crtc_gen_cntl);
	MMIO_W(CRTC2_GEN_CNTL, crtc2_gen_cntl);
	DRM_UDELAY(10000);
	switch (dev_priv->family) {
	case CHIP_R100:
	case CHIP_R200:
	case CHIP_RV200:
	case CHIP_RV250:
	case CHIP_RV280:
	case CHIP_RS300:
		for (i = 0; i < dev_priv->usec_timeout; i++) {
			if ((MC_STATUS__MC_IDLE & MMIO_R(MC_STATUS))) {
				DRM_INFO("[radeon_ms] gpu stoped in %d usecs\n",
					 i);
				return;
			}
			DRM_UDELAY(1);
		}
		break;
	case CHIP_R300:
	case CHIP_R350:
	case CHIP_R360:
	case CHIP_RV350:
	case CHIP_RV370:
	case CHIP_RV380:
	case CHIP_RS400:
	case CHIP_RV410:
	case CHIP_R420:
	case CHIP_R430:
	case CHIP_R480:
		for (i = 0; i < dev_priv->usec_timeout; i++) {
			if ((MC_STATUS__MC_IDLE_R3 & MMIO_R(MC_STATUS))) {
				DRM_INFO("[radeon_ms] gpu stoped in %d usecs\n",
					 i);
				return;
			}
			DRM_UDELAY(1);
		}
		break;
	default:
		DRM_INFO("Unknown radeon family, aborting\n");
		return;
	}
	DRM_INFO("[radeon_ms] failed to stop gpu...will proceed anyway\n");
	DRM_UDELAY(20000);
}

static int radeon_ms_wait_for_fifo(struct drm_device *dev, int num_fifo)
{
	struct drm_radeon_private *dev_priv = dev->dev_private;
	int i;

	for (i = 0; i < dev_priv->usec_timeout; i++) {
		int t;
		t = RBBM_STATUS__CMDFIFO_AVAIL__MASK & MMIO_R(RBBM_STATUS);
		t = t >> RBBM_STATUS__CMDFIFO_AVAIL__SHIFT;
		if (t >= num_fifo)
			return 0;
		DRM_UDELAY(1);
	}
	DRM_INFO("[radeon_ms] failed to wait for fifo\n");
	return -EBUSY;
}

int radeon_ms_gpu_initialize(struct drm_device *dev)
{
	struct drm_radeon_private *dev_priv = dev->dev_private;
	struct radeon_state *state = &dev_priv->driver_state;
	int ret;

	state->disp_misc_cntl = DISP_MISC_CNTL__SYNC_PAD_FLOP_EN |
		REG_S(DISP_MISC_CNTL, SYNC_STRENGTH, 2) |
		REG_S(DISP_MISC_CNTL, PALETTE_MEM_RD_MARGIN, 0xb) |
		REG_S(DISP_MISC_CNTL, PALETTE2_MEM_RD_MARGIN, 0xb) |
		REG_S(DISP_MISC_CNTL, RMX_BUF_MEM_RD_MARGIN, 0x5);
	state->disp_merge_cntl = REG_S(DISP_MERGE_CNTL, DISP_GRPH_ALPHA, 0xff) |
		REG_S(DISP_MERGE_CNTL, DISP_OV0_ALPHA, 0xff);
	state->disp_pwr_man = DISP_PWR_MAN__DISP_PWR_MAN_D3_CRTC_EN |
		DISP_PWR_MAN__DISP2_PWR_MAN_D3_CRTC2_EN |
		REG_S(DISP_PWR_MAN, DISP_PWR_MAN_DPMS, DISP_PWR_MAN_DPMS__OFF) |
		DISP_PWR_MAN__DISP_D3_RST |
		DISP_PWR_MAN__DISP_D3_REG_RST |
		DISP_PWR_MAN__DISP_D3_GRPH_RST |
		DISP_PWR_MAN__DISP_D3_SUBPIC_RST |
		DISP_PWR_MAN__DISP_D3_OV0_RST |
		DISP_PWR_MAN__DISP_D1D2_GRPH_RST |
		DISP_PWR_MAN__DISP_D1D2_SUBPIC_RST |
		DISP_PWR_MAN__DISP_D1D2_OV0_RST |
		DISP_PWR_MAN__DISP_DVO_ENABLE_RST |
		DISP_PWR_MAN__TV_ENABLE_RST;
	state->disp2_merge_cntl = 0;
	ret = radeon_ms_gpu_address_space_init(dev);
	if (ret) {
		return ret;
	}

	/* initialize bus */
	ret = dev_priv->bus_init(dev);
	if (ret != 0) {
		return ret;
	}
	return 0;
}

void radeon_ms_gpu_dpms(struct drm_device *dev)
{
	struct drm_radeon_private *dev_priv = dev->dev_private;
	struct radeon_state *state = &dev_priv->driver_state;

	if (dev_priv->crtc1_dpms == dev_priv->crtc2_dpms) {
		/* both crtc are in same state so use global display pwr */
		state->disp_pwr_man &= ~DISP_PWR_MAN__DISP_PWR_MAN_DPMS__MASK;
		switch(dev_priv->crtc1_dpms) {
		case DPMSModeOn:
			state->disp_pwr_man |= REG_S(DISP_PWR_MAN,
					DISP_PWR_MAN_DPMS,
					DISP_PWR_MAN_DPMS__ON);
			break;
		case DPMSModeStandby:
			state->disp_pwr_man |= REG_S(DISP_PWR_MAN,
					DISP_PWR_MAN_DPMS,
					DISP_PWR_MAN_DPMS__STANDBY);
			break;
		case DPMSModeSuspend:
			state->disp_pwr_man |= REG_S(DISP_PWR_MAN,
					DISP_PWR_MAN_DPMS,
					DISP_PWR_MAN_DPMS__SUSPEND);
			break;
		case DPMSModeOff:
			state->disp_pwr_man |= REG_S(DISP_PWR_MAN,
					DISP_PWR_MAN_DPMS,
					DISP_PWR_MAN_DPMS__OFF);
			break;
		default:
			/* error */
			break;
		}
		MMIO_W(DISP_PWR_MAN, state->disp_pwr_man);
	} else {
		state->disp_pwr_man &= ~DISP_PWR_MAN__DISP_PWR_MAN_DPMS__MASK;
		state->disp_pwr_man |= REG_S(DISP_PWR_MAN,
				DISP_PWR_MAN_DPMS,
				DISP_PWR_MAN_DPMS__ON);
		MMIO_W(DISP_PWR_MAN, state->disp_pwr_man);
	}
}

void radeon_ms_gpu_flush(struct drm_device *dev)
{
	struct drm_radeon_private *dev_priv = dev->dev_private;
	uint32_t i;
	uint32_t purge2d;
	uint32_t purge3d;

	switch (dev_priv->family) {
	case CHIP_R100:
	case CHIP_R200:
	case CHIP_RV200:
	case CHIP_RV250:
	case CHIP_RV280:
	case CHIP_RS300:
		purge2d = REG_S(RB2D_DSTCACHE_CTLSTAT, DC_FLUSH, 3) |
			REG_S(RB2D_DSTCACHE_CTLSTAT, DC_FREE, 3);
		purge3d = REG_S(RB3D_DSTCACHE_CTLSTAT, DC_FLUSH, 3) |
			REG_S(RB3D_DSTCACHE_CTLSTAT, DC_FREE, 3);
		MMIO_W(RB2D_DSTCACHE_CTLSTAT, purge2d);
		MMIO_W(RB3D_DSTCACHE_CTLSTAT, purge3d);
		break;
	case CHIP_R300:
	case CHIP_R350:
	case CHIP_R360:
	case CHIP_RV350:
	case CHIP_RV370:
	case CHIP_RV380:
	case CHIP_RS400:
	case CHIP_RV410:
	case CHIP_R420:
	case CHIP_R430:
	case CHIP_R480:
		purge2d = REG_S(RB2D_DSTCACHE_CTLSTAT, DC_FLUSH, 3) |
			REG_S(RB2D_DSTCACHE_CTLSTAT, DC_FREE, 3);
		purge3d = REG_S(RB3D_DSTCACHE_CTLSTAT_R3, DC_FLUSH, 3) |
			REG_S(RB3D_DSTCACHE_CTLSTAT_R3, DC_FREE, 3);
		MMIO_W(RB2D_DSTCACHE_CTLSTAT, purge2d);
		MMIO_W(RB3D_DSTCACHE_CTLSTAT_R3, purge3d);
		break;
	default:
		DRM_INFO("Unknown radeon family, aborting\n");
		return;
	}
	for (i = 0; i < dev_priv->usec_timeout; i++) {
		if (!(RB2D_DSTCACHE_CTLSTAT__DC_BUSY &
					MMIO_R(RB2D_DSTCACHE_CTLSTAT))) {
			return;
		}
		DRM_UDELAY(1);
	}
	DRM_INFO("[radeon_ms] gpu flush timeout\n");
}

void radeon_ms_gpu_restore(struct drm_device *dev, struct radeon_state *state)
{
	struct drm_radeon_private *dev_priv = dev->dev_private;
	uint32_t wait_until;
	uint32_t fbstart;
	int ret, ok = 1;

	radeon_ms_gpu_reset(dev);
	radeon_ms_wait_for_idle(dev);
	radeon_ms_gpu_stop(dev);

	MMIO_W(AIC_CTRL, state->aic_ctrl);
	MMIO_W(MC_FB_LOCATION, state->mc_fb_location);
	MMIO_R(MC_FB_LOCATION);
	MMIO_W(CONFIG_APER_0_BASE, state->config_aper_0_base);
	MMIO_W(CONFIG_APER_1_BASE, state->config_aper_1_base);
	MMIO_W(CONFIG_APER_SIZE, state->config_aper_size);
	MMIO_W(DISPLAY_BASE_ADDR, state->display_base_addr);
	if (dev_priv->bus_restore) {
		dev_priv->bus_restore(dev, state);
	}

	radeon_ms_gpu_reset(dev);
	radeon_ms_gpu_resume(dev);

	MMIO_W(BUS_CNTL, state->bus_cntl);
	wait_until = WAIT_UNTIL__WAIT_DMA_VIPH0_IDLE |
		WAIT_UNTIL__WAIT_DMA_VIPH1_IDLE |
		WAIT_UNTIL__WAIT_DMA_VIPH2_IDLE |
		WAIT_UNTIL__WAIT_DMA_VIPH3_IDLE |
		WAIT_UNTIL__WAIT_DMA_VID_IDLE |
		WAIT_UNTIL__WAIT_DMA_GUI_IDLE |
		WAIT_UNTIL__WAIT_2D_IDLE |
		WAIT_UNTIL__WAIT_3D_IDLE |
		WAIT_UNTIL__WAIT_2D_IDLECLEAN |
		WAIT_UNTIL__WAIT_3D_IDLECLEAN |
		WAIT_UNTIL__WAIT_HOST_IDLECLEAN;
	switch (dev_priv->family) {
	case CHIP_R100:
	case CHIP_R200:
	case CHIP_RV200:
	case CHIP_RV250:
	case CHIP_RV280:
	case CHIP_RS300:
		break;
	case CHIP_R300:
	case CHIP_R350:
	case CHIP_R360:
	case CHIP_RV350:
	case CHIP_RV370:
	case CHIP_RV380:
	case CHIP_RS400:
	case CHIP_RV410:
	case CHIP_R420:
	case CHIP_R430:
	case CHIP_R480:
		wait_until |= WAIT_UNTIL__WAIT_VAP_IDLE;
		break;
	}
	MMIO_W(WAIT_UNTIL, wait_until); 
	MMIO_W(DISP_MISC_CNTL, state->disp_misc_cntl);
	MMIO_W(DISP_PWR_MAN, state->disp_pwr_man);
	MMIO_W(DISP_MERGE_CNTL, state->disp_merge_cntl);
	MMIO_W(DISP_OUTPUT_CNTL, state->disp_output_cntl);
	MMIO_W(DISP2_MERGE_CNTL, state->disp2_merge_cntl);

	/* Setup engine location. This shouldn't be necessary since we
	 * set them appropriately before any accel ops, but let's avoid
	 * random bogus DMA in case we inadvertently trigger the engine
	 * in the wrong place (happened).
	 */
	ret = radeon_ms_wait_for_fifo(dev, 2);
	if (ret) {
		ok = 0;
		DRM_INFO("[radeon_ms] no fifo for setting up dst & src gui\n");
		DRM_INFO("[radeon_ms] proceed anyway\n");
	}
	fbstart = (MC_FB_LOCATION__MC_FB_START__MASK &
			MMIO_R(MC_FB_LOCATION)) << 16;
	MMIO_W(DST_PITCH_OFFSET,
		REG_S(DST_PITCH_OFFSET, DST_OFFSET, fbstart >> 10));
	MMIO_W(SRC_PITCH_OFFSET,
		REG_S(SRC_PITCH_OFFSET, SRC_OFFSET, fbstart >> 10));

	ret = radeon_ms_wait_for_fifo(dev, 1);
	if (ret) {
		ok = 0;
		DRM_INFO("[radeon_ms] no fifo for setting up dp data type\n");
		DRM_INFO("[radeon_ms] proceed anyway\n");
	}
#ifdef __BIG_ENDIAN
	MMIO_W(DP_DATATYPE, DP_DATATYPE__DP_BYTE_PIX_ORDER);
#else
	MMIO_W(DP_DATATYPE, 0);
#endif

	ret = radeon_ms_wait_for_fifo(dev, 1);
	if (ret) {
		ok = 0;
		DRM_INFO("[radeon_ms] no fifo for setting up surface cntl\n");
		DRM_INFO("[radeon_ms] proceed anyway\n");
	}
	MMIO_W(SURFACE_CNTL, SURFACE_CNTL__SURF_TRANSLATION_DIS);

	ret = radeon_ms_wait_for_fifo(dev, 2);
	if (ret) {
		ok = 0;
		DRM_INFO("[radeon_ms] no fifo for setting scissor\n");
		DRM_INFO("[radeon_ms] proceed anyway\n");
	}
	MMIO_W(DEFAULT_SC_BOTTOM_RIGHT, 0x1fff1fff);
	MMIO_W(DEFAULT2_SC_BOTTOM_RIGHT, 0x1fff1fff);

	ret = radeon_ms_wait_for_fifo(dev, 1);
	if (ret) {
		ok = 0;
		DRM_INFO("[radeon_ms] no fifo for setting up gui cntl\n");
		DRM_INFO("[radeon_ms] proceed anyway\n");
	}
	MMIO_W(DP_GUI_MASTER_CNTL, 0);

	ret = radeon_ms_wait_for_fifo(dev, 5);
	if (ret) {
		ok = 0;
		DRM_INFO("[radeon_ms] no fifo for setting up clear color\n");
		DRM_INFO("[radeon_ms] proceed anyway\n");
	}
	MMIO_W(DP_BRUSH_BKGD_CLR, 0x00000000);
	MMIO_W(DP_BRUSH_FRGD_CLR, 0xffffffff);
	MMIO_W(DP_SRC_BKGD_CLR, 0x00000000);
	MMIO_W(DP_SRC_FRGD_CLR, 0xffffffff);
	MMIO_W(DP_WRITE_MSK, 0xffffffff);

	if (!ok) {
		DRM_INFO("[radeon_ms] engine restore not enough fifo\n");
	}
}

void radeon_ms_gpu_save(struct drm_device *dev, struct radeon_state *state)
{
	struct drm_radeon_private *dev_priv = dev->dev_private;

	state->aic_ctrl = MMIO_R(AIC_CTRL);
	state->bus_cntl = MMIO_R(BUS_CNTL);
	state->mc_fb_location = MMIO_R(MC_FB_LOCATION);
	state->display_base_addr = MMIO_R(DISPLAY_BASE_ADDR);
	state->config_aper_0_base = MMIO_R(CONFIG_APER_0_BASE);
	state->config_aper_1_base = MMIO_R(CONFIG_APER_1_BASE);
	state->config_aper_size = MMIO_R(CONFIG_APER_SIZE);
	state->disp_misc_cntl = MMIO_R(DISP_MISC_CNTL);
	state->disp_pwr_man = MMIO_R(DISP_PWR_MAN);
	state->disp_merge_cntl = MMIO_R(DISP_MERGE_CNTL);
	state->disp_output_cntl = MMIO_R(DISP_OUTPUT_CNTL);
	state->disp2_merge_cntl = MMIO_R(DISP2_MERGE_CNTL);
	if (dev_priv->bus_save) {
		dev_priv->bus_save(dev, state);
	}
}

int radeon_ms_wait_for_idle(struct drm_device *dev)
{
	struct drm_radeon_private *dev_priv = dev->dev_private;
	struct radeon_state *state = &dev_priv->driver_state;
	int i, j, ret;

	for (i = 0; i < 2; i++) {
		ret = radeon_ms_wait_for_fifo(dev, 64);
		if (ret) {
			DRM_INFO("[radeon_ms] fifo not empty\n");
		}
		for (j = 0; j < dev_priv->usec_timeout; j++) {
			if (!(RBBM_STATUS__GUI_ACTIVE & MMIO_R(RBBM_STATUS))) {
				radeon_ms_gpu_flush(dev);
				return 0;
			}
			DRM_UDELAY(1);
		}
		DRM_INFO("[radeon_ms] idle timed out:  status=0x%08x\n",
				MMIO_R(RBBM_STATUS));
		radeon_ms_gpu_reset(dev);
		radeon_ms_gpu_resume(dev);
	}
	return -EBUSY;
}
