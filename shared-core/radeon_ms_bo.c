/*
 * Copyright 2007 Dave Airlie
 * Copyright 2007 Jérôme Glisse
 * All Rights Reserved.
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sub license, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT. IN NO EVENT SHALL
 * THE COPYRIGHT HOLDERS, AUTHORS AND/OR ITS SUPPLIERS BE LIABLE FOR ANY CLAIM,
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR 
 * OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE 
 * USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial portions
 * of the Software.
 */
/*
 * Authors:
 *    Dave Airlie <airlied@linux.ie>
 *    Jerome Glisse <glisse@freedesktop.org>
 */
#include "drmP.h"
#include "drm.h"

#include "radeon_ms.h"

#define GMC_SRC_PITCH_OFFSET_CNTL   (1    <<  0)
#define GMC_DST_PITCH_OFFSET_CNTL   (1    <<  1)
#define GMC_BRUSH_NONE              (15   <<  4)
#define GMC_SRC_DATATYPE_COLOR      (3    << 12)
#define ROP3_S                      0x00cc0000
#define DP_SRC_SOURCE_MEMORY        (2    << 24)
#define GMC_CLR_CMP_CNTL_DIS        (1    << 28)
#define GMC_WR_MSK_DIS              (1    << 30)

void radeon_ms_bo_copy_blit(struct drm_device *dev,
			    uint32_t src_offset,
			    uint32_t dst_offset,
			    uint32_t pages)
{
	struct drm_radeon_private *dev_priv = dev->dev_private;
	uint32_t num_pages, stride, c;
	uint32_t offset_inc = 0;
	uint32_t cmd[7];

	if (!dev_priv) {
		return;
	}

	/* radeon limited to 16320=255*64 bytes per row so copy at
	 * most 2 pages */
	num_pages = 2;
	stride = ((num_pages * PAGE_SIZE) / 64) & 0xff;
	while(pages > 0) {
		if (num_pages > pages) {
			num_pages = pages;
			stride = ((num_pages * PAGE_SIZE) / 64) & 0xff;
		}
		c = pages / num_pages;
		if (c >= 8192) {
			c = 8191;
		}
		cmd[0] = CP_PACKET3(PACKET3_OPCODE_BITBLT, 5);
		cmd[1] = GMC_SRC_PITCH_OFFSET_CNTL |
			 GMC_DST_PITCH_OFFSET_CNTL |
			 GMC_BRUSH_NONE |
			 (0x6 << 8) |
			 GMC_SRC_DATATYPE_COLOR |
			 ROP3_S |
			 DP_SRC_SOURCE_MEMORY |
			 GMC_CLR_CMP_CNTL_DIS |
			 GMC_WR_MSK_DIS;
		cmd[2] = (stride << 22) | (src_offset >> 10);
		cmd[3] = (stride << 22) | (dst_offset >> 10);
		cmd[4] = (0 << 16) | 0;
		cmd[5] = (0 << 16) | 0;
		cmd[6] = ((stride * 16) << 16) | c;
		radeon_ms_ring_emit(dev, cmd, 7);
		offset_inc = num_pages * c * PAGE_SIZE;
		src_offset += offset_inc;
		dst_offset += offset_inc;
		pages -= num_pages * c;
	}
	/* wait for 2d engine to go busy so wait_until stall */
	for (c = 0; c < dev_priv->usec_timeout; c++) {
		uint32_t status = MMIO_R(RBBM_STATUS);
		if ((RBBM_STATUS__E2_BUSY & status) ||
		    (RBBM_STATUS__CBA2D_BUSY & status)) {
			DRM_INFO("[radeon_ms] RBBM_STATUS 0x%08X\n", status);
			break;
		}
		DRM_UDELAY(1);
	}
	/* Sync everything up */
	cmd[0] = CP_PACKET0(WAIT_UNTIL, 0);
	cmd[1] = WAIT_UNTIL__WAIT_2D_IDLECLEAN |
		 WAIT_UNTIL__WAIT_HOST_IDLECLEAN;
	radeon_ms_ring_emit(dev, cmd, 2);
	return;
}

