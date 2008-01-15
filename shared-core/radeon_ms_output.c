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

static struct radeon_ms_output *radeon_ms_connector_get_output(
		struct drm_radeon_private *dev_priv,
		struct radeon_ms_connector *connector, int i)
{
	if (connector->outputs[i] < 0) {
		return NULL;
	}
	if (connector->outputs[i] >= RADEON_MAX_OUTPUTS) {
		return NULL;
	}
	i = connector->outputs[i];
	if (dev_priv->outputs[i] == NULL) {
		return NULL;
	}
	if (dev_priv->outputs[i]->connector == NULL) {
		return dev_priv->outputs[i];
	}
	if (dev_priv->outputs[i]->connector == connector) {
		return dev_priv->outputs[i];
	}
	return NULL;
}

static void radeon_ms_output_dpms(struct drm_output *output, int mode)
{
	struct drm_radeon_private *dev_priv = output->dev->dev_private;
	struct radeon_ms_connector *connector = output->driver_private;
	struct radeon_ms_output *routput = NULL;
	int i;

	if (connector == NULL) {
		return;
	}
	for (i = 0; i < RADEON_MAX_OUTPUTS; i++) {
		routput = radeon_ms_connector_get_output(dev_priv,
				connector, i);

		if (routput) {
			routput->connector = connector;
			routput->dpms(routput, mode);
		}
	}
	radeon_ms_gpu_dpms(output->dev);
}

static int radeon_ms_output_mode_valid(struct drm_output *output,
		struct drm_display_mode *mode)
{
	struct radeon_ms_connector *connector = output->driver_private;

	if (connector == NULL) {
		return MODE_ERROR;
	}
	return MODE_OK;
}

static bool radeon_ms_output_mode_fixup(struct drm_output *output,
		struct drm_display_mode *mode,
		struct drm_display_mode *adjusted_mode)
{
	return true;
}

static void radeon_ms_output_prepare(struct drm_output *output)
{
	if (output->funcs->dpms) {
		output->funcs->dpms(output, DPMSModeOff);
	}
}

static void radeon_ms_output_commit(struct drm_output *output)
{
	if (output->funcs->dpms) {
		output->funcs->dpms(output, DPMSModeOn);
	}
}

static void radeon_ms_output_mode_set(struct drm_output *output,
		struct drm_display_mode *mode,
		struct drm_display_mode *adjusted_mode)
{
	struct drm_radeon_private *dev_priv = output->dev->dev_private;
	struct radeon_ms_connector *connector = output->driver_private;
	struct radeon_ms_crtc *crtc;
	struct radeon_ms_output *routput = NULL;
	int i;

	if (connector == NULL) {
		return;
	}
	if (output->crtc == NULL) {
		return;
	}
	crtc = output->crtc->driver_private;
	connector->crtc = crtc->crtc;
	/* catch unknown crtc */
	switch (connector->crtc) {
		case 1:
		case 2:
			break;
		default:
			/* error */
			return;
	}
	for (i = 0; i < RADEON_MAX_OUTPUTS; i++) {
		routput = radeon_ms_connector_get_output(dev_priv,
				connector, i);
		if (routput) {
			routput->connector = connector;
			routput->mode_set(routput, mode, adjusted_mode);
		}
	}
}

static enum drm_output_status radeon_ms_output_detect(struct drm_output *output)
{
	struct radeon_ms_connector *connector = output->driver_private;

	if (connector == NULL || connector->i2c == NULL) {
		return output_status_unknown;
	}
	kfree(connector->edid);
	connector->edid = drm_get_edid(output, &connector->i2c->adapter);
	if (connector->edid == NULL) {
		return output_status_unknown;
	}
	return output_status_connected;
}

static int radeon_ms_output_get_modes(struct drm_output *output)
{
	struct radeon_ms_connector *connector = output->driver_private;
	int ret = 0;

	if (connector == NULL || connector->i2c == NULL) {
		return 0;
	}
	if (connector->edid == NULL) {
		return 0;
	}
	drm_mode_output_update_edid_property(output, connector->edid);
	ret = drm_add_edid_modes(output, connector->edid);
	kfree(connector->edid);
	connector->edid = NULL;
	return ret;
}

static void radeon_ms_output_cleanup(struct drm_output *output)
{
	struct radeon_ms_connector *connector = output->driver_private;

	if (connector == NULL) {
		return;
	}
	if (connector->edid) {
		kfree(connector->edid);
	}
	connector->edid = NULL;
	connector->output = NULL;
	output->driver_private = NULL;
}

const struct drm_output_funcs radeon_ms_output_funcs = {
	.dpms = radeon_ms_output_dpms,
	.save = NULL,
	.restore = NULL,
	.mode_valid = radeon_ms_output_mode_valid,
	.mode_fixup = radeon_ms_output_mode_fixup,
	.prepare = radeon_ms_output_prepare,
	.mode_set = radeon_ms_output_mode_set,
	.commit = radeon_ms_output_commit,
	.detect = radeon_ms_output_detect,
	.get_modes = radeon_ms_output_get_modes,
	.cleanup = radeon_ms_output_cleanup,
};

