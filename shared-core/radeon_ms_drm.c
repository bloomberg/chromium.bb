/*
 * Copyright 2007 Jérôme Glisse
 * Copyright 2007 Alex Deucher
 * Copyright 2007 Dave Airlie
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
#include "drm_pciids.h"
#include "radeon_ms.h"


static uint32_t radeon_ms_mem_prios[] = {
	DRM_BO_MEM_VRAM,
	DRM_BO_MEM_TT,
	DRM_BO_MEM_LOCAL,
};

static uint32_t radeon_ms_busy_prios[] = {
	DRM_BO_MEM_TT,
	DRM_BO_MEM_VRAM,
	DRM_BO_MEM_LOCAL,
};

struct drm_bo_driver radeon_ms_bo_driver = {
	.mem_type_prio = radeon_ms_mem_prios,
	.mem_busy_prio = radeon_ms_busy_prios,
	.num_mem_type_prio = sizeof(radeon_ms_mem_prios)/sizeof(uint32_t),
	.num_mem_busy_prio = sizeof(radeon_ms_busy_prios)/sizeof(uint32_t),
	.create_ttm_backend_entry = radeon_ms_create_ttm_backend,
	.fence_type = r3xx_fence_types,
	.invalidate_caches = radeon_ms_invalidate_caches,
	.init_mem_type = radeon_ms_init_mem_type,
	.evict_flags = radeon_ms_evict_flags,
	.move = radeon_ms_bo_move,
	.ttm_cache_flush = radeon_ms_ttm_flush,
};

struct drm_ioctl_desc radeon_ms_ioctls[] = {
	DRM_IOCTL_DEF(DRM_RADEON_EXECBUFFER, radeon_ms_execbuffer, DRM_AUTH),
};
int radeon_ms_num_ioctls = DRM_ARRAY_SIZE(radeon_ms_ioctls);

int radeon_ms_driver_dma_ioctl(struct drm_device *dev, void *data,
			       struct drm_file *file_priv)
{
	struct drm_device_dma *dma = dev->dma;
	struct drm_dma *d = data;

	LOCK_TEST_WITH_RETURN(dev, file_priv);

	/* Please don't send us buffers.
	 */
	if (d->send_count != 0) {
		DRM_ERROR("Process %d trying to send %d buffers via drmDMA\n",
			  DRM_CURRENTPID, d->send_count);
		return -EINVAL;
	}

	/* Don't ask us buffer neither :)
	 */
	DRM_ERROR("Process %d trying to get %d buffers (of %d max)\n",
		  DRM_CURRENTPID, d->request_count, dma->buf_count);
	return -EINVAL;
}

void radeon_ms_driver_lastclose(struct drm_device * dev)
{
}

int radeon_ms_driver_load(struct drm_device *dev, unsigned long flags)
{
	struct drm_radeon_private *dev_priv;
	int ret = 0;

	DRM_INFO("[radeon_ms] loading\n");
	/* allocate and clear device private structure */
	dev_priv = drm_alloc(sizeof(struct drm_radeon_private), DRM_MEM_DRIVER);
	if (dev_priv == NULL)
		return -ENOMEM;
	memset(dev_priv, 0, sizeof(struct drm_radeon_private));
	dev->dev_private = (void *)dev_priv;

	/* initialize modesetting structure (must be done here) */
	drm_mode_config_init(dev);

	/* flags correspond to chipset family */
	dev_priv->usec_timeout = 100;
	dev_priv->family = flags & 0xffffU;
	dev_priv->bus_type = flags & 0xff0000U;
	/* initialize family functions */
	ret = radeon_ms_family_init(dev);
	if (ret != 0) {
		radeon_ms_driver_unload(dev);
		return ret;
	}

	dev_priv->fence = drm_alloc(sizeof(struct r3xx_fence), DRM_MEM_DRIVER);
	if (dev_priv->fence == NULL) {
		radeon_ms_driver_unload(dev);
		return -ENOMEM;
	}
	memset(dev_priv->fence, 0, sizeof(struct r3xx_fence));

	/* we don't want userspace to be able to map this so don't use
	 * drm_addmap */
	dev_priv->mmio.offset = drm_get_resource_start(dev, 2);
	dev_priv->mmio.size = drm_get_resource_len(dev, 2);
	dev_priv->mmio.type = _DRM_REGISTERS;
	dev_priv->mmio.flags = _DRM_RESTRICTED;
	drm_core_ioremap(&dev_priv->mmio, dev); 
	/* map vram FIXME: IGP likely don't have any of this */
	dev_priv->vram.offset = drm_get_resource_start(dev, 0);
	dev_priv->vram.size = drm_get_resource_len(dev, 0);
	dev_priv->vram.type = _DRM_FRAME_BUFFER;
	dev_priv->vram.flags = _DRM_RESTRICTED;
	drm_core_ioremap(&dev_priv->vram, dev);

	/* save radeon initial state which will be restored upon module
	 * exit */
	radeon_ms_state_save(dev, &dev_priv->load_state);
	dev_priv->restore_state = 1;
	memcpy(&dev_priv->driver_state, &dev_priv->load_state,
	       sizeof(struct radeon_state));

	/* initialize irq */
	ret = radeon_ms_irq_init(dev);
	if (ret != 0) {
		radeon_ms_driver_unload(dev);
		return ret;
	}

	/* init bo driver */
	dev_priv->fence_id_last = 1;
	dev_priv->fence_reg = SCRATCH_REG2;
	drm_bo_driver_init(dev);
	/* initialize vram */
	ret = drm_bo_init_mm(dev, DRM_BO_MEM_VRAM, 0, dev_priv->vram.size, 1);
	if (ret != 0) {
		radeon_ms_driver_unload(dev);
		return ret;
	}

	/* initialize gpu address space (only after) VRAM initialization */
	ret = radeon_ms_gpu_initialize(dev);
	if (ret != 0) {
		radeon_ms_driver_unload(dev);
		return ret;
	}
	radeon_ms_gpu_restore(dev, &dev_priv->driver_state);

	/* initialize ttm */
	ret = drm_bo_init_mm(dev, DRM_BO_MEM_TT, 0,
			     dev_priv->gpu_gart_size / RADEON_PAGE_SIZE, 1);
	if (ret != 0) {
		radeon_ms_driver_unload(dev);
		return ret;
	}

	/* initialize ring buffer */
	/* set ring size to 4Mo FIXME: should make a parameter for this */
	dev_priv->write_back_area_size = 4 * 1024;
	dev_priv->ring_buffer_size = 4 * 1024 * 1024;
	ret = radeon_ms_cp_init(dev);
	if (ret != 0) {
		radeon_ms_driver_unload(dev);
		return ret;
	}

	/* initialize modesetting */
	dev->mode_config.min_width = 0;
	dev->mode_config.min_height = 0;
	dev->mode_config.max_width = 4096;
	dev->mode_config.max_height = 4096;
	dev->mode_config.fb_base = dev_priv->vram.offset;
	ret = radeon_ms_crtc_create(dev, 1);
	if (ret != 0) {
		radeon_ms_driver_unload(dev);
		return ret;
	}
	ret = radeon_ms_outputs_from_rom(dev);
	if (ret < 0) {
		radeon_ms_driver_unload(dev);
		return ret;
	} else if (!ret) {
		ret = radeon_ms_outputs_from_properties(dev);
		if (ret < 0) {
			radeon_ms_driver_unload(dev);
			return ret;
		} else if (ret == 0) {
			DRM_INFO("[radeon_ms] no outputs !\n");
		}
	} else {
		DRM_INFO("[radeon_ms] added %d outputs from rom.\n", ret);
	}
	ret = radeon_ms_connectors_from_rom(dev);
	if (ret < 0) {
		radeon_ms_driver_unload(dev);
		return ret;
	} else if (!ret) {
		ret = radeon_ms_connectors_from_properties(dev);
		if (ret < 0) {
			radeon_ms_driver_unload(dev);
			return ret;
		} else if (!ret) {
			DRM_INFO("[radeon_ms] no connectors !\n");
		}
	} else {
		DRM_INFO("[radeon_ms] added %d connectors from rom.\n", ret);
	}
	radeon_ms_outputs_save(dev, &dev_priv->load_state);
	drm_initial_config(dev, false);

	ret = drm_irq_install(dev);
	if (ret != 0) {
		radeon_ms_driver_unload(dev);
		return ret;
	}

	DRM_INFO("[radeon_ms] successfull initialization\n");
	return 0;
}

