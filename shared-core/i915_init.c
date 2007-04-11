#include "drmP.h"
#include "drm.h"
#include "drm_sarea.h"
#include "i915_drm.h"
#include "i915_drv.h"

/**
 * i915_driver_load - setup chip and create an initial config
 * @dev: DRM device
 * @flags: startup flags
 *
 * The driver load routine has to do several things:
 *   - drive output discovery via intel_modeset_init()
 *   - initialize the memory manager
 *   - allocate initial config memory
 *   - setup the DRM framebuffer with the allocated memory
 */
int i915_driver_load(drm_device_t *dev, unsigned long flags)
{
	drm_i915_private_t *dev_priv;
	drm_i915_init_t init;
	drm_buffer_object_t *entry;
	struct drm_framebuffer *fb;
	int ret;

	dev_priv = drm_alloc(sizeof(drm_i915_private_t), DRM_MEM_DRIVER);
	if (dev_priv == NULL)
		return DRM_ERR(ENOMEM);

	memset(dev_priv, 0, sizeof(drm_i915_private_t));
	dev->dev_private = (void *)dev_priv;
//	dev_priv->flags = flags;

	/* i915 has 4 more counters */
	dev->counters += 4;
	dev->types[6] = _DRM_STAT_IRQ;
	dev->types[7] = _DRM_STAT_PRIMARY;
	dev->types[8] = _DRM_STAT_SECONDARY;
	dev->types[9] = _DRM_STAT_DMA;

	if (IS_I9XX(dev)) {
		dev_priv->mmiobase = drm_get_resource_start(dev, 0);
		dev_priv->mmiolen = drm_get_resource_len(dev, 0);
		dev->mode_config.fb_base = dev_priv->baseaddr = drm_get_resource_start(dev, 2) & 0xff000000;
	} else if (drm_get_resource_start(dev, 1)) {
		dev_priv->mmiobase = drm_get_resource_start(dev, 1);
		dev_priv->mmiolen = drm_get_resource_len(dev, 1);
		dev->mode_config.fb_base = dev_priv->baseaddr = drm_get_resource_start(dev, 0) & 0xff000000;
	} else {
		DRM_ERROR("Unable to find MMIO registers\n");
		return -ENODEV;
	}

	ret = drm_addmap(dev, dev_priv->mmiobase, dev_priv->mmiolen,
			 _DRM_REGISTERS, _DRM_READ_ONLY, &dev_priv->mmio_map);
	if (ret != 0) {
		DRM_ERROR("Cannot add mapping for MMIO registers\n");
		return ret;
	}

	ret = drm_setup(dev);
	if (ret) {
		DRM_ERROR("drm_setup failed\n");
		return ret;
	}

	DRM_GETSAREA();
	if (!dev_priv->sarea) {
		DRM_ERROR("can not find sarea!\n");
		dev->dev_private = (void *)dev_priv;
		i915_dma_cleanup(dev);
		return DRM_ERR(EINVAL);
	}

	/* FIXME: assume sarea_priv is right after SAREA */
        dev_priv->sarea_priv = dev_priv->sarea->handle + sizeof(drm_sarea_t);

	/*
	 * Initialize the memory manager for local and AGP space
	 */
	drm_bo_driver_init(dev);
	/* FIXME: initial stolen area 8M init */
#define SCANOUT_SIZE 1024*1024*8 /* big enough for 2048x1024 32bpp */
	drm_bo_init_mm(dev, DRM_BO_MEM_PRIV0, dev->mode_config.fb_base,
		       SCANOUT_SIZE);

	/* Allocate scanout buffer and command ring */
	/* FIXME: types and other args correct? */
	drm_buffer_object_create(dev, SCANOUT_SIZE, drm_bo_type_dc,
				 DRM_BO_FLAG_READ | DRM_BO_FLAG_WRITE |
				 DRM_BO_FLAG_MEM_PRIV0 | DRM_BO_FLAG_NO_MOVE,
				 0, PAGE_SIZE, 0,
				 &entry);

	DRM_DEBUG("allocated bo, start: 0x%lx, offset: 0x%lx\n",
		  entry->buffer_start, entry->offset);
	intel_modeset_init(dev);

	fb = drm_framebuffer_create(dev);
	if (!fb) {
		DRM_ERROR("failed to allocate fb\n");
		return -EINVAL;
	}

	fb->width = 1024;
	fb->height = 768;
	fb->pitch = 1024;
	fb->bits_per_pixel = 32;
	fb->depth = 32;
	fb->offset = entry->offset;
	fb->bo = entry;

	drm_initial_config(dev, fb, false);
	drmfb_probe(dev, fb);
        drm_set_desired_modes(dev);

#if 0
	/* FIXME: command ring needs AGP space, do we own it at this point? */
	dev_priv->ring.Start = dev_priv->baseaddr;
	dev_priv->ring.End = 128*1024;
	dev_priv->ring.Size = 128*1024;
	dev_priv->ring.tail_mask = dev_priv->ring.Size - 1;

	dev_priv->ring.map.offset = dev_priv->ring.Start;
	dev_priv->ring.map.size = dev_priv->ring.Size;
	dev_priv->ring.map.type = 0;
	dev_priv->ring.map.flags = 0;
	dev_priv->ring.map.mtrr = 0;

	drm_core_ioremap(&dev_priv->ring.map, dev);

	if (dev_priv->ring.map.handle == NULL) {
		dev->dev_private = (void *)dev_priv;
		i915_dma_cleanup(dev);
		DRM_ERROR("can not ioremap virtual address for"
			  " ring buffer\n");
		return DRM_ERR(ENOMEM);
	}

	dev_priv->ring.virtual_start = dev_priv->ring.map.handle;
	dev_priv->cpp = 4;
	dev_priv->sarea_priv->pf_current_page = 0;

	/* We are using separate values as placeholders for mechanisms for
	 * private backbuffer/depthbuffer usage.
	 */
	dev_priv->use_mi_batchbuffer_start = 0;

	/* Allow hardware batchbuffers unless told otherwise.
	 */
	dev_priv->allow_batchbuffer = 1;

	/* Program Hardware Status Page */
	dev_priv->status_page_dmah = drm_pci_alloc(dev, PAGE_SIZE, PAGE_SIZE, 
	    0xffffffff);

	if (!dev_priv->status_page_dmah) {
		dev->dev_private = (void *)dev_priv;
		i915_dma_cleanup(dev);
		DRM_ERROR("Can not allocate hardware status page\n");
		return DRM_ERR(ENOMEM);
	}
	dev_priv->hw_status_page = dev_priv->status_page_dmah->vaddr;
	dev_priv->dma_status_page = dev_priv->status_page_dmah->busaddr;
	
	memset(dev_priv->hw_status_page, 0, PAGE_SIZE);
	DRM_DEBUG("hw status page @ %p\n", dev_priv->hw_status_page);

	I915_WRITE(0x02080, dev_priv->dma_status_page);
	DRM_DEBUG("Enabled hardware status page\n");
#endif

	return 0;
}

int i915_driver_unload(drm_device_t *dev)
{
	drm_i915_private_t *dev_priv = dev->dev_private;
	struct drm_framebuffer *fb;

	/* FIXME: remove framebuffer */
	intel_modeset_cleanup(dev);
	drm_free(dev_priv, sizeof(*dev_priv), DRM_MEM_DRIVER);

	dev->dev_private = NULL;
	return 0;
}

void i915_driver_lastclose(drm_device_t * dev)
{
	drm_i915_private_t *dev_priv = dev->dev_private;
	
	i915_mem_takedown(&(dev_priv->agp_heap));

	i915_dma_cleanup(dev);

	dev_priv->mmio_map = NULL;
}

void i915_driver_preclose(drm_device_t * dev, DRMFILE filp)
{
	drm_i915_private_t *dev_priv = dev->dev_private;
	i915_mem_release(dev, filp, dev_priv->agp_heap);
}

