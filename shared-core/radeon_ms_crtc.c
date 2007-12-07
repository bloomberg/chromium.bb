/*
 * Copyright © 2007 Alex Deucher
 * Copyright © 2007 Dave Airlie
 * Copyright © 2007 Michel Dänzer
 * Copyright © 2007 Jerome Glisse
 *
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
#include "drmP.h"
#include "drm.h"
#include "drm_crtc.h"
#include "radeon_ms.h"

static void radeon_pll1_init(struct drm_radeon_private *dev_priv,
			     struct radeon_state *state);
static void radeon_pll1_restore(struct drm_radeon_private *dev_priv,
				struct radeon_state *state);
static void radeon_pll1_save(struct drm_radeon_private *dev_priv,
			     struct radeon_state *state);
static void radeon_ms_crtc_load_lut(struct drm_crtc *crtc);

/**
 * radeon_ms_crtc1_init - initialize CRTC state
 * @dev_priv: radeon private structure
 * @state: state structure to initialize to default value
 *
 * Initialize CRTC state to default values
 */
static void radeon_ms_crtc1_init(struct drm_radeon_private *dev_priv,
				 struct radeon_state *state)
{
	state->surface_cntl = SURFACE_CNTL__SURF_TRANSLATION_DIS;
	state->surface0_info = 0;
	state->surface0_lower_bound = 0;
	state->surface0_upper_bound = 0;
	state->surface1_info = 0;
	state->surface1_lower_bound = 0;
	state->surface1_upper_bound = 0;
	state->surface2_info = 0;
	state->surface2_lower_bound = 0;
	state->surface2_upper_bound = 0;
	state->surface3_info = 0;
	state->surface3_lower_bound = 0;
	state->surface3_upper_bound = 0;
	state->surface4_info = 0;
	state->surface4_lower_bound = 0;
	state->surface4_upper_bound = 0;
	state->surface5_info = 0;
	state->surface5_lower_bound = 0;
	state->surface5_upper_bound = 0;
	state->surface6_info = 0;
	state->surface6_lower_bound = 0;
	state->surface6_upper_bound = 0;
	state->surface7_info = 0;
	state->surface7_lower_bound = 0;
	state->surface7_upper_bound = 0;
	state->crtc_gen_cntl = CRTC_GEN_CNTL__CRTC_EXT_DISP_EN |
			       CRTC_GEN_CNTL__CRTC_DISP_REQ_EN_B;
	state->crtc_ext_cntl = CRTC_EXT_CNTL__VGA_ATI_LINEAR |
			       CRTC_EXT_CNTL__VGA_XCRT_CNT_EN |
			       CRTC_EXT_CNTL__CRT_ON;
	state->crtc_h_total_disp = 0;
	state->crtc_h_sync_strt_wid = 0;
	state->crtc_v_total_disp = 0;
	state->crtc_v_sync_strt_wid = 0;
	state->crtc_offset = 0;
	state->crtc_pitch = 0;
	state->crtc_more_cntl = 0;
	state->crtc_tile_x0_y0 = 0;
	state->crtc_offset_cntl = 0;
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
		state->crtc_offset_cntl |= REG_S(CRTC_OFFSET_CNTL,
				CRTC_MICRO_TILE_BUFFER_MODE,
				CRTC_MICRO_TILE_BUFFER_MODE__DIS);
		break;
	default:
		DRM_ERROR("Unknown radeon family, aborting\n");
		return;
	}
	radeon_pll1_init(dev_priv, state);
}

/**
 * radeon_pll1_init - initialize PLL1 state
 * @dev_priv: radeon private structure
 * @state: state structure to initialize to default value
 *
 * Initialize PLL1 state to default values
 */
static void radeon_pll1_init(struct drm_radeon_private *dev_priv,
			     struct radeon_state *state)
{
	state->clock_cntl_index = 0;
	state->ppll_cntl = PPLL_R(PPLL_CNTL);
	state->ppll_cntl |= PPLL_CNTL__PPLL_ATOMIC_UPDATE_EN |
		PPLL_CNTL__PPLL_ATOMIC_UPDATE_SYNC |
		PPLL_CNTL__PPLL_VGA_ATOMIC_UPDATE_EN;
	state->ppll_cntl &= ~PPLL_CNTL__PPLL_TST_EN;
	state->ppll_cntl &= ~PPLL_CNTL__PPLL_TCPOFF;
	state->ppll_cntl &= ~PPLL_CNTL__PPLL_TVCOMAX;
	state->ppll_cntl &= ~PPLL_CNTL__PPLL_DISABLE_AUTO_RESET;
	state->ppll_ref_div = 0;
	state->ppll_ref_div = REG_S(PPLL_REF_DIV, PPLL_REF_DIV, 12) |
		REG_S(PPLL_REF_DIV, PPLL_REF_DIV_SRC, PPLL_REF_DIV_SRC__XTALIN);
	state->ppll_div_0 = 0;
	state->ppll_div_1 = 0;
	state->ppll_div_2 = 0;
	state->ppll_div_3 = 0;
	state->vclk_ecp_cntl = 0;
	state->htotal_cntl = 0;
}

