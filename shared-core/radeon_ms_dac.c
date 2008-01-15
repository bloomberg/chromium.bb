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

int radeon_ms_dac1_initialize(struct radeon_ms_output *output)
{
	struct drm_radeon_private *dev_priv = output->dev->dev_private;
	struct radeon_state *state = &dev_priv->driver_state;

	state->dac_cntl =
		REG_S(DAC_CNTL, DAC_RANGE_CNTL, DAC_RANGE_CNTL__PS2) |
		DAC_CNTL__DAC_8BIT_EN |
		DAC_CNTL__DAC_VGA_ADR_EN |
		DAC_CNTL__DAC_PDWN |
		REG_S(DAC_CNTL, DAC, 0xff);
	state->dac_ext_cntl = 0;
	state->dac_macro_cntl =
		DAC_MACRO_CNTL__DAC_PDWN_R |
		DAC_MACRO_CNTL__DAC_PDWN_G |
		DAC_MACRO_CNTL__DAC_PDWN_B |
		REG_S(DAC_MACRO_CNTL, DAC_WHITE_CNTL, 7) |
		REG_S(DAC_MACRO_CNTL, DAC_BG_ADJ, 7);
	state->dac_embedded_sync_cntl =
		DAC_EMBEDDED_SYNC_CNTL__DAC_EMBED_VSYNC_EN_Y_G;
	state->dac_broad_pulse = 0;
	state->dac_skew_clks = 0;
	state->dac_incr = 0;
	state->dac_neg_sync_level = 0;
	state->dac_pos_sync_level = 0;
	state->dac_blank_level = 0;
	state->dac_sync_equalization = 0;
	state->disp_output_cntl = 0;
	radeon_ms_dac1_restore(output, state);
	return 0;
}

enum drm_output_status radeon_ms_dac1_detect(struct radeon_ms_output *output)
{
	return output_status_unknown;
}

void radeon_ms_dac1_dpms(struct radeon_ms_output *output, int mode)
{
	struct drm_radeon_private *dev_priv = output->dev->dev_private;
	struct radeon_state *state = &dev_priv->driver_state;
	uint32_t dac_cntl;
	uint32_t dac_macro_cntl;

	dac_cntl = DAC_CNTL__DAC_PDWN;
	dac_macro_cntl = DAC_MACRO_CNTL__DAC_PDWN_R |
		DAC_MACRO_CNTL__DAC_PDWN_G |
		DAC_MACRO_CNTL__DAC_PDWN_B;
	switch(mode) {
	case DPMSModeOn:
		state->dac_cntl &= ~dac_cntl;
		state->dac_macro_cntl &= ~dac_macro_cntl;
		break;
	case DPMSModeStandby:
	case DPMSModeSuspend:
	case DPMSModeOff:
		state->dac_cntl |= dac_cntl;
		state->dac_macro_cntl |= dac_macro_cntl;
		break;
	default:
		/* error */
		break;
	}
	MMIO_W(DAC_CNTL, state->dac_cntl);
	MMIO_W(DAC_MACRO_CNTL, state->dac_macro_cntl);
}

int radeon_ms_dac1_get_modes(struct radeon_ms_output *output)
{
	return 0;
}

bool radeon_ms_dac1_mode_fixup(struct radeon_ms_output *output,
		struct drm_display_mode *mode,
		struct drm_display_mode *adjusted_mode)
{
	return true;
}

int radeon_ms_dac1_mode_set(struct radeon_ms_output *output,
		struct drm_display_mode *mode,
		struct drm_display_mode *adjusted_mode)
{
	struct drm_radeon_private *dev_priv = output->dev->dev_private;
	struct radeon_ms_connector *connector = output->connector;
	struct radeon_state *state = &dev_priv->driver_state;
	uint32_t v = 0;

	if (connector == NULL) {
		/* output not associated with a connector */
		return -EINVAL;
	}
	state->disp_output_cntl &= ~DISP_OUTPUT_CNTL__DISP_DAC_SOURCE__MASK;
	state->dac_cntl2 &= ~DAC_CNTL2__DAC_CLK_SEL;
	switch (connector->crtc) {
	case 1:
		v =  DISP_DAC_SOURCE__PRIMARYCRTC;
		break;
	case 2:
		v =  DISP_DAC_SOURCE__SECONDARYCRTC;
		state->dac_cntl2 |= DAC_CNTL2__DAC_CLK_SEL;
		break;
	}
	state->disp_output_cntl |= REG_S(DISP_OUTPUT_CNTL, DISP_DAC_SOURCE, v);
	MMIO_W(DISP_OUTPUT_CNTL, state->disp_output_cntl);
	MMIO_W(DAC_CNTL2, state->dac_cntl2);
	return 0;
}

void radeon_ms_dac1_restore(struct radeon_ms_output *output,
		struct radeon_state *state)
{
	struct drm_radeon_private *dev_priv = output->dev->dev_private;

