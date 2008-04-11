/*
 * Copyright 2008 Jérôme Glisse
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
 *    Jerome Glisse <glisse@freedesktop.org>
 */
#include "radeon_ms.h"
#include "amd.h"

#define RADEON_DST_Y_X                      0x1438
#define RADEON_SRC_Y_X                      0x1434
#define RADEON_DP_CNTL                      0x16c0
#define RADEON_DP_GUI_MASTER_CNTL           0x146c
#define RADEON_DP_BRUSH_BKGD_CLR            0x1478
#define RADEON_DP_BRUSH_FRGD_CLR            0x147c
#define RADEON_DP_WRITE_MASK                0x16cc
#define RADEON_DST_PITCH_OFFSET             0x142c
#define RADEON_SRC_PITCH_OFFSET             0x1428
#define RADEON_DST_HEIGHT_WIDTH             0x143c

struct legacy_check
{
	/* for 2D operations */
	uint32_t                dp_gui_master_cntl;
	uint32_t                dst_offset;
	uint32_t                dst_pitch;
	struct amd_cmd_bo       *dst;
	uint32_t                dst_x;
	uint32_t                dst_y;
	uint32_t                dst_h;
	uint32_t                dst_w;
	struct amd_cmd_bo       *src;
	uint32_t                src_pitch;
	uint32_t                src_x;
	uint32_t                src_y;
};

static int check_blit(struct drm_device *dev, struct amd_cmd *cmd)
{
	struct legacy_check *legacy_check;
	long bpp, start, end;
	
	legacy_check = (struct legacy_check *)cmd->driver;
	/* check that gui master cntl have been set */
	if (legacy_check->dp_gui_master_cntl == 0xffffffff) {
		return -EINVAL;
	}
	switch ((legacy_check->dp_gui_master_cntl >> 8) & 0xf) {
	case 2:
		bpp = 1;
		break;
	case 3:
	case 4:
		bpp = 2;
		break;
	case 6:
		bpp = 4;
		break;
	default:
		return -EINVAL;
	}
	/* check that a destination have been set */
	if (legacy_check->dst == (void *)-1) {
		return -EINVAL;
	}
	if (legacy_check->dst_pitch == 0xffffffff) {
		return -EINVAL;
	}
	if (legacy_check->dst_x == 0xffffffff) {
		return -EINVAL;
	}
	if (legacy_check->dst_y == 0xffffffff) {
		return -EINVAL;
	}
	if (legacy_check->dst_w == 0xffffffff) {
		return -EINVAL;
	}
	if (legacy_check->dst_h == 0xffffffff) {
		return -EINVAL;
	}
	/* compute start offset of blit */
	start = legacy_check->dst_pitch * legacy_check->dst_y +
		legacy_check->dst_x * bpp;
	/* compute end offset of blit */
	end = legacy_check->dst_pitch * legacy_check->dst_h +
	      legacy_check->dst_w * bpp;
	/* FIXME: check that end offset is inside dst bo */

	/* check that a destination have been set */
	if (legacy_check->dp_gui_master_cntl & 1) {
		if (legacy_check->src == (void *)-1) {
			return -EINVAL;
		}
		if (legacy_check->src_pitch == 0xffffffff) {
			return -EINVAL;
		}
		if (legacy_check->src_x == 0xffffffff) {
			return -EINVAL;
		}
		if (legacy_check->src_y == 0xffffffff) {
			return -EINVAL;
		}
		/* compute start offset of blit */
		start = legacy_check->src_pitch * legacy_check->src_y +
			legacy_check->src_x * bpp;
		/* compute end offset of blit */
		end = legacy_check->src_pitch * legacy_check->dst_h +
		      legacy_check->dst_w * bpp + start;
		/* FIXME: check that end offset is inside src bo */
	}
	return 0;
}