/**
 * radeon_ms_crtc1_restore - restore CRTC state
 * @dev_priv: radeon private structure
 * @state: CRTC state to restore
 */
void radeon_ms_crtc1_restore(struct drm_device *dev, struct radeon_state *state)
{
	struct drm_radeon_private *dev_priv = dev->dev_private;

	/* We prevent the CRTC from hitting the memory controller until
	 * fully programmed
	 */
	MMIO_W(CRTC_GEN_CNTL, ~CRTC_GEN_CNTL__CRTC_DISP_REQ_EN_B &
			state->crtc_gen_cntl);
	MMIO_W(CRTC_EXT_CNTL, CRTC_EXT_CNTL__CRTC_VSYNC_DIS |
			CRTC_EXT_CNTL__CRTC_HSYNC_DIS |
			CRTC_EXT_CNTL__CRTC_DISPLAY_DIS |
			state->crtc_ext_cntl);
	MMIO_W(SURFACE_CNTL, state->surface_cntl);
	MMIO_W(SURFACE0_INFO, state->surface0_info);
	MMIO_W(SURFACE0_LOWER_BOUND, state->surface0_lower_bound);
	MMIO_W(SURFACE0_UPPER_BOUND, state->surface0_upper_bound);
	MMIO_W(SURFACE1_INFO, state->surface1_info);
	MMIO_W(SURFACE1_LOWER_BOUND, state->surface1_lower_bound);
	MMIO_W(SURFACE1_UPPER_BOUND, state->surface1_upper_bound);
	MMIO_W(SURFACE2_INFO, state->surface2_info);
	MMIO_W(SURFACE2_LOWER_BOUND, state->surface2_lower_bound);
	MMIO_W(SURFACE2_UPPER_BOUND, state->surface2_upper_bound);
	MMIO_W(SURFACE3_INFO, state->surface3_info);
	MMIO_W(SURFACE3_LOWER_BOUND, state->surface3_lower_bound);
	MMIO_W(SURFACE3_UPPER_BOUND, state->surface3_upper_bound);
	MMIO_W(SURFACE4_INFO, state->surface4_info);
	MMIO_W(SURFACE4_LOWER_BOUND, state->surface4_lower_bound);
	MMIO_W(SURFACE4_UPPER_BOUND, state->surface4_upper_bound);
	MMIO_W(SURFACE5_INFO, state->surface5_info);
	MMIO_W(SURFACE5_LOWER_BOUND, state->surface5_lower_bound);
	MMIO_W(SURFACE5_UPPER_BOUND, state->surface5_upper_bound);
	MMIO_W(SURFACE6_INFO, state->surface6_info);
	MMIO_W(SURFACE6_LOWER_BOUND, state->surface6_lower_bound);
	MMIO_W(SURFACE6_UPPER_BOUND, state->surface6_upper_bound);
	MMIO_W(SURFACE7_INFO, state->surface7_info);
	MMIO_W(SURFACE7_LOWER_BOUND, state->surface7_lower_bound);
	MMIO_W(SURFACE7_UPPER_BOUND, state->surface7_upper_bound);
	MMIO_W(CRTC_H_TOTAL_DISP, state->crtc_h_total_disp);
	MMIO_W(CRTC_H_SYNC_STRT_WID, state->crtc_h_sync_strt_wid);
	MMIO_W(CRTC_V_TOTAL_DISP, state->crtc_v_total_disp);
	MMIO_W(CRTC_V_SYNC_STRT_WID, state->crtc_v_sync_strt_wid);
	MMIO_W(FP_H_SYNC_STRT_WID, state->fp_h_sync_strt_wid);
	MMIO_W(FP_V_SYNC_STRT_WID, state->fp_v_sync_strt_wid);
	MMIO_W(FP_CRTC_H_TOTAL_DISP, state->fp_crtc_h_total_disp);
	MMIO_W(FP_CRTC_V_TOTAL_DISP, state->fp_crtc_v_total_disp);
	MMIO_W(CRTC_TILE_X0_Y0, state->crtc_tile_x0_y0);
	MMIO_W(CRTC_OFFSET_CNTL, state->crtc_offset_cntl);
	MMIO_W(CRTC_OFFSET, state->crtc_offset);
	MMIO_W(CRTC_PITCH, state->crtc_pitch);
	radeon_pll1_restore(dev_priv, state);
	MMIO_W(CRTC_MORE_CNTL, state->crtc_more_cntl);
	MMIO_W(CRTC_GEN_CNTL, state->crtc_gen_cntl);
	MMIO_W(CRTC_EXT_CNTL, state->crtc_ext_cntl);
}

