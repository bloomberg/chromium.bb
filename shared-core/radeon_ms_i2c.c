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

static int get_clock(void *data)
{
	struct radeon_ms_i2c *i2c = data;
	struct drm_radeon_private *dev_priv = i2c->drm_dev->dev_private;
	int v;

	switch (i2c->reg) {
	case VIPPAD_EN:
		v = MMIO_R(VIPPAD_Y);
		if ((REG_G(VIPPAD_Y, VIPPAD_Y_VHAD, v) & 2)) {
			v = 1;
		} else {
			v = 0;
		}
		break;
	case VIPPAD1_EN:
		v = MMIO_R(VIPPAD1_Y);
		if ((REG_G(VIPPAD1_Y, VIPPAD_Y_DVODATA, v) & 8)) {
			v = 1;
		} else {
			v = 0;
		}
		break;
	case GPIO_DDC1:
		v = MMIO_R(GPIO_DDC1);
		if ((GPIO_DDC1__DDC1_CLK_INPUT & v)) {
			v = 1;
		} else {
			v = 0;
		}
		break;
	case GPIO_DDC2:
		v = MMIO_R(GPIO_DDC2);
		if ((GPIO_DDC2__DDC2_CLK_INPUT & v)) {
			v = 1;
		} else {
			v = 0;
		}
		break;
	default:
		v = 0;
		break;
	}
	return v;
}

static int get_data(void *data)
{
	struct radeon_ms_i2c *i2c = data;
	struct drm_radeon_private *dev_priv = i2c->drm_dev->dev_private;
	int v;

	switch (i2c->reg) {
	case VIPPAD_EN:
		v = MMIO_R(VIPPAD_Y);
		if ((REG_G(VIPPAD_Y, VIPPAD_Y_VHAD, v) & 1)) {
			v = 1;
		} else {
			v = 0;
		}
		break;
	case VIPPAD1_EN:
		v = MMIO_R(VIPPAD1_Y);
		if ((REG_G(VIPPAD1_Y, VIPPAD_Y_DVODATA, v) & 4)) {
			v = 1;
		} else {
			v = 0;
		}
		break;
	case GPIO_DDC1:
		v = MMIO_R(GPIO_DDC1);
		if ((GPIO_DDC1__DDC1_DATA_INPUT & v)) {
			v = 1;
		} else {
			v = 0;
		}
		break;
	case GPIO_DDC2:
		v = MMIO_R(GPIO_DDC2);
		if ((GPIO_DDC2__DDC2_DATA_INPUT & v)) {
			v = 1;
		} else {
			v = 0;
		}
		break;
	default:
		v = 0;
		break;
	}
	return v;
}

static void set_clock(void *i2c_priv, int clock)
{
	struct radeon_ms_i2c *i2c = i2c_priv;
	struct drm_radeon_private *dev_priv = i2c->drm_dev->dev_private;
	int v, line;

	v = MMIO_R(i2c->reg);
	switch (i2c->reg) {
	case VIPPAD_EN:
		line = REG_G(VIPPAD_EN, VIPPAD_EN_VHAD, v) & ~2;
		v &= ~VIPPAD_EN__VIPPAD_EN_VHAD__MASK;
		if (!clock) {
			v |= REG_S(VIPPAD_EN, VIPPAD_EN_VHAD, line | 2);
		} else {
			v |= REG_S(VIPPAD_EN, VIPPAD_EN_VHAD, line);
		}
		break;
	case VIPPAD1_EN:
		line = REG_G(VIPPAD1_EN, VIPPAD_EN_DVODATA, v) & ~8;
		v &= ~VIPPAD1_EN__VIPPAD_EN_DVODATA__MASK;
		if (!clock) {
			v |= REG_S(VIPPAD1_EN, VIPPAD_EN_DVODATA, line | 8);
		} else {
			v |= REG_S(VIPPAD1_EN, VIPPAD_EN_DVODATA, line);
		}
		break;
	case GPIO_DDC1:
		v &= ~GPIO_DDC1__DDC1_CLK_OUT_EN;
		if (!clock) {
			v |= GPIO_DDC1__DDC1_CLK_OUT_EN;
		}
		break;
	case GPIO_DDC2:
		v &= ~GPIO_DDC2__DDC2_CLK_OUT_EN;
		if (!clock) {
			v |= GPIO_DDC2__DDC2_CLK_OUT_EN;
		}
		break;
	default:
		return;
	}
	MMIO_W(i2c->reg, v);
}