static int p0_dp_gui_master_cntl(struct drm_device *dev,
				 struct amd_cmd *cmd,
				 int cdw_id, int reg)
{
	struct legacy_check *legacy_check;
	
	legacy_check = (struct legacy_check *)cmd->driver;
	legacy_check->dp_gui_master_cntl = cmd->cdw[cdw_id];
	/* we only accept src data type to be same as dst */
	if (((legacy_check->dp_gui_master_cntl >> 12) & 0x3) != 3) {
		return -EINVAL;
	}
	return 0;
}

static int p0_dst_pitch_offset(struct drm_device *dev,
			       struct amd_cmd *cmd,
			       int cdw_id, int reg)
{
	struct legacy_check *legacy_check;
	uint32_t tmp;
	int ret;
	
	legacy_check = (struct legacy_check *)cmd->driver;
	legacy_check->dst_pitch = ((cmd->cdw[cdw_id] >> 22) & 0xff) << 6;
	tmp = cmd->cdw[cdw_id] & 0x003fffff;
	legacy_check->dst = amd_cmd_get_bo(cmd, tmp);
	if (legacy_check->dst == NULL) {
		DRM_ERROR("invalid bo (%d) for DST_PITCH_OFFSET register.\n",
			  tmp);
		return -EINVAL;
	}
	ret = radeon_ms_bo_get_gpu_addr(dev, &legacy_check->dst->bo->mem, &tmp);
	if (ret) {
		DRM_ERROR("failed to get GPU offset for bo 0x%x.\n",
			  legacy_check->dst->handle);
		return -EINVAL;
	}
	if (tmp & 0x3fff) {
		DRM_ERROR("bo 0x%x offset doesn't meet alignement 0x%x.\n",
			  legacy_check->dst->handle, tmp);
	}
	cmd->cdw[cdw_id] &= 0xffc00000;
	cmd->cdw[cdw_id] |= (tmp >> 10);
	return 0;
}

static int p0_src_pitch_offset(struct drm_device *dev,
			       struct amd_cmd *cmd,
			       int cdw_id, int reg)
{
	struct legacy_check *legacy_check;
	uint32_t tmp;
	int ret;
	
	legacy_check = (struct legacy_check *)cmd->driver;
	legacy_check->src_pitch = ((cmd->cdw[cdw_id] >> 22) & 0xff) << 6;
	tmp = cmd->cdw[cdw_id] & 0x003fffff;
	legacy_check->src = amd_cmd_get_bo(cmd, tmp);
	if (legacy_check->src == NULL) {
		DRM_ERROR("invalid bo (%d) for SRC_PITCH_OFFSET register.\n",
			  tmp);
		return -EINVAL;
	}
	ret = radeon_ms_bo_get_gpu_addr(dev, &legacy_check->src->bo->mem, &tmp);
	if (ret) {
		DRM_ERROR("failed to get GPU offset for bo 0x%x.\n",
			  legacy_check->src->handle);
		return -EINVAL;
	}
	if (tmp & 0x3fff) {
		DRM_ERROR("bo 0x%x offset doesn't meet alignement 0x%x.\n",
			  legacy_check->src->handle, tmp);
	}
	cmd->cdw[cdw_id] &= 0xffc00000;
	cmd->cdw[cdw_id] |= (tmp >> 10);
	return 0;
}

static int p0_dst_y_x(struct drm_device *dev,
		      struct amd_cmd *cmd,
		      int cdw_id, int reg)
{
	struct legacy_check *legacy_check;
	
	legacy_check = (struct legacy_check *)cmd->driver;
	legacy_check->dst_x = cmd->cdw[cdw_id] & 0xffff;
	legacy_check->dst_y = (cmd->cdw[cdw_id] >> 16) & 0xffff;
	return 0;
}

static int p0_src_y_x(struct drm_device *dev,
		      struct amd_cmd *cmd,
		      int cdw_id, int reg)
{
	struct legacy_check *legacy_check;
	
	legacy_check = (struct legacy_check *)cmd->driver;
	legacy_check->src_x = cmd->cdw[cdw_id] & 0xffff;
	legacy_check->src_y = (cmd->cdw[cdw_id] >> 16) & 0xffff;
	return 0;
}