/**
 * radeon_pll1_restore - restore PLL1 state
 * @dev_priv: radeon private structure
 * @state: PLL1 state to restore
 */
static void radeon_pll1_restore(struct drm_radeon_private *dev_priv,
				struct radeon_state *state)
{
	uint32_t tmp;

	/* switch to gpu clock while programing new clock */
	MMIO_W(CLOCK_CNTL_INDEX, state->clock_cntl_index);
	tmp = state->vclk_ecp_cntl;
	tmp = REG_S(VCLK_ECP_CNTL, VCLK_SRC_SEL, VCLK_SRC_SEL__CPUCLK);
	PPLL_W(VCLK_ECP_CNTL, tmp);
	/* reset PLL and update atomicly */
	state->ppll_cntl |= PPLL_CNTL__PPLL_ATOMIC_UPDATE_EN |
		PPLL_CNTL__PPLL_ATOMIC_UPDATE_SYNC; 

	PPLL_W(PPLL_CNTL, state->ppll_cntl | PPLL_CNTL__PPLL_RESET);
	PPLL_W(PPLL_REF_DIV, state->ppll_ref_div);
	PPLL_W(PPLL_DIV_0, state->ppll_div_0);
	PPLL_W(PPLL_DIV_1, state->ppll_div_1);
	PPLL_W(PPLL_DIV_2, state->ppll_div_2);
	PPLL_W(PPLL_DIV_3, state->ppll_div_3);
	PPLL_W(HTOTAL_CNTL, state->htotal_cntl);

	/* update */
	PPLL_W(PPLL_REF_DIV, state->ppll_ref_div |
			PPLL_REF_DIV__PPLL_ATOMIC_UPDATE_W);
	for (tmp = 0; tmp < 100; tmp++) {
		if (!(PPLL_REF_DIV__PPLL_ATOMIC_UPDATE_R &
					PPLL_R(PPLL_REF_DIV))) {
		      break;
		}
		DRM_UDELAY(10);
	}
	state->ppll_cntl &= ~PPLL_CNTL__PPLL_RESET;
	PPLL_W(PPLL_CNTL, state->ppll_cntl);
	PPLL_W(VCLK_ECP_CNTL, state->vclk_ecp_cntl);
}

/**
 * radeon_ms_crtc1_save - save CRTC state
 * @dev_priv: radeon private structure
 * @state: state where saving current CRTC state
 */
void radeon_ms_crtc1_save(struct drm_device *dev, struct radeon_state *state)
{
	struct drm_radeon_private *dev_priv = dev->dev_private;

	state->surface_cntl = MMIO_R(SURFACE_CNTL);
	state->surface0_info = MMIO_R(SURFACE0_INFO);
	state->surface0_lower_bound = MMIO_R(SURFACE0_LOWER_BOUND);
	state->surface0_upper_bound = MMIO_R(SURFACE0_UPPER_BOUND);
	state->surface1_info = MMIO_R(SURFACE1_INFO);
	state->surface1_lower_bound = MMIO_R(SURFACE1_LOWER_BOUND);
	state->surface1_upper_bound = MMIO_R(SURFACE1_UPPER_BOUND);
	state->surface2_info = MMIO_R(SURFACE2_INFO);
	state->surface2_lower_bound = MMIO_R(SURFACE2_LOWER_BOUND);
	state->surface2_upper_bound = MMIO_R(SURFACE2_UPPER_BOUND);
	state->surface3_info = MMIO_R(SURFACE3_INFO);
	state->surface3_lower_bound = MMIO_R(SURFACE3_LOWER_BOUND);
	state->surface3_upper_bound = MMIO_R(SURFACE3_UPPER_BOUND);
	state->surface4_info = MMIO_R(SURFACE4_INFO);
	state->surface4_lower_bound = MMIO_R(SURFACE4_LOWER_BOUND);
	state->surface4_upper_bound = MMIO_R(SURFACE4_UPPER_BOUND);
	state->surface5_info = MMIO_R(SURFACE5_INFO);
	state->surface5_lower_bound = MMIO_R(SURFACE5_LOWER_BOUND);
	state->surface5_upper_bound = MMIO_R(SURFACE5_UPPER_BOUND);
	state->surface6_info = MMIO_R(SURFACE6_INFO);
	state->surface6_lower_bound = MMIO_R(SURFACE6_LOWER_BOUND);
	state->surface6_upper_bound = MMIO_R(SURFACE6_UPPER_BOUND);
	state->surface7_info = MMIO_R(SURFACE7_INFO);
	state->surface7_lower_bound = MMIO_R(SURFACE7_LOWER_BOUND);
	state->surface7_upper_bound = MMIO_R(SURFACE7_UPPER_BOUND);
	state->crtc_gen_cntl = MMIO_R(CRTC_GEN_CNTL);
	state->crtc_ext_cntl = MMIO_R(CRTC_EXT_CNTL);
	state->crtc_h_total_disp = MMIO_R(CRTC_H_TOTAL_DISP);
	state->crtc_h_sync_strt_wid = MMIO_R(CRTC_H_SYNC_STRT_WID);
	state->crtc_v_total_disp = MMIO_R(CRTC_V_TOTAL_DISP);
	state->crtc_v_sync_strt_wid = MMIO_R(CRTC_V_SYNC_STRT_WID);
	state->fp_h_sync_strt_wid = MMIO_R(FP_H_SYNC_STRT_WID);
	state->fp_v_sync_strt_wid = MMIO_R(FP_V_SYNC_STRT_WID);
	state->fp_crtc_h_total_disp = MMIO_R(FP_CRTC_H_TOTAL_DISP);
	state->fp_crtc_v_total_disp = MMIO_R(FP_CRTC_V_TOTAL_DISP);
	state->crtc_offset = MMIO_R(CRTC_OFFSET);
	state->crtc_offset_cntl = MMIO_R(CRTC_OFFSET_CNTL);
	state->crtc_pitch = MMIO_R(CRTC_PITCH);
	state->crtc_more_cntl = MMIO_R(CRTC_MORE_CNTL);
	state->crtc_tile_x0_y0 =  MMIO_R(CRTC_TILE_X0_Y0);
	radeon_pll1_save(dev_priv,state);
}

