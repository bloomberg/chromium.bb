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
 *    Jérôme Glisse <glisse@freedesktop.org>
 */
#include "radeon_ms.h"

extern struct radeon_ms_output radeon_ms_dac1;
extern struct radeon_ms_output radeon_ms_dac2;
extern const struct drm_output_funcs radeon_ms_output_funcs;

static struct combios_connector_chip_info *
radeon_ms_combios_get_connector_chip_info(struct drm_device *dev, int chip_num)
{
	struct drm_radeon_private *dev_priv = dev->dev_private;
	struct radeon_ms_rom *rom = &dev_priv->rom;
	struct combios_header *header;
	struct combios_connector_table *connector_table;
	struct combios_connector_chip_info *connector_chip_info;
	uint32_t offset;
	int numof_chips, i;

	if (rom->type != ROM_COMBIOS || rom->rom_image == NULL) {
		return NULL;
	}
	header = rom->rom.combios_header;
	offset = header->usPointerToExtendedInitTable2;
	if ((offset + sizeof(struct combios_connector_table)) > rom->rom_size) {
		DRM_INFO("[radeon_ms] wrong COMBIOS connector offset\n");
		return NULL;
	}
	if (!offset) {
		return NULL;
	}
	connector_table = (struct combios_connector_table *)
	                  &rom->rom_image[offset];
	numof_chips = (connector_table->ucConnectorHeader &
		       BIOS_CONNECTOR_HEADER__NUMBER_OF_CHIPS__MASK) >>
		      BIOS_CONNECTOR_HEADER__NUMBER_OF_CHIPS__SHIFT;
	DRM_INFO("[radeon_ms] COMBIOS number of chip: %d (table rev: %d)\n",
		 numof_chips,
		 (connector_table->ucConnectorHeader &
		  BIOS_CONNECTOR_HEADER__TABLE_REVISION__MASK) >>
		 BIOS_CONNECTOR_HEADER__TABLE_REVISION__SHIFT);
	for (i = 0; i < numof_chips; i++) {
		int chip;

		connector_chip_info = &connector_table->sChipConnectorInfo[i];
		chip = (connector_chip_info->ucChipHeader &
			BIOS_CHIPINFO_HEADER__CHIP_NUMBER__MASK) >>
		       BIOS_CHIPINFO_HEADER__CHIP_NUMBER__SHIFT;
		DRM_INFO("[radeon_ms] COMBIOS chip: %d (asked for: %d)\n",
		         chip, chip_num);
		if (chip == chip_num) {
			return connector_chip_info;
		}
	}
	return NULL;
}

static int radeon_combios_get_connector_infos(struct drm_device *dev,
					      int connector_info, 
					      int *connector_type, 
					      int *ddc_line,
					      int *tmds_type,
					      int *dac_type)
{
	struct drm_radeon_private *dev_priv = dev->dev_private;

	*connector_type = (connector_info & BIOS_CONNECTOR_INFO__TYPE__MASK) >>
			  BIOS_CONNECTOR_INFO__TYPE__SHIFT;
	*ddc_line = (connector_info & BIOS_CONNECTOR_INFO__DDC_LINE__MASK) >>
		    BIOS_CONNECTOR_INFO__DDC_LINE__SHIFT;
	*tmds_type = (connector_info & BIOS_CONNECTOR_INFO__TMDS_TYPE__MASK) >>
		     BIOS_CONNECTOR_INFO__TMDS_TYPE__SHIFT;
	*dac_type = (connector_info & BIOS_CONNECTOR_INFO__DAC_TYPE__MASK) >>
		    BIOS_CONNECTOR_INFO__DAC_TYPE__SHIFT;

	/* most XPRESS chips seem to specify DDC_CRT2 for their 
	 * VGA DDC port, however DDC never seems to work on that
	 * port.  Some have reported success on DDC_MONID, so 
	 * lets see what happens with that.
	 */
	if (dev_priv->family == CHIP_RS400 &&
	    *connector_type == BIOS_CONNECTOR_TYPE__CRT &&
	    *ddc_line == BIOS_DDC_LINE__CRT2) {
		*ddc_line = BIOS_DDC_LINE__MONID01;
	}
	/* XPRESS desktop chips seem to have a proprietary
	 * connector listed for DVI-D, try and do the right
	 * thing here.
	 */
	if (dev_priv->family == CHIP_RS400 &&
	    *connector_type == BIOS_CONNECTOR_TYPE__PROPRIETARY) {
		DRM_INFO("[radeon_ms] COMBIOS Proprietary connector "
		         "found, assuming DVI-D\n");
		*dac_type = 2;
	        *tmds_type = BIOS_TMDS_TYPE__EXTERNAL;
		*connector_type = BIOS_CONNECTOR_TYPE__DVI_D;
	}
	return 0;
}