static int p0_dst_h_w(struct drm_device *dev,
		      struct amd_cmd *cmd,
		      int cdw_id, int reg)
{
	struct legacy_check *legacy_check;
	
	legacy_check = (struct legacy_check *)cmd->driver;
	legacy_check->dst_w = cmd->cdw[cdw_id] & 0xffff;
	legacy_check->dst_h = (cmd->cdw[cdw_id] >> 16) & 0xffff;
	return check_blit(dev, cmd);
}

static int legacy_cmd_check(struct drm_device *dev,
				struct amd_cmd *cmd)
{
	struct legacy_check legacy_check;

	memset(&legacy_check, 0xff, sizeof(struct legacy_check));
	cmd->driver = &legacy_check;
	return amd_cmd_check(dev, cmd);
}

int amd_legacy_cmd_module_destroy(struct drm_device *dev)
{
	struct drm_radeon_private *dev_priv = dev->dev_private;

	dev_priv->cmd_module.check = NULL;
	if (dev_priv->cmd_module.numof_p0_checkers) {
		drm_free(dev_priv->cmd_module.check_p0,
			 dev_priv->cmd_module.numof_p0_checkers *
			 sizeof(void*), DRM_MEM_DRIVER);
		dev_priv->cmd_module.numof_p0_checkers = 0;
	}
	if (dev_priv->cmd_module.numof_p3_checkers) {
		drm_free(dev_priv->cmd_module.check_p3,
			 dev_priv->cmd_module.numof_p3_checkers *
			 sizeof(void*), DRM_MEM_DRIVER);
		dev_priv->cmd_module.numof_p3_checkers = 0;
	}
	return 0;
}

int amd_legacy_cmd_module_initialize(struct drm_device *dev)
{
	struct drm_radeon_private *dev_priv = dev->dev_private;
	struct amd_cmd_module *checker = &dev_priv->cmd_module;
	long size;

	/* packet 0 */
	checker->numof_p0_checkers = 0x5000 >> 2;
	size = checker->numof_p0_checkers * sizeof(void*);
	checker->check_p0 = drm_alloc(size, DRM_MEM_DRIVER);
	if (checker->check_p0 == NULL) {
		amd_legacy_cmd_module_destroy(dev);
		return -ENOMEM;
	}
	/* initialize to -1 */
	memset(checker->check_p0, 0xff, size);

	/* packet 3 */
	checker->numof_p3_checkers = 20;
	size = checker->numof_p3_checkers * sizeof(void*);
	checker->check_p3 = drm_alloc(size, DRM_MEM_DRIVER);
	if (checker->check_p3 == NULL) {
		amd_legacy_cmd_module_destroy(dev);
		return -ENOMEM;
	}
	/* initialize to -1 */
	memset(checker->check_p3, 0xff, size);

	/* initialize packet0 checker */
	checker->check_p0[RADEON_DST_Y_X >> 2] = p0_dst_y_x;
	checker->check_p0[RADEON_SRC_Y_X >> 2] = p0_src_y_x;
	checker->check_p0[RADEON_DST_HEIGHT_WIDTH >> 2] = p0_dst_h_w;
	checker->check_p0[RADEON_DST_PITCH_OFFSET >> 2] = p0_dst_pitch_offset;
	checker->check_p0[RADEON_SRC_PITCH_OFFSET >> 2] = p0_src_pitch_offset;
	checker->check_p0[RADEON_DP_GUI_MASTER_CNTL>>2] = p0_dp_gui_master_cntl;
	checker->check_p0[RADEON_DP_BRUSH_FRGD_CLR >> 2] = NULL;
	checker->check_p0[RADEON_DP_WRITE_MASK >> 2] = NULL;
	checker->check_p0[RADEON_DP_CNTL >> 2] = NULL;

	checker->check = legacy_cmd_check;
	return 0;
}