/**
 * radeon_pll1_save - save PLL1 state
 * @dev_priv: radeon private structure
 * @state: state where saving current PLL1 state
 */
static void radeon_pll1_save(struct drm_radeon_private *dev_priv,
			     struct radeon_state *state)
{
	state->clock_cntl_index = MMIO_R(CLOCK_CNTL_INDEX);
	state->ppll_cntl = PPLL_R(PPLL_CNTL);
	state->ppll_ref_div = PPLL_R(PPLL_REF_DIV);
	state->ppll_div_0 = PPLL_R(PPLL_DIV_0);
	state->ppll_div_1 = PPLL_R(PPLL_DIV_1);
	state->ppll_div_2 = PPLL_R(PPLL_DIV_2);
	state->ppll_div_3 = PPLL_R(PPLL_DIV_3);
	state->vclk_ecp_cntl = PPLL_R(VCLK_ECP_CNTL);
	state->htotal_cntl = PPLL_R(HTOTAL_CNTL);
}

static void radeon_ms_crtc1_dpms(struct drm_crtc *crtc, int mode)
{
	struct drm_radeon_private *dev_priv = crtc->dev->dev_private;
	struct radeon_state *state = &dev_priv->driver_state;

	state->crtc_gen_cntl &= ~CRTC_GEN_CNTL__CRTC_DISP_REQ_EN_B;
	state->crtc_ext_cntl &= ~CRTC_EXT_CNTL__CRTC_DISPLAY_DIS;
	state->crtc_ext_cntl &= ~CRTC_EXT_CNTL__CRTC_HSYNC_DIS;
	state->crtc_ext_cntl &= ~CRTC_EXT_CNTL__CRTC_VSYNC_DIS;
	switch(mode) {
	case DPMSModeOn:
		break;
	case DPMSModeStandby:
		state->crtc_ext_cntl |=
			CRTC_EXT_CNTL__CRTC_DISPLAY_DIS |
			CRTC_EXT_CNTL__CRTC_HSYNC_DIS;
		break;
	case DPMSModeSuspend:
		state->crtc_ext_cntl |=
			CRTC_EXT_CNTL__CRTC_DISPLAY_DIS |
			CRTC_EXT_CNTL__CRTC_VSYNC_DIS;
		break;
	case DPMSModeOff:
		state->crtc_ext_cntl |=
			CRTC_EXT_CNTL__CRTC_DISPLAY_DIS |
			CRTC_EXT_CNTL__CRTC_HSYNC_DIS |
			CRTC_EXT_CNTL__CRTC_VSYNC_DIS;
		state->crtc_gen_cntl |=
			CRTC_GEN_CNTL__CRTC_DISP_REQ_EN_B;
		break;
	}
	MMIO_W(CRTC_GEN_CNTL, state->crtc_gen_cntl);
	MMIO_W(CRTC_EXT_CNTL, state->crtc_ext_cntl);

	dev_priv->crtc1_dpms = mode;
	/* FIXME: once adding crtc2 remove this */
	dev_priv->crtc2_dpms = mode;
	radeon_ms_gpu_dpms(crtc->dev);

	if (mode != DPMSModeOff) {
		radeon_ms_crtc_load_lut(crtc);
	}
}

static bool radeon_ms_crtc_mode_fixup(struct drm_crtc *crtc,
				      struct drm_display_mode *mode,
				      struct drm_display_mode *adjusted_mode)
{
	return true;
}

static void radeon_ms_crtc_mode_prepare(struct drm_crtc *crtc)
{
	crtc->funcs->dpms(crtc, DPMSModeOff);
}