	MMIO_W(DAC_CNTL, state->dac_cntl);
	MMIO_W(DAC_EXT_CNTL, state->dac_ext_cntl);
	MMIO_W(DAC_MACRO_CNTL, state->dac_macro_cntl);
	MMIO_W(DAC_EMBEDDED_SYNC_CNTL, state->dac_embedded_sync_cntl);
	MMIO_W(DAC_BROAD_PULSE, state->dac_broad_pulse);
	MMIO_W(DAC_SKEW_CLKS, state->dac_skew_clks);
	MMIO_W(DAC_INCR, state->dac_incr);
	MMIO_W(DAC_NEG_SYNC_LEVEL, state->dac_neg_sync_level);
	MMIO_W(DAC_POS_SYNC_LEVEL, state->dac_pos_sync_level);
	MMIO_W(DAC_BLANK_LEVEL, state->dac_blank_level);
	MMIO_W(DAC_SYNC_EQUALIZATION, state->dac_sync_equalization);
}

void radeon_ms_dac1_save(struct radeon_ms_output *output,
		struct radeon_state *state)
{
	struct drm_radeon_private *dev_priv = output->dev->dev_private;

	state->dac_cntl = MMIO_R(DAC_CNTL);
	state->dac_ext_cntl = MMIO_R(DAC_EXT_CNTL);
	state->dac_macro_cntl = MMIO_R(DAC_MACRO_CNTL);
	state->dac_embedded_sync_cntl = MMIO_R(DAC_EMBEDDED_SYNC_CNTL);
	state->dac_broad_pulse = MMIO_R(DAC_BROAD_PULSE);
	state->dac_skew_clks = MMIO_R(DAC_SKEW_CLKS);
	state->dac_incr = MMIO_R(DAC_INCR);
	state->dac_neg_sync_level = MMIO_R(DAC_NEG_SYNC_LEVEL);
	state->dac_pos_sync_level = MMIO_R(DAC_POS_SYNC_LEVEL);
	state->dac_blank_level = MMIO_R(DAC_BLANK_LEVEL);
	state->dac_sync_equalization = MMIO_R(DAC_SYNC_EQUALIZATION);
}

int radeon_ms_dac2_initialize(struct radeon_ms_output *output)
{
	struct drm_radeon_private *dev_priv = output->dev->dev_private;
	struct radeon_state *state = &dev_priv->driver_state;

	state->tv_dac_cntl = TV_DAC_CNTL__BGSLEEP |
		REG_S(TV_DAC_CNTL, STD, STD__PS2) |
		REG_S(TV_DAC_CNTL, BGADJ, 0);
	switch (dev_priv->family) {
	case CHIP_R100:
	case CHIP_R200:
	case CHIP_RV200:
	case CHIP_RV250:
	case CHIP_RV280:
	case CHIP_RS300:
	case CHIP_R300:
	case CHIP_R350:
	case CHIP_R360:
	case CHIP_RV350:
	case CHIP_RV370:
	case CHIP_RV380:
	case CHIP_RS400:
		state->tv_dac_cntl |= TV_DAC_CNTL__RDACPD |
			TV_DAC_CNTL__GDACPD |
			TV_DAC_CNTL__BDACPD |
			REG_S(TV_DAC_CNTL, DACADJ, 0);
		break;
	case CHIP_RV410:
	case CHIP_R420:
	case CHIP_R430:
	case CHIP_R480:
		state->tv_dac_cntl |= TV_DAC_CNTL__RDACPD_R4 |
			TV_DAC_CNTL__GDACPD_R4 |
			TV_DAC_CNTL__BDACPD_R4 |
			REG_S(TV_DAC_CNTL, DACADJ_R4, 0);
		break;
	}
	state->tv_master_cntl = TV_MASTER_CNTL__TV_ASYNC_RST |
		TV_MASTER_CNTL__CRT_ASYNC_RST |
		TV_MASTER_CNTL__RESTART_PHASE_FIX |
		TV_MASTER_CNTL__CRT_FIFO_CE_EN |
		TV_MASTER_CNTL__TV_FIFO_CE_EN;
	state->dac_cntl2 = 0;
	state->disp_output_cntl = 0;
	radeon_ms_dac2_restore(output, state);
	return 0;
}

enum drm_output_status radeon_ms_dac2_detect(struct radeon_ms_output *output)
{
	return output_status_unknown;
}

