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
#include "radeon_ms.h"
#include "amd.h"

static inline void amd_cmd_bo_cleanup(struct drm_device *dev,
				      struct amd_cmd *cmd)
{
	struct amd_cmd_bo *bo;

	mutex_lock(&dev->struct_mutex);
	list_for_each_entry(bo, &cmd->bo_unused.list, list) {
		drm_bo_usage_deref_locked(&bo->bo);
	}
	list_for_each_entry(bo, &cmd->bo_used.list, list) {
		drm_bo_usage_deref_locked(&bo->bo);
	}
	mutex_unlock(&dev->struct_mutex);
}

static inline int amd_cmd_bo_validate(struct drm_device *dev,
				      struct drm_file *file,
				      struct amd_cmd_bo *cmd_bo,
				      struct drm_amd_cmd_bo *bo,
				      uint64_t data)
{
	int ret;

	/* validate only cmd indirect or data bo */
	switch (bo->type) {
	case DRM_AMD_CMD_BO_TYPE_CMD_INDIRECT:
	case DRM_AMD_CMD_BO_TYPE_DATA:
	case DRM_AMD_CMD_BO_TYPE_CMD_RING:
		/* FIXME: make sure userspace can no longer map the bo */
		break;
	default:
		return 0;
	}
	/* check that buffer operation is validate */
	if (bo->op_req.op != drm_bo_validate) {
		DRM_ERROR("buffer 0x%x object operation is not validate.\n",
			  cmd_bo->handle);
		return -EINVAL;
	}
	/* validate buffer */
	memset(&bo->op_rep, 0, sizeof(struct drm_bo_arg_rep));
	ret = drm_bo_handle_validate(file,
				     bo->op_req.bo_req.handle,
				     bo->op_req.bo_req.flags,
				     bo->op_req.bo_req.mask,
				     bo->op_req.bo_req.hint,
				     bo->op_req.bo_req.fence_class,
				     0,
				     &bo->op_rep.bo_info,
				     &cmd_bo->bo);
	if (ret) {
		DRM_ERROR("validate error %d for 0x%08x\n",
			  ret, cmd_bo->handle);
		return ret;
	}
	if (copy_to_user((void __user *)((unsigned)data), bo,
			 sizeof(struct drm_amd_cmd_bo))) {
		DRM_ERROR("failed to copy to user validate result of 0x%08x\n",
			  cmd_bo->handle);
		return -EFAULT;
	}
	return 0;
}

static int amd_cmd_parse_cmd_bo(struct drm_device *dev,
				struct drm_file *file,
				struct drm_amd_cmd *drm_amd_cmd,
				struct amd_cmd *cmd)
{
	struct drm_amd_cmd_bo drm_amd_cmd_bo;
	struct amd_cmd_bo *cmd_bo;
	uint32_t bo_count = 0;
	uint64_t data = drm_amd_cmd->bo;
	int ret = 0;

	do {
		/* check we don't have more buffer than announced */
		if (bo_count >= drm_amd_cmd->bo_count) {
			DRM_ERROR("cmd bo count exceeded got %d waited %d\n.",
				  bo_count, drm_amd_cmd->bo_count);
			return -EINVAL;
		}
		/* initialize amd_cmd_bo */
		cmd_bo = &cmd->bo[bo_count];
		INIT_LIST_HEAD(&cmd_bo->list);
		cmd_bo->bo = NULL;
		/* copy from userspace */
		if (copy_from_user(&drm_amd_cmd_bo,
				   (void __user *)((unsigned)data),
				   sizeof(struct drm_amd_cmd_bo))) {
			return -EFAULT;
		}
		/* collect informations */
		cmd_bo->type = drm_amd_cmd_bo.type;
		cmd_bo->mask = drm_amd_cmd_bo.op_req.bo_req.mask;
		cmd_bo->flags = drm_amd_cmd_bo.op_req.bo_req.flags;
		cmd_bo->handle = drm_amd_cmd_bo.op_req.arg_handle;
		/* get bo objects */
		mutex_lock(&dev->struct_mutex);
		cmd_bo->bo = drm_lookup_buffer_object(file, cmd_bo->handle, 1);
		mutex_unlock(&dev->struct_mutex);
		if (cmd_bo->bo == NULL) {
			DRM_ERROR("unknown bo handle 0x%x\n", cmd_bo->handle);
			return -EINVAL;
		}
		/* validate buffer if necessary */
		ret = amd_cmd_bo_validate(dev, file, cmd_bo,
					  &drm_amd_cmd_bo, data);
		if (ret) {
			mutex_lock(&dev->struct_mutex);
			drm_bo_usage_deref_locked(&cmd_bo->bo);
			mutex_unlock(&dev->struct_mutex);
			return ret;
		}
		/* inspect bo type */
		switch (cmd_bo->type) {
		case DRM_AMD_CMD_BO_TYPE_CMD_INDIRECT:
			/* add it so we properly unreference in case of error */
			list_add_tail(&cmd_bo->list, &cmd->bo_used.list);
			return -EINVAL;
		case DRM_AMD_CMD_BO_TYPE_DATA:
			/* add to unused list */
			list_add_tail(&cmd_bo->list, &cmd->bo_unused.list);
			break;
		case DRM_AMD_CMD_BO_TYPE_CMD_RING:
			/* set cdw_bo */
			list_add_tail(&cmd_bo->list, &cmd->bo_used.list);
			cmd->cdw_bo = cmd_bo;
			break;
		default:
			mutex_lock(&dev->struct_mutex);
			drm_bo_usage_deref_locked(&cmd_bo->bo);
			mutex_unlock(&dev->struct_mutex);
			DRM_ERROR("unknow bo 0x%x unknown type 0x%x in cmd\n",
				  cmd_bo->handle, cmd_bo->type);
			return -EINVAL;
		}
		/* ok next bo */
		data = drm_amd_cmd_bo.next;
		bo_count++;
	} while (data != 0);
	if (bo_count != drm_amd_cmd->bo_count) {
		DRM_ERROR("not enought buffer got %d expected %d\n.",
			  bo_count, drm_amd_cmd->bo_count);
		return -EINVAL;
	}
	return 0;
}

