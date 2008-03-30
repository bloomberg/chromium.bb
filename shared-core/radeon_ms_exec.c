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

static void radeon_ms_execbuffer_args_clean(struct drm_device *dev,
					    struct drm_buffer_object **buffers,
					    uint32_t args_count)
{
	mutex_lock(&dev->struct_mutex);
	while (args_count--) {
		drm_bo_usage_deref_locked(&buffers[args_count]);
	}
	mutex_unlock(&dev->struct_mutex);
}

static int radeon_ms_execbuffer_args(struct drm_device *dev,
				     struct drm_file *file_priv,
				     struct drm_radeon_execbuffer *execbuffer,
				     struct drm_buffer_object **buffers,
				     uint32_t *relocs)
{
	struct drm_radeon_execbuffer_arg arg;
	struct drm_bo_arg_rep rep;
	uint32_t args_count = 0;
	uint64_t next = 0;
	uint64_t data = execbuffer->args;
	int ret = 0;

	do {
		if (args_count >= execbuffer->args_count) {
			DRM_ERROR("[radeon_ms] buffer count exceeded %d\n.",
				  execbuffer->args_count);
			ret = -EINVAL;
			goto out_err;
		}
		buffers[args_count] = NULL;
		if (copy_from_user(&arg, (void __user *)((unsigned)data),
		    sizeof(struct drm_radeon_execbuffer_arg))) {
			ret = -EFAULT;
			goto out_err;
		}
		mutex_lock(&dev->struct_mutex);
		buffers[args_count] =
			drm_lookup_buffer_object(file_priv,
						 arg.d.req.arg_handle, 1);
		relocs[args_count] = arg.reloc_offset;
		mutex_unlock(&dev->struct_mutex);
		if (arg.d.req.op != drm_bo_validate) {
			DRM_ERROR("[radeon_ms] buffer object operation wasn't "
				  "validate.\n");
			ret = -EINVAL;
			goto out_err;
		}
		memset(&rep, 0, sizeof(struct drm_bo_arg_rep));
		if (args_count >= 1) {
			ret = drm_bo_handle_validate(file_priv,
						     arg.d.req.bo_req.handle,
						     arg.d.req.bo_req.flags,
						     arg.d.req.bo_req.mask,
						     arg.d.req.bo_req.hint,
						     arg.d.req.bo_req.fence_class,
						     0,
						     &rep.bo_info,
						     &buffers[args_count]);
		}
		if (ret) {
			DRM_ERROR("[radeon_ms] error on handle validate %d\n",
				  ret);
			rep.ret = ret;
			goto out_err;
		}
		next = arg.next;
		arg.d.rep = rep;
		if (copy_to_user((void __user *)((unsigned)data), &arg,
		    sizeof(struct drm_radeon_execbuffer_arg))) {
			ret = -EFAULT;
			goto out_err;
		}
		data = next;
		args_count++;
	} while (next != 0);
	if (args_count != execbuffer->args_count) {
		DRM_ERROR("[radeon_ms] not enought buffer got %d waited %d\n.",
			  args_count, execbuffer->args_count);
		ret = -EINVAL;
		goto out_err;
	}
	return 0;
out_err:
	radeon_ms_execbuffer_args_clean(dev, buffers, args_count);
	return ret;
}

static int radeon_ms_execbuffer_check(struct drm_device *dev,
				      struct drm_file *file_priv,
				      struct drm_radeon_execbuffer *execbuffer,
				      struct drm_buffer_object **buffers,
				      uint32_t *relocs,
				      uint32_t *cmd)
{
	uint32_t i, gpu_addr;
	int ret;

	for (i = 0; i < execbuffer->args_count; i++) {
		if (relocs[i]) {
			ret = radeon_ms_bo_get_gpu_addr(dev, &buffers[i]->mem,
							&gpu_addr);
			if (ret) {
				return ret;
			}
			cmd[relocs[i]] |= (gpu_addr) >> 10;
		}
	}
	for (i = 0; i < execbuffer->cmd_size; i++) {
#if 0
		DRM_INFO("cmd[%d]=0x%08X\n", i, cmd[i]);
#endif
	}
	return 0;
}