static int radeon_ms_combios_connector_add(struct drm_device *dev,
					   int connector_number,
					   int connector_type,
					   uint32_t i2c_reg)
{
	struct drm_radeon_private *dev_priv = dev->dev_private;
	struct radeon_ms_connector *connector = NULL;
	struct drm_output *output = NULL;

	connector = drm_alloc(sizeof(struct radeon_ms_connector),
			      DRM_MEM_DRIVER);
	if (connector == NULL) {
		radeon_ms_connectors_destroy(dev);
		return -ENOMEM;
	}
	memset(connector, 0, sizeof(struct radeon_ms_connector));
	connector->monitor_type = MT_NONE;
	connector->type = connector_type;
	connector->i2c_reg = i2c_reg;

	switch (connector->type) {
	case CONNECTOR_VGA:
		sprintf(connector->name, "VGA");
		break;
	case CONNECTOR_DVI_I:
		sprintf(connector->name, "DVI-I");
		break;
	case CONNECTOR_DVI_D:
		sprintf(connector->name, "DVI-D");
		break;
	default:
		sprintf(connector->name, "UNKNOWN-CONNECTOR");
		break;
	}

	if (i2c_reg) {
		connector->i2c = radeon_ms_i2c_create(dev,
						      connector->i2c_reg,
						      connector->name);
		if (connector->i2c == NULL) {
			radeon_ms_connectors_destroy(dev);
			return -ENOMEM;
		}
	} else {
		connector->i2c = NULL;
	}

	output = drm_output_create(dev, &radeon_ms_output_funcs,
				   connector->type);
	if (output == NULL) {
		radeon_ms_connectors_destroy(dev);
		return -EINVAL;
	}
	connector->output = output;
	output->driver_private = connector;
	output->possible_crtcs = 0x3;
	dev_priv->connectors[connector_number] = connector;
	return 0;
}

int radeon_ms_combios_get_properties(struct drm_device *dev)
{
	struct drm_radeon_private *dev_priv = dev->dev_private;
	struct radeon_ms_rom *rom = &dev_priv->rom;
	struct combios_pll_block *pll_block;
	struct combios_header *header;
	uint32_t offset;

	if (rom->type != ROM_COMBIOS || rom->rom_image == NULL) {
		return 0;
	}
	header = rom->rom.combios_header;
	offset = header->usPointerToPllInfoBlock;
	if ((offset + sizeof(struct combios_pll_block)) > rom->rom_size) {
		DRM_INFO("[radeon_ms] wrong COMBIOS pll block offset\n");
		return 0;
	}
	if (!offset) {
		return 0;
	}
	pll_block = (struct combios_pll_block *)&rom->rom_image[offset];
	dev_priv->properties.pll_reference_freq = pll_block->usDotClockRefFreq;
	dev_priv->properties.pll_reference_div = pll_block->usDotClockRefDiv;
	dev_priv->properties.pll_min_pll_freq = pll_block->ulDotClockMinFreq;
	dev_priv->properties.pll_max_pll_freq = pll_block->ulDotClockMaxFreq;
	dev_priv->properties.pll_reference_freq *= 10;
	dev_priv->properties.pll_min_pll_freq *= 10;
	dev_priv->properties.pll_max_pll_freq *= 10;
	DRM_INFO("[radeon_ms] COMBIOS pll reference frequency : %d\n",
		 dev_priv->properties.pll_reference_freq);
	DRM_INFO("[radeon_ms] COMBIOS pll reference divider   : %d\n",
		 dev_priv->properties.pll_reference_div);
	DRM_INFO("[radeon_ms] COMBIOS pll minimum frequency   : %d\n",
		 dev_priv->properties.pll_min_pll_freq);
	DRM_INFO("[radeon_ms] COMBIOS pll maximum frequency   : %d\n",
		 dev_priv->properties.pll_max_pll_freq);
	return 1;
}

