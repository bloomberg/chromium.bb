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

struct radeon_ms_output radeon_ms_dac1 = {
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

struct radeon_ms_output radeon_ms_dac2 = {
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

struct radeon_ms_connector radeon_ms_vga = {
	NULL, NULL, NULL, ConnectorVGA, MT_NONE, 0, GPIO_DDC1,
	{
		0, -1, -1, -1, -1, -1, -1, -1
	}
};

struct radeon_ms_connector radeon_ms_dvi_i_2 = {
	NULL, NULL, NULL, ConnectorDVII, MT_NONE, 0, GPIO_DDC2,
	{
		1, -1, -1, -1, -1, -1, -1, -1
	}
};

struct radeon_ms_properties properties[] = {
	/* default only one VGA connector */
	{
		0, 0, 27000, 12, 25000, 200000, 1, 1, 1, 1,
		{
			&radeon_ms_dac1, NULL, NULL, NULL, NULL, NULL, NULL,
			NULL
		},
		{
			&radeon_ms_vga, NULL, NULL, NULL, NULL, NULL, NULL,
			NULL
		}
	}
};

int radeon_ms_properties_init(struct drm_device *dev)
{
	struct drm_radeon_private *dev_priv = dev->dev_private;
	int i, ret;

	for (i = 1; i < sizeof(properties)/sizeof(properties[0]); i++) {
		if (dev->pdev->subsystem_vendor == properties[i].subvendor &&
		    dev->pdev->subsystem_device == properties[i].subdevice) {
			DRM_INFO("[radeon_ms] found properties for "
			         "0x%04X:0x%04X\n", properties[i].subvendor,
				 properties[i].subdevice);
			memcpy(&dev_priv->properties, &properties[i],
			       sizeof(struct radeon_ms_properties));
		}
	}
	if (dev_priv->properties.subvendor == 0) {
		ret = radeon_ms_rom_get_properties(dev);
		if (ret < 0) {
			return ret;
		}
		if (!ret) {
			memcpy(&dev_priv->properties, &properties[0],
			       sizeof(struct radeon_ms_properties));
		} else {
			dev_priv->properties.pll_dummy_reads = 1;
			dev_priv->properties.pll_delay = 1;
			dev_priv->properties.pll_r300_errata = 1;
		}
		dev_priv->properties.subvendor = dev->pdev->subsystem_vendor;
		dev_priv->properties.subdevice = dev->pdev->subsystem_device;
	}
	return 0;
}