static void set_data(void *i2c_priv, int data)
{
	struct radeon_ms_i2c *i2c = i2c_priv;
	struct drm_radeon_private *dev_priv = i2c->drm_dev->dev_private;
	int v, line;

	v = MMIO_R(i2c->reg);
	switch (i2c->reg) {
	case VIPPAD_EN:
		line = REG_G(VIPPAD_EN, VIPPAD_EN_VHAD, v) & ~1;
		v &= ~VIPPAD_EN__VIPPAD_EN_VHAD__MASK;
		if (!data) {
			v |= REG_S(VIPPAD_EN, VIPPAD_EN_VHAD, line | 1);
		} else {
			v |= REG_S(VIPPAD_EN, VIPPAD_EN_VHAD, line);
		}
		break;
	case VIPPAD1_EN:
		line = REG_G(VIPPAD1_EN, VIPPAD_EN_DVODATA, v) & ~4;
		v &= ~VIPPAD1_EN__VIPPAD_EN_DVODATA__MASK;
		if (!data) {
			v |= REG_S(VIPPAD1_EN, VIPPAD_EN_DVODATA, line | 4);
		} else {
			v |= REG_S(VIPPAD1_EN, VIPPAD_EN_DVODATA, line);
		}
		break;
	case GPIO_DDC1:
		v &= ~GPIO_DDC1__DDC1_DATA_OUT_EN;
		if (!data) {
			v |= GPIO_DDC1__DDC1_DATA_OUT_EN;
		}
		break;
	case GPIO_DDC2:
		v &= ~GPIO_DDC2__DDC2_DATA_OUT_EN;
		if (!data) {
			v |= GPIO_DDC2__DDC2_DATA_OUT_EN;
		}
		break;
	default:
		return;
	}
	MMIO_W(i2c->reg, v);
}

/**
 * radeon_ms_i2c_create - instantiate an radeon i2c bus on specified GPIO reg
 * @dev: DRM device
 * @output: driver specific output device
 * @reg: GPIO reg to use
 * @name: name for this bus
 *
 * Creates and registers a new i2c bus with the Linux i2c layer, for use
 * in output probing and control (e.g. DDC or SDVO control functions).
 *
 */
struct radeon_ms_i2c *radeon_ms_i2c_create(struct drm_device *dev,
					   const uint32_t reg,
					   const char *name)
{
	struct radeon_ms_i2c *i2c;
	int ret;

	i2c = drm_alloc(sizeof(struct radeon_ms_i2c), DRM_MEM_DRIVER);
	if (i2c == NULL) {
		return NULL;
	}
	memset(i2c, 0, sizeof(struct radeon_ms_i2c));

	i2c->drm_dev = dev;
	i2c->reg = reg;
	snprintf(i2c->adapter.name, I2C_NAME_SIZE, "radeon-%s", name);
	i2c->adapter.owner = THIS_MODULE;
	/* fixme need to take a look at what its needed for */
	i2c->adapter.id = I2C_HW_B_RADEON;
	i2c->adapter.algo_data = &i2c->algo;
	i2c->adapter.dev.parent = &dev->pdev->dev;
	i2c->algo.setsda = set_data;
	i2c->algo.setscl = set_clock;
	i2c->algo.getsda = get_data;
	i2c->algo.getscl = get_clock;
	i2c->algo.udelay = 20;
	i2c->algo.timeout = usecs_to_jiffies(2200);
	i2c->algo.data = i2c;

	i2c_set_adapdata(&i2c->adapter, i2c);

	ret = i2c_bit_add_bus(&i2c->adapter);
	if(ret) {
		DRM_INFO("[radeon_ms] failed to register I2C '%s' bus\n",
			 i2c->adapter.name);
		goto out_free;
	}
	DRM_INFO("[radeon_ms] registered I2C '%s' bus\n", i2c->adapter.name);
	return i2c;

out_free:
	drm_free(i2c, sizeof(struct radeon_ms_i2c), DRM_MEM_DRIVER);
	return NULL;
}

/**
 * radeon_ms_i2c_destroy - unregister and free i2c bus resources
 * @output: channel to free
 *
 * Unregister the adapter from the i2c layer, then free the structure.
 */
void radeon_ms_i2c_destroy(struct radeon_ms_i2c *i2c)
{
	if (i2c == NULL) {
		return;
	}
	i2c_del_adapter(&i2c->adapter);
	drm_free(i2c, sizeof(struct radeon_ms_i2c), DRM_MEM_DRIVER);
}
