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
 *
 * Authors:
 *    Jerome Glisse <glisse@freedesktop.org>
 */
#ifndef __AMD_H__
#define __AMD_H__

/* struct amd_cbuffer are for command buffer, this is the structure passed
 * around during command validation (ie check that user have the right to
 * execute the given command).
 */
struct amd_cmd_bo
{
	struct list_head            list;
	uint64_t                    *offsets;
	uint32_t                    *cdw_id;
	struct drm_buffer_object    *bo;
	unsigned int                handle;
	uint64_t                    mask;
	uint64_t                    flags;
	uint32_t                    type;
};

struct amd_cmd
{
	uint32_t                    *cdw;
	uint32_t                    cdw_count;
	struct drm_bo_kmap_obj      cdw_kmap;
	size_t                      cdw_size;
	struct amd_cmd_bo           *cdw_bo;
	struct amd_cmd_bo           bo_unused;
	struct amd_cmd_bo           bo_used;
	struct amd_cmd_bo           *bo;
	uint32_t                    bo_count;
	void                        *driver;
};

struct amd_cmd_module
{
	uint32_t numof_p0_checkers;
	uint32_t numof_p3_checkers;
	int (*check)(struct drm_device *dev, struct amd_cmd *cmd);
	int (**check_p0)(struct drm_device *dev, struct amd_cmd *cmd,
			 int cdw_id, int reg);
	int (**check_p3)(struct drm_device *dev, struct amd_cmd *cmd,
			 int cdw_id, int op, int count);
};

int amd_cmd_check(struct drm_device *dev, struct amd_cmd *cmd);
int amd_ioctl_cmd(struct drm_device *dev, void *data, struct drm_file *file);

static inline struct amd_cmd_bo *amd_cmd_get_bo(struct amd_cmd *cmd, int i)
{
	if (i < cmd->bo_count && cmd->bo[i].type == DRM_AMD_CMD_BO_TYPE_DATA) {
		list_del(&cmd->bo[i].list);
		list_add_tail(&cmd->bo[i].list, &cmd->bo_used.list);
		return &cmd->bo[i]; 
	}
	return NULL;
}

/* struct amd_fb amd is for storing amd framebuffer informations
 */
struct amd_fb
{
	struct drm_device       *dev;
	struct drm_crtc         *crtc;
	struct drm_display_mode *fb_mode;
	struct drm_framebuffer  *fb;
};

#endif