static int amd_cmd_packet0_check(struct drm_device *dev,
				 struct amd_cmd *cmd,
				 int *cdw_id)
{
	struct drm_radeon_private *dev_priv = dev->dev_private;
	uint32_t reg, count, r, i;
	int ret;

	reg = cmd->cdw[*cdw_id] & PACKET0_REG_MASK;
	count = (cmd->cdw[*cdw_id] & PACKET0_COUNT_MASK) >> PACKET0_COUNT_SHIFT;
	if (reg + count > dev_priv->cmd_module.numof_p0_checkers) {
		DRM_ERROR("0x%08X registers is above last accepted registers\n",
			  reg << 2);
		return -EINVAL;
	}
	for (r = reg, i = 0; i <= count; i++, r++) {
		if (dev_priv->cmd_module.check_p0[r] == NULL) {
			continue;
		}
		if (dev_priv->cmd_module.check_p0[r] == (void *)-1) {
			DRM_ERROR("register 0x%08X (at %d) is forbidden\n",
			         r << 2, (*cdw_id) + i + 1);
			return -EINVAL;
		}
		ret = dev_priv->cmd_module.check_p0[r](dev, cmd,
						       (*cdw_id) + i + 1, r);
		if (ret) {
			return ret;
		}
	}
	/* header + N + 1 dword passed test */
	(*cdw_id) += count + 2;
	return 0;
}

static int amd_cmd_packet3_check(struct drm_device *dev,
				 struct amd_cmd *cmd,
				 int *cdw_id)
{
	struct drm_radeon_private *dev_priv = dev->dev_private;
	uint32_t opcode, count;
	int ret;

	opcode = (cmd->cdw[*cdw_id] & PACKET3_OPCODE_MASK) >>
		 PACKET3_OPCODE_SHIFT;
	if (opcode > dev_priv->cmd_module.numof_p3_checkers) {
		DRM_ERROR("0x%08X opcode is above last accepted opcodes\n",
			  opcode);
		return -EINVAL;
	}
	count = (cmd->cdw[*cdw_id] & PACKET3_COUNT_MASK) >> PACKET3_COUNT_SHIFT;
	if (dev_priv->cmd_module.check_p3[opcode] == NULL) {
		DRM_ERROR("0x%08X opcode is forbidden\n", opcode);
		return -EINVAL;
	}
	ret = dev_priv->cmd_module.check_p3[opcode](dev, cmd,
						    (*cdw_id) + 1, opcode,
						    count);
	if (ret) {
		return ret;
	}
	/* header + N + 1 dword passed test */
	(*cdw_id) += count + 2;
	return 0;
}

int amd_cmd_check(struct drm_device *dev, struct amd_cmd *cmd)
{
	uint32_t i;
	int ret;

	for (i = 0; i < cmd->cdw_count;) {
		switch (PACKET_HEADER_GET(cmd->cdw[i])) {
		case 0:
			ret = amd_cmd_packet0_check(dev, cmd, &i);
			if (ret) {
				return ret;
			}
			break;
		case 1:
			/* we don't accept packet 1 */
			return -EINVAL;
		case 2:
			/* FIXME: accept packet 2 */
			return -EINVAL;
		case 3:
			ret = amd_cmd_packet3_check(dev, cmd, &i);
			if (ret) {
				return ret;
			}
			break;
		}
	}
	return 0;
}

static int amd_ioctl_cmd_cleanup(struct drm_device *dev,
				 struct drm_file *file,
				 struct amd_cmd *cmd,
				 int r)
{
	/* check if we need to unfence object */
	if (r && (!list_empty(&cmd->bo_unused.list) ||
		  !list_empty(&cmd->bo_unused.list))) {
		drm_putback_buffer_objects(dev);		
	}
	if (cmd->cdw) {
		drm_bo_kunmap(&cmd->cdw_kmap);
		cmd->cdw = NULL;
	}
	/* derefence buffer as lookup reference them */
	amd_cmd_bo_cleanup(dev, cmd);
	if (cmd->bo) {
		drm_free(cmd->bo,
			 cmd->bo_count * sizeof(struct amd_cmd_bo),
		 	 DRM_MEM_DRIVER);
		cmd->bo = NULL;
	}
	drm_bo_read_unlock(&dev->bm.bm_lock);
	return r;
}

