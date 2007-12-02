/*
 * Copyright 2007 Jérôme Glisse
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
#include "drmP.h"
#include "drm.h"
#include "radeon_ms.h"

static struct radeon_ms_output radeon_ms_dac1 = {
	OUTPUT_DAC1,
	NULL,
	NULL,
	radeon_ms_dac1_initialize,
	radeon_ms_dac1_detect,
	radeon_ms_dac1_dpms,
	radeon_ms_dac1_get_modes,
	radeon_ms_dac1_mode_fixup,
	radeon_ms_dac1_mode_set,
	radeon_ms_dac1_restore,
	radeon_ms_dac1_save
};

static struct radeon_ms_output radeon_ms_dac2 = {
	OUTPUT_DAC2,
	NULL,
	NULL,
	radeon_ms_dac2_initialize,
	radeon_ms_dac2_detect,
	radeon_ms_dac2_dpms,
	radeon_ms_dac2_get_modes,
	radeon_ms_dac2_mode_fixup,
	radeon_ms_dac2_mode_set,
	radeon_ms_dac2_restore,
	radeon_ms_dac2_save
};

static struct radeon_ms_connector radeon_ms_vga = {
	NULL, NULL, NULL, CONNECTOR_VGA, MT_NONE, 0, GPIO_DDC1,
	{
		0, -1, -1, -1, -1, -1, -1, -1
	},
	"VGA"
};

static struct radeon_ms_connector radeon_ms_dvi_i_2 = {
	NULL, NULL, NULL, CONNECTOR_DVI_I, MT_NONE, 0, GPIO_DDC2,
	{
		1, -1, -1, -1, -1, -1, -1, -1
	},
	"DVI-I"
};

static struct radeon_ms_properties properties[] = {
	/* default only one VGA connector */
	{
		0, 0, 2700, 2500, 20000, 1, 1, 1, 1,
		{
			&radeon_ms_dac1, NULL, NULL, NULL, NULL, NULL, NULL,
			NULL
		},
		{
			&radeon_ms_vga, NULL, NULL, NULL, NULL, NULL, NULL,
			NULL
		}
	},
	{
		0x1043, 0x176, 2700, 2500, 20000, 1, 1, 1, 1,
		{
			&radeon_ms_dac1, &radeon_ms_dac2, NULL, NULL, NULL,
			NULL, NULL, NULL
		},
		{
			&radeon_ms_vga, &radeon_ms_dvi_i_2, NULL, NULL, NULL,
			NULL, NULL, NULL
		}
	},
	{
		0x1002, 0x4150, 2700, 2500, 20000, 1, 1, 1, 1,
		{
			&radeon_ms_dac1, &radeon_ms_dac2, NULL, NULL, NULL,
			NULL, NULL, NULL
		},
		{
			&radeon_ms_vga, &radeon_ms_dvi_i_2, NULL, NULL, NULL,
			NULL, NULL, NULL
		}
	},
};

extern const uint32_t radeon_cp_microcode[];
extern const uint32_t r200_cp_microcode[];
extern const uint32_t r300_cp_microcode[];

static void radeon_flush_cache(struct drm_device *dev)
{
	struct drm_radeon_private *dev_priv = dev->dev_private;
	uint32_t cmd[6];
	int i, ret;

	cmd[0] = CP_PACKET0(RB2D_DSTCACHE_CTLSTAT, 0);
	cmd[1] = REG_S(RB2D_DSTCACHE_CTLSTAT, DC_FLUSH, 3);
	cmd[2] = CP_PACKET0(RB3D_DSTCACHE_CTLSTAT, 0);
	cmd[3] = REG_S(RB3D_DSTCACHE_CTLSTAT, DC_FLUSH, 3);
	cmd[4] = CP_PACKET0(RB3D_ZCACHE_CTLSTAT, 0);
	cmd[5] = RB3D_ZCACHE_CTLSTAT__ZC_FLUSH;
	/* try to wait but if we timeout we likely are in bad situation */
	for (i = 0; i < dev_priv->usec_timeout; i++) {
		ret = radeon_ms_ring_emit(dev, cmd, 6);
		if (!ret) {
			break;
		}
	}
}

static void r300_flush_cache(struct drm_device *dev)
{
	struct drm_radeon_private *dev_priv = dev->dev_private;
	uint32_t cmd[6];
	int i, ret;

	cmd[0] = CP_PACKET0(RB2D_DSTCACHE_CTLSTAT, 0);
	cmd[1] = REG_S(RB2D_DSTCACHE_CTLSTAT, DC_FLUSH, 3);
	cmd[2] = CP_PACKET0(RB3D_DSTCACHE_CTLSTAT_R3, 0);
	cmd[3] = REG_S(RB3D_DSTCACHE_CTLSTAT_R3, DC_FLUSH, 3);
	cmd[4] = CP_PACKET0(RB3D_ZCACHE_CTLSTAT_R3, 0);
	cmd[5] = RB3D_ZCACHE_CTLSTAT_R3__ZC_FLUSH;
	/* try to wait but if we timeout we likely are in bad situation */
	for (i = 0; i < dev_priv->usec_timeout; i++) {
		ret = radeon_ms_ring_emit(dev, cmd, 6);
		if (!ret) {
			break;
		}
	}
}

int radeon_ms_family_init(struct drm_device *dev)
{
	struct drm_radeon_private *dev_priv = dev->dev_private;
	int i;

	dev_priv->microcode = radeon_cp_microcode;
	dev_priv->irq_emit = radeon_ms_irq_emit;

	switch (dev_priv->family) {
	case CHIP_R100:
	case CHIP_R200:
		dev_priv->microcode = radeon_cp_microcode;
		dev_priv->flush_cache = radeon_flush_cache;
		break;
	case CHIP_RV200:
	case CHIP_RV250:
	case CHIP_RV280:
	case CHIP_RS300:
		dev_priv->microcode = r200_cp_microcode;
		dev_priv->flush_cache = radeon_flush_cache;
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
		dev_priv->microcode = r300_cp_microcode;
		dev_priv->flush_cache = r300_flush_cache;
		break;
	default:
		DRM_ERROR("Unknown radeon family, aborting\n");
		return -EINVAL;
	}
	switch (dev_priv->bus_type) {
	case RADEON_AGP:
		dev_priv->create_ttm = drm_agp_init_ttm;
		dev_priv->bus_init = radeon_ms_agp_init;
		dev_priv->bus_restore = radeon_ms_agp_restore;
		dev_priv->bus_save = radeon_ms_agp_save;
		break;
	case RADEON_PCIE:
		dev_priv->create_ttm = radeon_ms_pcie_create_ttm;
		dev_priv->bus_finish = radeon_ms_pcie_finish;
		dev_priv->bus_init = radeon_ms_pcie_init;
		dev_priv->bus_restore = radeon_ms_pcie_restore;
		dev_priv->bus_save = radeon_ms_pcie_save;
		break;
	default:
		DRM_ERROR("Unknown radeon bus type, aborting\n");
		return -EINVAL;
	}
	dev_priv->properties = NULL;
	for (i = 1; i < sizeof(properties)/sizeof(properties[0]); i++) {
		if (dev->pdev->subsystem_vendor == properties[i].subvendor &&
		    dev->pdev->subsystem_device == properties[i].subdevice) {
		    DRM_INFO("[radeon_ms] found properties for 0x%04X:0x%04X\n",
		    	     properties[i].subvendor, properties[i].subdevice);
		    dev_priv->properties = &properties[i];
		}
	}
	if (dev_priv->properties == NULL) {
		dev_priv->properties = &properties[0];
	}
	return 0;
}