int radeon_ms_driver_open(struct drm_device * dev, struct drm_file *file_priv)
{
	return 0;
}


int radeon_ms_driver_unload(struct drm_device *dev)
{
	struct drm_radeon_private *dev_priv = dev->dev_private;

	if (dev_priv == NULL) {
		return 0;
	}

	/* cleanup modesetting */
	drm_mode_config_cleanup(dev);
	DRM_INFO("[radeon_ms] modesetting clean\n");
	radeon_ms_outputs_restore(dev, &dev_priv->load_state);
	radeon_ms_connectors_destroy(dev);
	radeon_ms_outputs_destroy(dev);
	
	/* shutdown cp engine */
	radeon_ms_cp_finish(dev);
	DRM_INFO("[radeon_ms] cp clean\n");

	drm_irq_uninstall(dev);
	DRM_INFO("[radeon_ms] irq uninstalled\n");

	DRM_INFO("[radeon_ms] unloading\n");
	/* clean ttm memory manager */
	mutex_lock(&dev->struct_mutex);
	if (drm_bo_clean_mm(dev, DRM_BO_MEM_TT, 1)) {
		DRM_ERROR("TT memory manager not clean. Delaying takedown\n");
	}
	mutex_unlock(&dev->struct_mutex);
	DRM_INFO("[radeon_ms] TT memory clean\n");
	/* finish */
	if (dev_priv->bus_finish) {
		dev_priv->bus_finish(dev);
	}
	DRM_INFO("[radeon_ms] bus down\n");
	/* clean vram memory manager */
	mutex_lock(&dev->struct_mutex);
	if (drm_bo_clean_mm(dev, DRM_BO_MEM_VRAM, 1)) {
		DRM_ERROR("VRAM memory manager not clean. Delaying takedown\n");
	}
	mutex_unlock(&dev->struct_mutex);
	DRM_INFO("[radeon_ms] VRAM memory clean\n");
	/* clean memory manager */
	drm_bo_driver_finish(dev);
	DRM_INFO("[radeon_ms] memory manager clean\n");
	/* restore card state */
	if (dev_priv->restore_state) {
		radeon_ms_state_restore(dev, &dev_priv->load_state);
	}
	DRM_INFO("[radeon_ms] state restored\n");
	if (dev_priv->mmio.handle) {
		drm_core_ioremapfree(&dev_priv->mmio, dev);
	}
	if (dev_priv->vram.handle) {
		drm_core_ioremapfree(&dev_priv->vram, dev);
	}
	DRM_INFO("[radeon_ms] map released\n");
	drm_free(dev_priv->fence, sizeof(struct r3xx_fence), DRM_MEM_DRIVER);
	drm_free(dev_priv, sizeof(*dev_priv), DRM_MEM_DRIVER);
	dev->dev_private = NULL;


	DRM_INFO("[radeon_ms] that's all the folks\n");
	return 0;
}