/* compute PLL registers values for requested video mode */
static int radeon_pll1_constraint(int clock, int rdiv,
                                  int fdiv, int pdiv,
                                  int rfrq, int pfrq)
{
    int dfrq;

    if (rdiv < 2 || fdiv < 4) {
        return 0;
    }
    dfrq = rfrq / rdiv;
    if (dfrq < 2000 || dfrq > 3300) {
        return 0;
    }
    if (pfrq < 125000 || pfrq > 250000) {
        return 0;
    }
    return 1;
}

static void radeon_pll1_compute(struct drm_crtc *crtc,
				struct drm_display_mode *mode)
{
	struct {
		int divider;
		int divider_id;
	} *post_div, post_divs[] = {
		/* From RAGE 128 VR/RAGE 128 GL Register
		 * Reference Manual (Technical Reference
		 * Manual P/N RRG-G04100-C Rev. 0.04), page
		 * 3-17 (PLL_DIV_[3:0]).
		 */
		{  1, 0 },              /* VCLK_SRC                 */
		{  2, 1 },              /* VCLK_SRC/2               */
		{  4, 2 },              /* VCLK_SRC/4               */
		{  8, 3 },              /* VCLK_SRC/8               */
		{  3, 4 },              /* VCLK_SRC/3               */
		{ 16, 5 },              /* VCLK_SRC/16              */
		{  6, 6 },              /* VCLK_SRC/6               */
		{ 12, 7 },              /* VCLK_SRC/12              */
		{  0, 0 }
	};
	struct drm_radeon_private *dev_priv = crtc->dev->dev_private;
	struct radeon_state *state = &dev_priv->driver_state;
	int clock = mode->clock;
	int rfrq = dev_priv->properties->pll_reference_freq;
	int pdiv = 1;
	int pdiv_id = 0;
	int rdiv_best = 2;
	int fdiv_best = 4;
	int tfrq_best = 0;
	int pfrq_best = 0;
	int diff_cpfrq_best = 350000;
	int vco_freq;
	int vco_gain;
	int rdiv = 0;
	int fdiv = 0;
	int tfrq = 35000;
	int pfrq = 35000;
	int diff_cpfrq = 350000;

	/* clamp frequency into pll [min; max] frequency range */
	if (clock > dev_priv->properties->pll_max_pll_freq) {
		clock = dev_priv->properties->pll_max_pll_freq;
	}
	if ((clock * 12) < dev_priv->properties->pll_min_pll_freq) {
		clock = dev_priv->properties->pll_min_pll_freq / 12;
	}

	/* maximize pll_ref_div while staying in boundary and minimizing
	 * the difference btw target frequency and programmed frequency */
	for (post_div = &post_divs[0]; post_div->divider; ++post_div) {
		if (post_div->divider == 0) {
			break;
		}
		tfrq = clock * post_div->divider;
		for (fdiv = 1023; fdiv >= 4; fdiv--) {
			rdiv = (fdiv * rfrq) / tfrq;
			if (radeon_pll1_constraint(clock, rdiv, fdiv,
						pdiv, rfrq, tfrq)) {
				pfrq = (fdiv * rfrq) / rdiv;
				diff_cpfrq = pfrq - tfrq;
				if ((diff_cpfrq >= 0 &&
				     diff_cpfrq < diff_cpfrq_best) ||
				    (diff_cpfrq == diff_cpfrq_best &&
				     rdiv > rdiv_best)) {
					rdiv_best = rdiv;
					fdiv_best = fdiv;
					tfrq_best = tfrq;
					pfrq_best = pfrq;
					pdiv = post_div->divider;
					pdiv_id = post_div->divider_id;
					diff_cpfrq_best = diff_cpfrq;
				}
			}
		}
	}
	state->ppll_ref_div =
		REG_S(PPLL_REF_DIV, PPLL_REF_DIV, rdiv_best) |
		REG_S(PPLL_REF_DIV, PPLL_REF_DIV_ACC, rdiv_best);
	state->ppll_div_0 = REG_S(PPLL_DIV_0, PPLL_FB0_DIV, fdiv_best) |
		REG_S(PPLL_DIV_0, PPLL_POST0_DIV, pdiv_id);