int amd_ioctl_cmd(struct drm_device *dev, void *data, struct drm_file *file)
{
	struct drm_radeon_private *dev_priv = dev->dev_private;
	struct drm_amd_cmd *drm_amd_cmd = data;
	struct drm_fence_arg *fence_arg = &drm_amd_cmd->fence_arg;
	struct drm_fence_object *fence;
	struct amd_cmd cmd;
	int tmp;
	int ret;

	/* check that we have a command checker */
	if (dev_priv->cmd_module.check == NULL) {
		DRM_ERROR("invalid command checker module.\n");
		return -EFAULT;
	}
	/* command dword count must be >= 0 */
	if (drm_amd_cmd->cdw_count == 0) {
		DRM_ERROR("command dword count is 0.\n");
		return -EINVAL;
	}
	/* FIXME: Lock buffer manager, is this really needed ? */
	ret = drm_bo_read_lock(&dev->bm.bm_lock);
	if (ret) {
		DRM_ERROR("bo read locking failed.\n");
		return ret;
	}
	/* cleanup & initialize amd cmd structure */
	memset(&cmd, 0, sizeof(struct amd_cmd));
	cmd.bo_count = drm_amd_cmd->bo_count;
	INIT_LIST_HEAD(&cmd.bo_unused.list);
	INIT_LIST_HEAD(&cmd.bo_used.list);
	/* allocate structure for bo parsing */
	cmd.bo = drm_calloc(cmd.bo_count, sizeof(struct amd_cmd_bo),
			    DRM_MEM_DRIVER);
	if (cmd.bo == NULL) {
		return amd_ioctl_cmd_cleanup(dev, file, &cmd, -ENOMEM);
        }
	/* parse cmd bo */
	ret = amd_cmd_parse_cmd_bo(dev, file, drm_amd_cmd, &cmd);
	if (ret) {
		return amd_ioctl_cmd_cleanup(dev, file, &cmd, ret);
	}
	/* check that a command buffer have been found */
	if (cmd.cdw_bo == NULL) {
		DRM_ERROR("no command buffer submited in cmd ioctl\n");
		return amd_ioctl_cmd_cleanup(dev, file, &cmd, -EINVAL);
	}
	/* map command buffer */
	cmd.cdw_count = drm_amd_cmd->cdw_count;
	cmd.cdw_size = (cmd.cdw_bo->bo->mem.num_pages * PAGE_SIZE) >> 2;
	if (cmd.cdw_size < cmd.cdw_count) {
		DRM_ERROR("command buffer (%d) is smaller than expected (%d)\n",
			  cmd.cdw_size, cmd.cdw_count);
		return amd_ioctl_cmd_cleanup(dev, file, &cmd, -EINVAL);
	}
	memset(&cmd.cdw_kmap, 0, sizeof(struct drm_bo_kmap_obj));
	ret = drm_bo_kmap(cmd.cdw_bo->bo, 0,
			  cmd.cdw_bo->bo->mem.num_pages, &cmd.cdw_kmap);
	if (ret) {
		DRM_ERROR("error mapping command buffer\n");
		return amd_ioctl_cmd_cleanup(dev, file, &cmd, ret);
	}
	cmd.cdw = drm_bmo_virtual(&cmd.cdw_kmap, &tmp);
	/* do command checking */
	ret = dev_priv->cmd_module.check(dev, &cmd);
	if (ret) {
		return amd_ioctl_cmd_cleanup(dev, file, &cmd, ret);
	}
	/* copy command to ring */
	ret = radeon_ms_ring_emit(dev, cmd.cdw, cmd.cdw_count);
	if (ret) {
		return amd_ioctl_cmd_cleanup(dev, file, &cmd, ret);
	}
	/* fence */
	ret = drm_fence_buffer_objects(dev, NULL, 0, NULL, &fence);
	if (ret) {
		return amd_ioctl_cmd_cleanup(dev, file, &cmd, ret);
	}
	if (!(fence_arg->flags & DRM_FENCE_FLAG_NO_USER)) {
		ret = drm_fence_add_user_object(file, fence,
						fence_arg->flags &
						DRM_FENCE_FLAG_SHAREABLE);
		if (!ret) {
			fence_arg->handle = fence->base.hash.key;
			fence_arg->fence_class = fence->fence_class;
			fence_arg->type = fence->type;
			fence_arg->signaled = fence->signaled_types;
			fence_arg->sequence = fence->sequence;
		} else {
			DRM_ERROR("error add object fence, expect oddity !\n");
		}
	}
	drm_fence_usage_deref_unlocked(&fence);
	return amd_ioctl_cmd_cleanup(dev, file, &cmd, 0);
}