static int radeon_ms_bo_move_blit(struct drm_buffer_object *bo,
				  int evict, int no_wait,
				  struct drm_bo_mem_reg *new_mem)
{
	struct drm_device *dev = bo->dev;
	struct drm_bo_mem_reg *old_mem = &bo->mem;
	uint32_t gpu_src_addr;
	uint32_t gpu_dst_addr;
	int ret;

	ret = radeon_ms_bo_get_gpu_addr(dev, old_mem, &gpu_src_addr);
	if (ret) {
		return ret;
	}
	ret = radeon_ms_bo_get_gpu_addr(dev, new_mem, &gpu_dst_addr);
	if (ret) {
		return ret;
	}

	radeon_ms_bo_copy_blit(bo->dev,
			       gpu_src_addr,
			       gpu_dst_addr,
			       new_mem->num_pages);
	
	ret = drm_bo_move_accel_cleanup(bo, evict, no_wait, 0,
					 DRM_FENCE_TYPE_EXE |
					 DRM_AMD_FENCE_TYPE_R |
					 DRM_AMD_FENCE_TYPE_W,
					 DRM_AMD_FENCE_FLAG_FLUSH,
					 new_mem);
	return ret;
}

static int radeon_ms_bo_move_flip(struct drm_buffer_object *bo,
				  int evict, int no_wait,
				  struct drm_bo_mem_reg *new_mem)
{
	struct drm_device *dev = bo->dev;
	struct drm_bo_mem_reg tmp_mem;
	int ret;

	tmp_mem = *new_mem;
	tmp_mem.mm_node = NULL;
	tmp_mem.flags = DRM_BO_FLAG_MEM_TT |
		       DRM_BO_FLAG_CACHED |
		       DRM_BO_FLAG_FORCE_CACHING;
	ret = drm_bo_mem_space(bo, &tmp_mem, no_wait);
	if (ret) {
		return ret;
	}

	ret = drm_ttm_bind(bo->ttm, &tmp_mem);
	if (ret) {
		goto out_cleanup;
	}
	ret = radeon_ms_bo_move_blit(bo, 1, no_wait, &tmp_mem);
	if (ret) {
		goto out_cleanup;
	}
	ret = drm_bo_move_ttm(bo, evict, no_wait, new_mem);
out_cleanup:
	if (tmp_mem.mm_node) {
		mutex_lock(&dev->struct_mutex);
		if (tmp_mem.mm_node != bo->pinned_node)
			drm_mm_put_block(tmp_mem.mm_node);
		tmp_mem.mm_node = NULL;
		mutex_unlock(&dev->struct_mutex);
	}
	return ret;
}

int radeon_ms_bo_get_gpu_addr(struct drm_device *dev,
			      struct drm_bo_mem_reg *mem,
			      uint32_t *gpu_addr)
{
	struct drm_radeon_private *dev_priv = dev->dev_private;

	*gpu_addr = mem->mm_node->start << PAGE_SHIFT;
	switch (mem->flags & DRM_BO_MASK_MEM) {
	case DRM_BO_FLAG_MEM_TT:
		*gpu_addr +=  dev_priv->gpu_gart_start;
		DRM_INFO("[radeon_ms] GPU TT: 0x%08X\n", *gpu_addr);
		break;
	case DRM_BO_FLAG_MEM_VRAM:
		*gpu_addr +=  dev_priv->gpu_vram_start;
		DRM_INFO("[radeon_ms] GPU VRAM: 0x%08X\n", *gpu_addr);
		break;
	default:
		DRM_ERROR("[radeon_ms] memory not accessible by GPU\n");
		return -EINVAL;
	}
	return 0;
}

int radeon_ms_bo_move(struct drm_buffer_object *bo, int evict,
		   int no_wait, struct drm_bo_mem_reg *new_mem)
{
	struct drm_bo_mem_reg *old_mem = &bo->mem;
	if (old_mem->mem_type == DRM_BO_MEM_LOCAL) {
		return drm_bo_move_memcpy(bo, evict, no_wait, new_mem);
	} else if (new_mem->mem_type == DRM_BO_MEM_LOCAL) {
		if (radeon_ms_bo_move_flip(bo, evict, no_wait, new_mem))
			return drm_bo_move_memcpy(bo, evict, no_wait, new_mem);
	} else {
		if (radeon_ms_bo_move_blit(bo, evict, no_wait, new_mem))
			return drm_bo_move_memcpy(bo, evict, no_wait, new_mem);
	}
	return 0;
}