	vco_freq = (fdiv_best * rfrq) / rdiv_best;
	/* This is horribly crude: the VCO frequency range is divided into
	 * 3 parts, each part having a fixed PLL gain value.
	 */
        if (vco_freq >= 300000) {
		/* [300..max] MHz : 7 */
		vco_gain = 7;
	} else if (vco_freq >= 180000) {
		/* [180..300) MHz : 4 */
		vco_gain = 4;
	} else {
		/* [0..180) MHz : 1 */
		vco_gain = 1;
	}
	state->ppll_cntl |= REG_S(PPLL_CNTL, PPLL_PVG, vco_gain);
	state->vclk_ecp_cntl |= REG_S(VCLK_ECP_CNTL, VCLK_SRC_SEL,
			VCLK_SRC_SEL__PPLLCLK);
	state->htotal_cntl = 0;
	DRM_INFO("rdiv: %d\n", rdiv_best);
	DRM_INFO("fdiv: %d\n", fdiv_best);
	DRM_INFO("pdiv: %d\n", pdiv);
	DRM_INFO("pdiv: %d\n", pdiv_id);
	DRM_INFO("tfrq: %d\n", tfrq_best);
	DRM_INFO("pfrq: %d\n", pfrq_best);
	DRM_INFO("PPLL_REF_DIV:  0x%08X\n", state->ppll_ref_div);
	DRM_INFO("PPLL_DIV_0:    0x%08X\n", state->ppll_div_0);
	DRM_INFO("PPLL_CNTL:     0x%08X\n", state->ppll_cntl);
	DRM_INFO("VCLK_ECP_CNTL: 0x%08X\n", state->vclk_ecp_cntl);
}

static void radeon_ms_crtc1_mode_set(struct drm_crtc *crtc,
				     struct drm_display_mode *mode,
				     struct drm_display_mode *adjusted_mode,
				     int x, int y)
{
	struct drm_device *dev = crtc->dev;
	struct drm_radeon_private *dev_priv = dev->dev_private;
	struct radeon_state *state = &dev_priv->driver_state;
	int format, hsync_wid, vsync_wid, pitch;

	DRM_INFO("[radeon_ms] set modeline %d:\"%s\" %d %d %d %d %d %d %d %d %d %d 0x%x\n",
		  mode->mode_id, mode->name, mode->vrefresh, mode->clock,
		  mode->hdisplay, mode->hsync_start,
		  mode->hsync_end, mode->htotal,
		  mode->vdisplay, mode->vsync_start,
		  mode->vsync_end, mode->vtotal, mode->type);
	DRM_INFO("[radeon_ms] set modeline %d:\"%s\" %d %d %d %d %d %d %d %d %d %d 0x%x (adjusted)\n",
		  adjusted_mode->mode_id, adjusted_mode->name, adjusted_mode->vrefresh, adjusted_mode->clock,
		  adjusted_mode->hdisplay, adjusted_mode->hsync_start,
		  adjusted_mode->hsync_end, adjusted_mode->htotal,
		  adjusted_mode->vdisplay, adjusted_mode->vsync_start,
		  adjusted_mode->vsync_end, adjusted_mode->vtotal, adjusted_mode->type);

	/* only support RGB555,RGB565,ARGB8888 should satisfy all users */
	switch (crtc->fb->bits_per_pixel) {
	case 16:
		if (crtc->fb->depth == 15) {
			format = 3;
		} else {
			format = 4;
		}
		break;
	case 32:
		format = 6;
		break;
	default:
		DRM_ERROR("Unknown color depth\n");
		return;
	}
	radeon_pll1_compute(crtc, adjusted_mode);

	state->crtc_offset = REG_S(CRTC_OFFSET, CRTC_OFFSET, crtc->fb->offset);
	state->crtc_gen_cntl = CRTC_GEN_CNTL__CRTC_EXT_DISP_EN |
		CRTC_GEN_CNTL__CRTC_EN |
		REG_S(CRTC_GEN_CNTL, CRTC_PIX_WIDTH, format);
	if (adjusted_mode->flags & V_DBLSCAN) {
		state->crtc_gen_cntl |= CRTC_GEN_CNTL__CRTC_DBL_SCAN_EN;
	}
	if (adjusted_mode->flags & V_CSYNC) {
		state->crtc_gen_cntl |= CRTC_GEN_CNTL__CRTC_C_SYNC_EN;
	}
	if (adjusted_mode->flags & V_INTERLACE) {
		state->crtc_gen_cntl |= CRTC_GEN_CNTL__CRTC_INTERLACE_EN;
	}
	state->crtc_more_cntl = 0;
	state->crtc_h_total_disp =
		REG_S(CRTC_H_TOTAL_DISP,
				CRTC_H_TOTAL,
				(adjusted_mode->crtc_htotal/8) - 1) |
		REG_S(CRTC_H_TOTAL_DISP,
				CRTC_H_DISP,
				(adjusted_mode->crtc_hdisplay/8) - 1);
	hsync_wid = (adjusted_mode->crtc_hsync_end -
		     adjusted_mode->crtc_hsync_start) / 8;
	if (!hsync_wid) {
		hsync_wid = 1;
	}
	if (hsync_wid > 0x3f) {
		hsync_wid = 0x3f;
	}
	state->crtc_h_sync_strt_wid =
		REG_S(CRTC_H_SYNC_STRT_WID,
				CRTC_H_SYNC_WID, hsync_wid) |
		REG_S(CRTC_H_SYNC_STRT_WID,
				CRTC_H_SYNC_STRT_PIX,
				adjusted_mode->crtc_hsync_start) |
		REG_S(CRTC_H_SYNC_STRT_WID,
				CRTC_H_SYNC_STRT_CHAR,
				adjusted_mode->crtc_hsync_start/8);
	if (adjusted_mode->flags & V_NHSYNC) {
		state->crtc_h_sync_strt_wid |=
			CRTC_H_SYNC_STRT_WID__CRTC_H_SYNC_POL;
	}

	state->crtc_v_total_disp =
		REG_S(CRTC_V_TOTAL_DISP, CRTC_V_TOTAL,
				adjusted_mode->crtc_vtotal - 1) |
		REG_S(CRTC_V_TOTAL_DISP, CRTC_V_DISP,
				adjusted_mode->crtc_vdisplay - 1);
	vsync_wid = adjusted_mode->crtc_vsync_end -
		    adjusted_mode->crtc_vsync_start;
	if (!vsync_wid) {
		vsync_wid = 1;
	}
	if (vsync_wid > 0x1f) {
		vsync_wid = 0x1f;
	}
	state->crtc_v_sync_strt_wid =
		REG_S(CRTC_V_SYNC_STRT_WID,
				CRTC_V_SYNC_WID,
				vsync_wid) |
		REG_S(CRTC_V_SYNC_STRT_WID,
				CRTC_V_SYNC_STRT,
				adjusted_mode->crtc_vsync_start);
	if (adjusted_mode->flags & V_NVSYNC) {
		state->crtc_v_sync_strt_wid |=
			CRTC_V_SYNC_STRT_WID__CRTC_V_SYNC_POL;
	}

	pitch = (crtc->fb->width * crtc->fb->bits_per_pixel +
		 ((crtc->fb->bits_per_pixel * 8)- 1)) /
		(crtc->fb->bits_per_pixel * 8);
	state->crtc_pitch = REG_S(CRTC_PITCH, CRTC_PITCH, pitch) |
		REG_S(CRTC_PITCH, CRTC_PITCH_RIGHT, pitch);

	state->fp_h_sync_strt_wid = state->crtc_h_sync_strt_wid;
	state->fp_v_sync_strt_wid = state->crtc_v_sync_strt_wid;
	state->fp_crtc_h_total_disp = state->crtc_h_total_disp;
	state->fp_crtc_v_total_disp = state->crtc_v_total_disp;

	radeon_ms_crtc1_restore(dev, state);
}