int radeon_ms_connectors_from_combios(struct drm_device *dev)
{
	struct drm_radeon_private *dev_priv = dev->dev_private;
	struct combios_connector_chip_info *connector_chip_info;
	int connector_type, ddc_line, tmds_type, dac_type;
	int dac1, dac2, tmdsint, tmdsext;
	int numof_connector, i, c = 0, added, j;
	uint32_t i2c_reg;
	int ret;

	dac1 = dac2 = tmdsint = tmdsext = -1;
	connector_chip_info = radeon_ms_combios_get_connector_chip_info(dev, 1);
	if (connector_chip_info == NULL) {
		return -1;
	}
	numof_connector = (connector_chip_info->ucChipHeader &
			   BIOS_CHIPINFO_HEADER__NUMBER_OF_CONNECTORS__MASK) >>
			  BIOS_CHIPINFO_HEADER__NUMBER_OF_CONNECTORS__SHIFT;
	DRM_INFO("[radeon_ms] COMBIOS number of connector: %d\n",
	         numof_connector);
	for (i = 0; i < numof_connector; i++) {
		int connector_info = connector_chip_info->sConnectorInfo[i];

		ret = radeon_combios_get_connector_infos(dev,
							 connector_info, 
							 &connector_type, 
							 &ddc_line,
							 &tmds_type,
							 &dac_type);

		switch (ddc_line) {
		case BIOS_DDC_LINE__MONID01:
			i2c_reg = GPIO_MONID;
			break;
		case BIOS_DDC_LINE__DVI:
			i2c_reg =  GPIO_DVI_DDC;
			break;
		case BIOS_DDC_LINE__VGA:
			i2c_reg = GPIO_DDC1;
			break;
		case BIOS_DDC_LINE__CRT2:
			i2c_reg = GPIO_CRT2_DDC;
			break;
		case BIOS_DDC_LINE__GPIOPAD:
			i2c_reg = VIPPAD_EN;
			break;
		case BIOS_DDC_LINE__ZV_LCDPAD:
			i2c_reg = VIPPAD1_EN;
			break;
		default:
			i2c_reg = 0;
			break;
		}
		added = 0;
		switch (connector_type) {
		case BIOS_CONNECTOR_TYPE__CRT:
			ret = radeon_ms_combios_connector_add(dev, c,
							      CONNECTOR_VGA,
							      i2c_reg);
			if (ret) {
				return ret;
			}
			added = 1;
			break;
		case BIOS_CONNECTOR_TYPE__DVI_I:
			ret = radeon_ms_combios_connector_add(dev, c,
							      CONNECTOR_DVI_I,
							      i2c_reg);
			if (ret) {
				return ret;
			}
			added = 1;
			break;
		case BIOS_CONNECTOR_TYPE__DVI_D:
			ret = radeon_ms_combios_connector_add(dev, c,
							      CONNECTOR_DVI_D,
							      i2c_reg);
			if (ret) {
				return ret;
			}
			added = 1;
			break;
		default:
			break;
		}
		if (added) {
			j = 0;
			/* find to which output this connector is associated 
			 * by following same algo as in:
			 * radeon_ms_outputs_from_combios*/
			switch (dac_type) {
			case BIOS_DAC_TYPE__CRT:
				if (dac1 == -1) {
					dac1 = c;
				}
				dev_priv->connectors[c]->outputs[j++] = dac1;
				break;
			case BIOS_DAC_TYPE__NON_CRT:
				if (dac2 == -1) {
					dac2 = c;
				}
				dev_priv->connectors[c]->outputs[j++] = dac2;
				break;
			}
#if 0
			switch (tmds_type) {
			case BIOS_TMDS_TYPE__INTERNAL:
				if (tmdsint == -1) {
					tmdsint = c;
				}
				dev_priv->connectors[c]->outputs[j++] = tmdsint;
				break;
			case BIOS_TMDS_TYPE__EXTERNAL:
				if (tmdsext == -1) {
					tmdsext = c;
				}
				dev_priv->connectors[c]->outputs[j++] = tmdsext;
				break;
			}
#endif
			c++;
		}
	}
	return c;
}

int radeon_ms_outputs_from_combios(struct drm_device *dev)
{
	struct drm_radeon_private *dev_priv = dev->dev_private;
	struct combios_connector_chip_info *connector_chip_info;
	int connector_type, ddc_line, tmds_type, dac_type;
	int numof_connector, i, dac1_present, dac2_present, c = 0;
	int ret;

	dac1_present = dac2_present = 0;
	connector_chip_info = radeon_ms_combios_get_connector_chip_info(dev, 1);
	if (connector_chip_info == NULL) {
		return -1;
	}
	numof_connector = (connector_chip_info->ucChipHeader &
			   BIOS_CHIPINFO_HEADER__NUMBER_OF_CONNECTORS__MASK) >>
			  BIOS_CHIPINFO_HEADER__NUMBER_OF_CONNECTORS__SHIFT;
	DRM_INFO("[radeon_ms] COMBIOS number of connector: %d\n",
	         numof_connector);
	for (i = 0; i < numof_connector; i++) {
		int connector_info = connector_chip_info->sConnectorInfo[i];

		ret = radeon_combios_get_connector_infos(dev,
							 connector_info, 
							 &connector_type, 
							 &ddc_line,
							 &tmds_type,
							 &dac_type);

		if (!dac1_present && dac_type == BIOS_DAC_TYPE__CRT) {
			dev_priv->outputs[c] =
				drm_alloc(sizeof(struct radeon_ms_output),
					  DRM_MEM_DRIVER);
			if (dev_priv->outputs[c] == NULL) {
				radeon_ms_outputs_destroy(dev);
				return -ENOMEM;
			}
			memcpy(dev_priv->outputs[c], &radeon_ms_dac1,
			       sizeof(struct radeon_ms_output));
			dev_priv->outputs[c]->dev = dev;
			dev_priv->outputs[c]->initialize(dev_priv->outputs[c]);
			dac1_present = 1;
			c++;
		}
		if (!dac2_present && dac_type == BIOS_DAC_TYPE__NON_CRT) {
			dev_priv->outputs[c] =
				drm_alloc(sizeof(struct radeon_ms_output),
					  DRM_MEM_DRIVER);
			if (dev_priv->outputs[c] == NULL) {
				radeon_ms_outputs_destroy(dev);
				return -ENOMEM;
			}
			memcpy(dev_priv->outputs[c], &radeon_ms_dac2,
			       sizeof(struct radeon_ms_output));
			dev_priv->outputs[c]->dev = dev;
			dev_priv->outputs[c]->initialize(dev_priv->outputs[c]);
			dac1_present = 1;
			c++;
		}
	}
	return c;
}
