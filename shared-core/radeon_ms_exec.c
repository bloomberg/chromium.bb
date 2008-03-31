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
#include "amd_cbuffer.h"

static void radeon_ms_execbuffer_args_clean(struct drm_device *dev,
					    struct amd_cbuffer *cbuffer,
					    uint32_t args_count)
{
	mutex_lock(&dev->struct_mutex);
	while (args_count--) {
		drm_bo_usage_deref_locked(&cbuffer->args[args_count].buffer);
	}
	mutex_unlock(&dev->struct_mutex);
}

static int radeon_ms_execbuffer_args(struct drm_device *dev,
				     struct drm_file *file_priv,
				     struct drm_radeon_execbuffer *execbuffer,
				     struct amd_cbuffer *cbuffer)
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
		INIT_LIST_HEAD(&cbuffer->args[args_count].list);
		cbuffer->args[args_count].buffer = NULL;
		if (copy_from_user(&arg, (void __user *)((unsigned)data),
		    sizeof(struct drm_radeon_execbuffer_arg))) {
			ret = -EFAULT;
			goto out_err;
		}
		mutex_lock(&dev->struct_mutex);
		cbuffer->args[args_count].buffer = 
			drm_lookup_buffer_object(file_priv,
						 arg.d.req.arg_handle, 1);
		cbuffer->args[args_count].dw_id = arg.reloc_offset;
		mutex_unlock(&dev->struct_mutex);
		if (arg.d.req.op != drm_bo_validate) {
			DRM_ERROR("[radeon_ms] buffer object operation wasn't "
				  "validate.\n");
			ret = -EINVAL;
			goto out_err;
		}
		memset(&rep, 0, sizeof(struct drm_bo_arg_rep));
		ret = drm_bo_handle_validate(file_priv,
					     arg.d.req.bo_req.handle,
					     arg.d.req.bo_req.flags,
					     arg.d.req.bo_req.mask,
					     arg.d.req.bo_req.hint,
					     arg.d.req.bo_req.fence_class,
					     0,
					     &rep.bo_info,
					     &cbuffer->args[args_count].buffer);
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

		list_add_tail(&cbuffer->args[args_count].list,
			      &cbuffer->arg_unused.list);

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
	radeon_ms_execbuffer_args_clean(dev, cbuffer, args_count);
	return ret;
}

enum {
	REGISTER_FORBIDDEN = 0,
	REGISTER_SAFE,
	REGISTER_SET_OFFSET,
};
static uint8_t _r3xx_register_right[0x5000 >> 2];

static int amd_cbuffer_packet0_set_offset(struct drm_device *dev,
					  struct amd_cbuffer *cbuffer,
					  uint32_t reg, int dw_id,
					  struct amd_cbuffer_arg *arg)
{
	uint32_t gpu_addr;
	int ret;

	ret = radeon_ms_bo_get_gpu_addr(dev, &arg->buffer->mem, &gpu_addr);
	if (ret) {
		return ret;
	}
	switch (reg) {
	default:
		return -EINVAL;
	}
	return 0;
}

static struct amd_cbuffer_arg *
amd_cbuffer_arg_from_dw_id(struct amd_cbuffer_arg *head, uint32_t dw_id)
{
	struct amd_cbuffer_arg *arg;

	list_for_each_entry(arg, &head->list, list) {
		if (arg->dw_id == dw_id) {
			return arg;
		}
	}
	/* no buffer at this dw index */
	return NULL;
}

static int amd_cbuffer_packet0_check(struct drm_device *dev,
				     struct drm_file *file_priv,
				     struct amd_cbuffer *cbuffer,
				     int dw_id,
				     uint8_t *register_right)
{
	struct amd_cbuffer_arg *arg;
	uint32_t reg, count, r, i;
	int ret;

	reg = cbuffer->cbuffer[dw_id] & PACKET0_REG_MASK;
	count = (cbuffer->cbuffer[dw_id] & PACKET0_COUNT_MASK) >>
		PACKET0_COUNT_SHIFT;
	for (r = reg, i = 0; i <= count; i++, r++) {
		switch (register_right[i]) {
		case REGISTER_FORBIDDEN:
			return -EINVAL;
		case REGISTER_SAFE:
			break;
		case REGISTER_SET_OFFSET:
			arg = amd_cbuffer_arg_from_dw_id(&cbuffer->arg_unused,
							 dw_id + i +1);
			if (arg == NULL) {
				return -EINVAL;
			}
			/* remove from unparsed list */
			list_del(&arg->list);
			list_add_tail(&arg->list, &cbuffer->arg_used.list);
			/* set the offset */
			ret =  amd_cbuffer_packet0_set_offset(dev, cbuffer,
					  		      r, dw_id + i + 1,
							      arg);
			if (ret) {
				return ret;
			}
			break;
		}
	}
	/* header + N + 1 dword passed test */
	return count + 2;
}