static void radeon_ms_crtc_mode_commit(struct drm_crtc *crtc)
{
	crtc->funcs->dpms(crtc, DPMSModeOn);
}

static void radeon_ms_crtc_gamma_set(struct drm_crtc *crtc, u16 r,
				     u16 g, u16 b, int regno)
{
	struct drm_radeon_private *dev_priv = crtc->dev->dev_private;
	struct radeon_ms_crtc *radeon_ms_crtc = crtc->driver_private;
	struct radeon_state *state = &dev_priv->driver_state;
	uint32_t color;

	switch(radeon_ms_crtc->crtc) {
	case 1:
		state->dac_cntl2 &= ~DAC_CNTL2__PALETTE_ACCESS_CNTL;
		break;
	case 2:
		state->dac_cntl2 |= DAC_CNTL2__PALETTE_ACCESS_CNTL;
		break;
	}
	MMIO_W(DAC_CNTL2, state->dac_cntl2);
	if (crtc->fb->bits_per_pixel == 16 && crtc->fb->depth == 16) {
		if (regno >= 64) {
			return;
		}
		MMIO_W(PALETTE_INDEX,
				REG_S(PALETTE_INDEX, PALETTE_W_INDEX,
					regno * 4));
		color = 0;
		color = REG_S(PALETTE_DATA, PALETTE_DATA_R, r >> 8) |
			REG_S(PALETTE_DATA, PALETTE_DATA_G, g >> 8) |
			REG_S(PALETTE_DATA, PALETTE_DATA_B, b >> 8);
		MMIO_W(PALETTE_DATA, color);
		MMIO_W(PALETTE_INDEX,
		       REG_S(PALETTE_INDEX, PALETTE_W_INDEX, regno * 4));
		color = 0;
		color = REG_S(PALETTE_30_DATA, PALETTE_DATA_R, r >> 6) |
			REG_S(PALETTE_30_DATA, PALETTE_DATA_G, g >> 6) |
			REG_S(PALETTE_30_DATA, PALETTE_DATA_B, b >> 6);
		MMIO_W(PALETTE_30_DATA, color);
		radeon_ms_crtc->lut_r[regno * 4] = r;
		radeon_ms_crtc->lut_g[regno * 4] = g;
		radeon_ms_crtc->lut_b[regno * 4] = b;
		if (regno < 32) {
			MMIO_W(PALETTE_INDEX,
			       REG_S(PALETTE_INDEX, PALETTE_W_INDEX,
				       regno * 8));
			color = 0;
			color = REG_S(PALETTE_DATA, PALETTE_DATA_R, r >> 8) |
				REG_S(PALETTE_DATA, PALETTE_DATA_G, g >> 8) |
				REG_S(PALETTE_DATA, PALETTE_DATA_B, b >> 8);
			MMIO_W(PALETTE_DATA, color);
			MMIO_W(PALETTE_INDEX,
			       REG_S(PALETTE_INDEX, PALETTE_W_INDEX,
				      regno * 8));
			color = 0;
			color = REG_S(PALETTE_30_DATA, PALETTE_DATA_R,r >> 6) |
				REG_S(PALETTE_30_DATA, PALETTE_DATA_G,g >> 6) |
				REG_S(PALETTE_30_DATA, PALETTE_DATA_B,b >> 6);
			MMIO_W(PALETTE_30_DATA, color);
			radeon_ms_crtc->lut_r[regno * 8] = r;
			radeon_ms_crtc->lut_g[regno * 8] = g;
			radeon_ms_crtc->lut_b[regno * 8] = b;
		}
	} else {
		if (regno >= 256) {
			return;
		}
		radeon_ms_crtc->lut_r[regno] = r;
		radeon_ms_crtc->lut_g[regno] = g;
		radeon_ms_crtc->lut_b[regno] = b;
		MMIO_W(PALETTE_INDEX,
		       REG_S(PALETTE_INDEX, PALETTE_W_INDEX, regno));
		color = 0;
		color = REG_S(PALETTE_DATA, PALETTE_DATA_R, r >> 8) |
			REG_S(PALETTE_DATA, PALETTE_DATA_G, g >> 8) |
			REG_S(PALETTE_DATA, PALETTE_DATA_B, b >> 8);
		MMIO_W(PALETTE_DATA, color);
		MMIO_W(PALETTE_INDEX,
				REG_S(PALETTE_INDEX, PALETTE_W_INDEX, regno));
		color = 0;
		color = REG_S(PALETTE_30_DATA, PALETTE_DATA_R, r >> 6) |
			REG_S(PALETTE_30_DATA, PALETTE_DATA_G, g >> 6) |
			REG_S(PALETTE_30_DATA, PALETTE_DATA_B, b >> 6);
	}
}