struct drm_ttm_backend *radeon_ms_create_ttm_backend(struct drm_device * dev)
{
	struct drm_radeon_private *dev_priv = dev->dev_private;

	if (dev_priv && dev_priv->create_ttm)
		return dev_priv->create_ttm(dev);
	return NULL;
}

uint64_t radeon_ms_evict_flags(struct drm_buffer_object *bo)
{
	switch (bo->mem.mem_type) {
	case DRM_BO_MEM_LOCAL:
	case DRM_BO_MEM_TT:
		return DRM_BO_FLAG_MEM_LOCAL;
	case DRM_BO_MEM_VRAM:
		if (bo->mem.num_pages > 128)
			return DRM_BO_MEM_TT;
		else
			return DRM_BO_MEM_LOCAL;
	default:
		return DRM_BO_FLAG_MEM_TT | DRM_BO_FLAG_CACHED;
	}
}

int radeon_ms_init_mem_type(struct drm_device * dev, uint32_t type,
			    struct drm_mem_type_manager * man)
{
	struct drm_radeon_private *dev_priv = dev->dev_private;

	switch (type) {
	case DRM_BO_MEM_LOCAL:
		man->flags = _DRM_FLAG_MEMTYPE_MAPPABLE |
		    _DRM_FLAG_MEMTYPE_CACHED;
		man->drm_bus_maptype = 0;
		break;
	case DRM_BO_MEM_VRAM:
		man->flags =  _DRM_FLAG_MEMTYPE_FIXED |
			      _DRM_FLAG_MEMTYPE_MAPPABLE |
			      _DRM_FLAG_NEEDS_IOREMAP;
		man->io_addr = NULL;
		man->drm_bus_maptype = _DRM_FRAME_BUFFER;
		man->io_offset = dev_priv->vram.offset;
		man->io_size = dev_priv->vram.size;
		break;
	case DRM_BO_MEM_TT:
		if (!dev_priv->bus_ready) {
			DRM_ERROR("Bus isn't initialized while "
				  "intializing TT memory type\n");
			return -EINVAL;
		}
		switch(dev_priv->bus_type) {
		case RADEON_AGP:
			if (!(drm_core_has_AGP(dev) && dev->agp)) {
				DRM_ERROR("AGP is not enabled for memory "
					  "type %u\n", (unsigned)type);
				return -EINVAL;
			}
			man->io_offset = dev->agp->agp_info.aper_base;
			man->io_size = dev->agp->agp_info.aper_size *
				       1024 * 1024;
			man->io_addr = NULL;
			man->flags = _DRM_FLAG_MEMTYPE_MAPPABLE |
				     _DRM_FLAG_MEMTYPE_CSELECT |
				     _DRM_FLAG_NEEDS_IOREMAP;
			man->drm_bus_maptype = _DRM_AGP;
			man->gpu_offset = 0;
			break;
		default:
			man->io_offset = dev_priv->gpu_gart_start;
			man->io_size = dev_priv->gpu_gart_size;
			man->io_addr = NULL;
			man->flags = _DRM_FLAG_MEMTYPE_CSELECT |
				     _DRM_FLAG_MEMTYPE_MAPPABLE |
				     _DRM_FLAG_MEMTYPE_CMA;
			man->drm_bus_maptype = _DRM_SCATTER_GATHER;
			break;
		}
		break;
	default:
		DRM_ERROR("Unsupported memory type %u\n", (unsigned)type);
		return -EINVAL;
	}
	return 0;
}

int radeon_ms_invalidate_caches(struct drm_device * dev, uint64_t flags)
{
	struct drm_radeon_private *dev_priv = dev->dev_private;

	dev_priv->flush_cache(dev);
	return 0;
}

void radeon_ms_ttm_flush(struct drm_ttm *ttm)
{
	if (!ttm)
		return;

	DRM_MEMORYBARRIER();
}