static int amd_cbuffer_packet3_check(struct drm_device *dev,
				     struct drm_file *file_priv,
				     struct amd_cbuffer *cbuffer,
				     int dw_id)
{
	struct amd_cbuffer_arg *arg;
	uint32_t opcode, count;
	uint32_t s_auth, s_mask;
	uint32_t gpu_addr;
	int ret;

	opcode = (cbuffer->cbuffer[dw_id] & PACKET3_OPCODE_MASK) >>
		 PACKET3_OPCODE_SHIFT;
	count = (cbuffer->cbuffer[dw_id] & PACKET3_COUNT_MASK) >>
		PACKET3_COUNT_SHIFT;
	switch (opcode) {
	case PACKET3_OPCODE_NOP:
		break;
	case PACKET3_OPCODE_BITBLT:
	case PACKET3_OPCODE_BITBLT_MULTI:
		/* we only alow simple blit */
		if (count != 5) {
			return -EINVAL;
		}
		s_mask = 0xf;
		s_auth = 0x3;
		if ((cbuffer->cbuffer[dw_id + 1] & s_mask) != s_auth) {
			return -EINVAL;
		}
		arg = amd_cbuffer_arg_from_dw_id(&cbuffer->arg_unused, dw_id+2);
		if (arg == NULL) {
			return -EINVAL;
		}
		ret = radeon_ms_bo_get_gpu_addr(dev, &arg->buffer->mem,
						&gpu_addr);
		if (ret) {
			return ret;
		}
		gpu_addr = (gpu_addr >> 10) & 0x003FFFFF;
		cbuffer->cbuffer[dw_id + 2] &= 0xFFC00000;
		cbuffer->cbuffer[dw_id + 2] |= gpu_addr;

		arg = amd_cbuffer_arg_from_dw_id(&cbuffer->arg_unused, dw_id+3);
		if (arg == NULL) {
			return -EINVAL;
		}
		ret = radeon_ms_bo_get_gpu_addr(dev, &arg->buffer->mem,
						&gpu_addr);
		if (ret) {
			return ret;
		}
		gpu_addr = (gpu_addr >> 10) & 0x003FFFFF;
		cbuffer->cbuffer[dw_id + 3] &= 0xFFC00000;
		cbuffer->cbuffer[dw_id + 3] |= gpu_addr;
		/* FIXME: check that source & destination are big enough
		 * for requested blit */
		break;
	default:
		return -EINVAL;
	}
	/* header + N + 1 dword passed test */
	return count + 2;
}

static int amd_cbuffer_check(struct drm_device *dev,
			     struct drm_file *file_priv,
			     struct amd_cbuffer *cbuffer)
{
	uint32_t i;
	int ret;

	for (i = 0; i < cbuffer->cbuffer_dw_count;) {
		switch (PACKET_HEADER_GET(cbuffer->cbuffer[i])) {
		case 0:
			ret = amd_cbuffer_packet0_check(dev, file_priv,
							cbuffer, i,
							_r3xx_register_right);
			if (ret <= 0) {
				return ret;
			}
			/* advance to next packet */
			i += ret;
			break;
		case 1:
			/* we don't accept packet 1 */
			return -EINVAL;
		case 2:
			/* packet 2 */
			i += 1;
			break;
		case 3:
			ret = amd_cbuffer_packet3_check(dev, file_priv,
							cbuffer, i);
			if (ret <= 0) {
				return ret;
			}
			/* advance to next packet */
			i += ret;
			break;
		}
	}
	return 0;
}