void radeon_ms_connectors_destroy(struct drm_device *dev)
{
	struct drm_radeon_private *dev_priv = dev->dev_private;
	struct radeon_ms_connector *connector = NULL;
	int i = 0;

	for (i = 0; i < RADEON_MAX_CONNECTORS; i++) {
		if (dev_priv->connectors[i]) {
			connector = dev_priv->connectors[i];
			dev_priv->connectors[i] = NULL;
			if (connector->output) {
				drm_output_destroy(connector->output);
				connector->output = NULL;
			}
			if (connector->i2c) {
				radeon_ms_i2c_destroy(connector->i2c);
				connector->i2c = NULL;
			}
			drm_free(connector,
					sizeof(struct radeon_ms_connector),
					DRM_MEM_DRIVER);
		}
	}
}

int radeon_ms_connectors_from_properties(struct drm_device *dev)
{
	struct drm_radeon_private *dev_priv = dev->dev_private;
	struct radeon_ms_connector *connector = NULL;
	struct drm_output *output = NULL;
	int i = 0, c = 0;

	radeon_ms_connectors_destroy(dev);
	for (i = 0; i < RADEON_MAX_CONNECTORS; i++) {
		if (dev_priv->properties.connectors[i]) {
			connector =
				drm_alloc(sizeof(struct radeon_ms_connector),
						DRM_MEM_DRIVER);
			if (connector == NULL) {
				radeon_ms_connectors_destroy(dev);
				return -ENOMEM;
			}
			memcpy(connector, dev_priv->properties.connectors[i],
			       sizeof(struct radeon_ms_connector));
			connector->i2c =
				radeon_ms_i2c_create(dev, connector->i2c_reg,
						     connector->name);
			if (connector->i2c == NULL) {
				radeon_ms_connectors_destroy(dev);
				return -ENOMEM;
			}
			output = drm_output_create(dev,
					&radeon_ms_output_funcs,
					connector->name);
			if (output == NULL) {
				radeon_ms_connectors_destroy(dev);
				return -EINVAL;
			}
			connector->output = output;
			output->driver_private = connector;
			output->possible_crtcs = 0x3;
			dev_priv->connectors[c++] = connector;
		}
	}
	return c;
}

int radeon_ms_connectors_from_rom(struct drm_device *dev)
{
	struct drm_radeon_private *dev_priv = dev->dev_private;

	switch (dev_priv->rom.type) {
	case ROM_COMBIOS:
		return radeon_ms_connectors_from_combios(dev);
	}
	return 0;
}

void radeon_ms_outputs_destroy(struct drm_device *dev)
{
	struct drm_radeon_private *dev_priv = dev->dev_private;
	int i = 0;

	for (i = 0; i < RADEON_MAX_OUTPUTS; i++) {
		if (dev_priv->outputs[i]) {
			drm_free(dev_priv->outputs[i],
					sizeof(struct radeon_ms_output),
					DRM_MEM_DRIVER);
			dev_priv->outputs[i] = NULL;
		}
	}
}

int radeon_ms_outputs_from_properties(struct drm_device *dev)
{
	struct drm_radeon_private *dev_priv = dev->dev_private;
	int i = 0;
	int c = 0;

	radeon_ms_outputs_destroy(dev);
	for (i = 0; i < RADEON_MAX_OUTPUTS; i++) {
		if (dev_priv->properties.outputs[i]) {
			dev_priv->outputs[i] =
				drm_alloc(sizeof(struct radeon_ms_output),
					  DRM_MEM_DRIVER);
			if (dev_priv->outputs[i] == NULL) {
				radeon_ms_outputs_destroy(dev);
				return -ENOMEM;
			}
			memcpy(dev_priv->outputs[i],
			       dev_priv->properties.outputs[i],
			       sizeof(struct radeon_ms_output));
			dev_priv->outputs[i]->dev = dev;
			dev_priv->outputs[i]->initialize(dev_priv->outputs[i]);
			c++;
		}
	}
	return c;
}

int radeon_ms_outputs_from_rom(struct drm_device *dev)
{
	struct drm_radeon_private *dev_priv = dev->dev_private;

	switch (dev_priv->rom.type) {
	case ROM_COMBIOS:
		return radeon_ms_outputs_from_combios(dev);
	}
	return 0;
}

void radeon_ms_outputs_restore(struct drm_device *dev,
		struct radeon_state *state)
{
	struct drm_radeon_private *dev_priv = dev->dev_private;
	int i;

	for (i = 0; i < RADEON_MAX_OUTPUTS; i++) {
		if (dev_priv->outputs[i]) {
			dev_priv->outputs[i]->restore(dev_priv->outputs[i],
					state);
		}
	}
}

void radeon_ms_outputs_save(struct drm_device *dev, struct radeon_state *state)
{
	struct drm_radeon_private *dev_priv = dev->dev_private;
	int i;

	for (i = 0; i < RADEON_MAX_OUTPUTS; i++) {
		if (dev_priv->outputs[i]) {
			dev_priv->outputs[i]->save(dev_priv->outputs[i], state);
		}
	}
}