static void radeon_ms_crtc_load_lut(struct drm_crtc *crtc)
{
	struct radeon_ms_crtc *radeon_ms_crtc = crtc->driver_private;
	int i;

	if (!crtc->enabled)
		return;

	for (i = 0; i < 256; i++) {
		radeon_ms_crtc_gamma_set(crtc,
				radeon_ms_crtc->lut_r[i],
				radeon_ms_crtc->lut_g[i],
				radeon_ms_crtc->lut_b[i],
				i);
	}
}

static bool radeon_ms_crtc_lock(struct drm_crtc *crtc)
{
    return true;
}

static void radeon_ms_crtc_unlock(struct drm_crtc *crtc)
{
}

static const struct drm_crtc_funcs radeon_ms_crtc1_funcs= {
	.dpms = radeon_ms_crtc1_dpms,
	.save = NULL, /* XXX */
	.restore = NULL, /* XXX */
	.lock = radeon_ms_crtc_lock,
	.unlock = radeon_ms_crtc_unlock,
	.prepare = radeon_ms_crtc_mode_prepare,
	.commit = radeon_ms_crtc_mode_commit,
	.mode_fixup = radeon_ms_crtc_mode_fixup,
	.mode_set = radeon_ms_crtc1_mode_set,
	.gamma_set = radeon_ms_crtc_gamma_set,
	.cleanup = NULL, /* XXX */
};

int radeon_ms_crtc_create(struct drm_device *dev, int crtc)
{
	struct drm_radeon_private *dev_priv = dev->dev_private;
	struct drm_crtc *drm_crtc;
	struct radeon_ms_crtc *radeon_ms_crtc;
	int i;

	switch (crtc) {
	case 1:
		radeon_ms_crtc1_init(dev_priv, &dev_priv->driver_state);
		drm_crtc = drm_crtc_create(dev, &radeon_ms_crtc1_funcs);
		break;
	case 2:
	default:
		return -EINVAL;
	}
	if (drm_crtc == NULL) {
		return -ENOMEM;
	}

	radeon_ms_crtc = drm_alloc(sizeof(struct radeon_ms_crtc), DRM_MEM_DRIVER);
	if (radeon_ms_crtc == NULL) {
		kfree(drm_crtc);
		return -ENOMEM;
	}

	radeon_ms_crtc->crtc = crtc;
	for (i = 0; i < 256; i++) {
		radeon_ms_crtc->lut_r[i] = i << 8;
		radeon_ms_crtc->lut_g[i] = i << 8;
		radeon_ms_crtc->lut_b[i] = i << 8;
	}
	drm_crtc->driver_private = radeon_ms_crtc;
	return 0;
}