int radeon_ms_execbuffer(struct drm_device *dev, void *data,
			 struct drm_file *file_priv)
{
	struct drm_radeon_execbuffer *execbuffer = data;
	struct drm_fence_arg *fence_arg = &execbuffer->fence_arg;
	struct drm_bo_kmap_obj cmd_kmap;
	struct drm_fence_object *fence;
	int cmd_is_iomem;
	int ret = 0;
	struct amd_cbuffer cbuffer;

	/* command buffer dword count must be >= 0 */
	if (execbuffer->cmd_size < 0) {
		return -EINVAL;
	}

	/* FIXME: Lock buffer manager, is this really needed ?
	 */
	ret = drm_bo_read_lock(&dev->bm.bm_lock);
	if (ret) {
		return ret;
	}

	cbuffer.args = drm_calloc(execbuffer->args_count,
				  sizeof(struct amd_cbuffer_arg),
				  DRM_MEM_DRIVER);
	if (cbuffer.args == NULL) {
		ret = -ENOMEM;
		goto out_free;
        }

	INIT_LIST_HEAD(&cbuffer.arg_unused.list);
	INIT_LIST_HEAD(&cbuffer.arg_used.list);

	/* process arguments */
	ret = radeon_ms_execbuffer_args(dev, file_priv, execbuffer, &cbuffer);
	if (ret) {
		DRM_ERROR("[radeon_ms] execbuffer wrong arguments\n");
		goto out_free;
	}

	/* map command buffer */
	cbuffer.cbuffer_dw_count = (cbuffer.args[0].buffer->mem.num_pages *
				    PAGE_SIZE) >> 2;
	if (execbuffer->cmd_size > cbuffer.cbuffer_dw_count) {
		ret = -EINVAL;
		goto out_free_release;
	}
	cbuffer.cbuffer_dw_count = execbuffer->cmd_size;
	memset(&cmd_kmap, 0, sizeof(struct drm_bo_kmap_obj));
	ret = drm_bo_kmap(cbuffer.args[0].buffer, 0,
		          cbuffer.args[0].buffer->mem.num_pages, &cmd_kmap);
	if (ret) {
		DRM_ERROR("[radeon_ms] error mapping ring buffer: %d\n", ret);
		goto out_free_release;
	}
	cbuffer.cbuffer = drm_bmo_virtual(&cmd_kmap, &cmd_is_iomem);
	list_del(&cbuffer.args[0].list);
	list_add_tail(&cbuffer.args[0].list , &cbuffer.arg_used.list);

	/* do cmd checking & relocations */
	ret = amd_cbuffer_check(dev, file_priv, &cbuffer);
	if (ret) {
		drm_putback_buffer_objects(dev);
		goto out_free_release;
	}

	ret = radeon_ms_ring_emit(dev, cbuffer.cbuffer,
				  cbuffer.cbuffer_dw_count);
	if (ret) {
		drm_putback_buffer_objects(dev);
		goto out_free_release;
	}

	/* fence */
	ret = drm_fence_buffer_objects(dev, NULL, 0, NULL, &fence);
	if (ret) {
		drm_putback_buffer_objects(dev);
		DRM_ERROR("[radeon_ms] fence buffer objects failed\n");
		goto out_free_release;
	}
	if (!(fence_arg->flags & DRM_FENCE_FLAG_NO_USER)) {
		ret = drm_fence_add_user_object(file_priv, fence,
						fence_arg->flags &
						DRM_FENCE_FLAG_SHAREABLE);
		if (!ret) {
			fence_arg->handle = fence->base.hash.key;
			fence_arg->fence_class = fence->fence_class;
			fence_arg->type = fence->type;
			fence_arg->signaled = fence->signaled_types;
			fence_arg->sequence = fence->sequence;
		}
	}
	drm_fence_usage_deref_unlocked(&fence);
out_free_release:
	drm_bo_kunmap(&cmd_kmap);
	radeon_ms_execbuffer_args_clean(dev, &cbuffer, execbuffer->args_count);
out_free:
	drm_free(cbuffer.args,
		 (execbuffer->args_count * sizeof(struct amd_cbuffer_arg)),
		 DRM_MEM_DRIVER);
	drm_bo_read_unlock(&dev->bm.bm_lock);
	return ret;
}