int radeon_ms_execbuffer(struct drm_device *dev, void *data,
			 struct drm_file *file_priv)
{
	struct drm_radeon_execbuffer *execbuffer = data;
	struct drm_fence_arg *fence_arg = &execbuffer->fence_arg;
	struct drm_buffer_object **buffers;
	struct drm_bo_kmap_obj cmd_kmap;
	struct drm_fence_object *fence;
	uint32_t *relocs;
	uint32_t *cmd;
	int cmd_is_iomem;
	int ret = 0;


	ret = drm_bo_read_lock(&dev->bm.bm_lock);
	if (ret) {
		return ret;
	}

	relocs = drm_calloc(execbuffer->args_count, sizeof(uint32_t),
			    DRM_MEM_DRIVER);
	if (relocs == NULL) {
	        drm_bo_read_unlock(&dev->bm.bm_lock);
		return -ENOMEM;
        }
	buffers = drm_calloc(execbuffer->args_count,
			     sizeof(struct drm_buffer_object *),
			     DRM_MEM_DRIVER);
	if (buffers == NULL) {
		drm_free(relocs, (execbuffer->args_count * sizeof(uint32_t)),
			 DRM_MEM_DRIVER);
	        drm_bo_read_unlock(&dev->bm.bm_lock);
		return -ENOMEM;
        }
	/* process arguments */
	ret = radeon_ms_execbuffer_args(dev, file_priv, execbuffer,
					buffers, relocs);
	if (ret) {
		DRM_ERROR("[radeon_ms] execbuffer wrong arguments\n");
		goto out_free;
	}
	/* map command buffer */
	memset(&cmd_kmap, 0, sizeof(struct drm_bo_kmap_obj));
	ret = drm_bo_kmap(buffers[0],
			  0,
		          buffers[0]->mem.num_pages,
			  &cmd_kmap);
	if (ret) {
		DRM_ERROR("[radeon_ms] error mapping ring buffer: %d\n", ret);
		goto out_free_release;
	}
	cmd = drm_bmo_virtual(&cmd_kmap, &cmd_is_iomem);
	/* do cmd checking & relocations */
	ret = radeon_ms_execbuffer_check(dev, file_priv, execbuffer,
					 buffers, relocs, cmd);
	if (ret) {
		drm_putback_buffer_objects(dev);
		goto out_free_release;
	}

	ret = radeon_ms_ring_emit(dev, cmd, execbuffer->cmd_size);
	if (ret) {
		drm_putback_buffer_objects(dev);
		goto out_free_release;
	}

	/* fence */
	if (execbuffer->args_count > 1) {
		ret = drm_fence_buffer_objects(dev, NULL, 0, NULL, &fence);
		if (ret) {
			drm_putback_buffer_objects(dev);
			DRM_ERROR("[radeon_ms] fence buffer objects failed\n");
			goto out_free_release;
		}
		if (!(fence_arg->flags & DRM_FENCE_FLAG_NO_USER)) {
			ret = drm_fence_add_user_object(file_priv, fence,
							fence_arg->flags & DRM_FENCE_FLAG_SHAREABLE);
			if (!ret) {
				fence_arg->handle = fence->base.hash.key;
				fence_arg->fence_class = fence->fence_class;
				fence_arg->type = fence->type;
				fence_arg->signaled = fence->signaled_types;
				fence_arg->sequence = fence->sequence;
			}
		}
		drm_fence_usage_deref_unlocked(&fence);
	}
out_free_release:
	drm_bo_kunmap(&cmd_kmap);
	radeon_ms_execbuffer_args_clean(dev, buffers, execbuffer->args_count);
out_free:
	drm_free(relocs, (execbuffer->args_count * sizeof(uint32_t)),
		 DRM_MEM_DRIVER);
	drm_free(buffers,
		 (execbuffer->args_count * sizeof(struct drm_buffer_object *)),
		 DRM_MEM_DRIVER);
	drm_bo_read_unlock(&dev->bm.bm_lock);
	return ret;
}