void radeon_ms_dac2_dpms(struct radeon_ms_output *output, int mode)
{
	struct drm_radeon_private *dev_priv = output->dev->dev_private;
	struct radeon_state *state = &dev_priv->driver_state;
	uint32_t tv_dac_cntl_on, tv_dac_cntl_off;

	tv_dac_cntl_off = TV_DAC_CNTL__BGSLEEP;
	tv_dac_cntl_on = TV_DAC_CNTL__NBLANK |
			 TV_DAC_CNTL__NHOLD;
	switch (dev_priv->family) {
	case CHIP_R100:
	case CHIP_R200:
	case CHIP_RV200:
	case CHIP_RV250:
	case CHIP_RV280:
	case CHIP_RS300:
	case CHIP_R300:
	case CHIP_R350:
	case CHIP_R360:
	case CHIP_RV350:
	case CHIP_RV370:
	case CHIP_RV380:
	case CHIP_RS400:
		tv_dac_cntl_off |= TV_DAC_CNTL__RDACPD |
			TV_DAC_CNTL__GDACPD |
			TV_DAC_CNTL__BDACPD;
		break;
	case CHIP_RV410:
	case CHIP_R420:
	case CHIP_R430:
	case CHIP_R480:
		tv_dac_cntl_off |= TV_DAC_CNTL__RDACPD_R4 |
			TV_DAC_CNTL__GDACPD_R4 |
			TV_DAC_CNTL__BDACPD_R4;
		break;
	}
	switch(mode) {
	case DPMSModeOn:
		state->tv_dac_cntl &= ~tv_dac_cntl_off;
		state->tv_dac_cntl |= tv_dac_cntl_on;
		break;
	case DPMSModeStandby:
	case DPMSModeSuspend:
	case DPMSModeOff:
		state->tv_dac_cntl &= ~tv_dac_cntl_on;
		state->tv_dac_cntl |= tv_dac_cntl_off;
		break;
	default:
		/* error */
		break;
	}
	MMIO_W(TV_DAC_CNTL, state->tv_dac_cntl);
}

int radeon_ms_dac2_get_modes(struct radeon_ms_output *output)
{
	return 0;
}

bool radeon_ms_dac2_mode_fixup(struct radeon_ms_output *output,
		struct drm_display_mode *mode,
		struct drm_display_mode *adjusted_mode)
{
	return true;
}

int radeon_ms_dac2_mode_set(struct radeon_ms_output *output,
		struct drm_display_mode *mode,
		struct drm_display_mode *adjusted_mode)
{
	struct drm_radeon_private *dev_priv = output->dev->dev_private;
	struct radeon_ms_connector *connector = output->connector;
	struct radeon_state *state = &dev_priv->driver_state;

	if (connector == NULL) {
		/* output not associated with a connector */
		return -EINVAL;
	}
	switch (dev_priv->family) {
	case CHIP_R100:
	case CHIP_R200:
		state->disp_output_cntl &= ~DISP_OUTPUT_CNTL__DISP_TV_SOURCE;
		switch (connector->crtc) {
		case 1:
			break;
		case 2:
			state->disp_output_cntl |=
				DISP_OUTPUT_CNTL__DISP_TV_SOURCE;
			break;
		}
		break;
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
		state->disp_output_cntl &=
			~DISP_OUTPUT_CNTL__DISP_TVDAC_SOURCE__MASK;
		switch (connector->crtc) {
		case 1:
			state->disp_output_cntl |=
				REG_S(DISP_OUTPUT_CNTL,
						DISP_TVDAC_SOURCE,
						DISP_TVDAC_SOURCE__PRIMARYCRTC);
				break;
		case 2:
				state->disp_output_cntl |=
					REG_S(DISP_OUTPUT_CNTL,
							DISP_TVDAC_SOURCE,
							DISP_TVDAC_SOURCE__SECONDARYCRTC);
				break;
		}
		break;
	}
	switch (dev_priv->family) {
	case CHIP_R200:
		break;
	case CHIP_R100:
	case CHIP_RV200:
	case CHIP_RV250:
	case CHIP_RV280:
	case CHIP_RS300:
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
		if (connector->type != CONNECTOR_CTV &&
		    connector->type != CONNECTOR_STV) {
			state->dac_cntl2 |= DAC_CNTL2__DAC2_CLK_SEL;
		}
	}
	MMIO_W(DAC_CNTL2, state->dac_cntl2);
	MMIO_W(DISP_OUTPUT_CNTL, state->disp_output_cntl);
	return 0;
}

void radeon_ms_dac2_restore(struct radeon_ms_output *output,
		struct radeon_state *state)
{
	struct drm_radeon_private *dev_priv = output->dev->dev_private;

	MMIO_W(DAC_CNTL2, state->dac_cntl2);
	MMIO_W(TV_DAC_CNTL, state->tv_dac_cntl);
	MMIO_W(TV_MASTER_CNTL, state->tv_master_cntl);
}

void radeon_ms_dac2_save(struct radeon_ms_output *output,
		struct radeon_state *state)
{
	struct drm_radeon_private *dev_priv = output->dev->dev_private;

	state->dac_cntl2 = MMIO_R(DAC_CNTL2);
	state->tv_dac_cntl = MMIO_R(TV_DAC_CNTL);
	state->tv_master_cntl = MMIO_R(TV_MASTER_CNTL);
}
