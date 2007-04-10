#include "drmP.h"
#include "drm.h"
#include "drm_sarea.h"
#include "i915_drm.h"
#include "i915_drv.h"

int i915_driver_load(drm_device_t *dev, unsigned long flags)
{
	drm_i915_private_t *dev_priv;
	drm_i915_init_t init;
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
		dev_priv->baseaddr = drm_get_resource_start(dev, 2) & 0xff000000;
	} else if (drm_get_resource_start(dev, 1)) {
		dev_priv->mmiobase = drm_get_resource_start(dev, 1);
		dev_priv->mmiolen = drm_get_resource_len(dev, 1);
		dev_priv->baseaddr = drm_get_resource_start(dev, 0) & 0xff000000;
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

	/* FIXME: where does the sarea_priv really go? */
	dev_priv->sarea_priv = kmalloc(sizeof(drm_i915_sarea_t), GFP_KERNEL);

	/* FIXME: need real front buffer offset */
	dev_priv->sarea_priv->front_handle = 0xa0000000 + 1024*1024;

	drm_bo_driver_init(dev);
	/* this probably doesn't belong here - TODO */
	//drm_framebuffer_set_object(dev, dev_priv->sarea_priv->front_handle);
	intel_modeset_init(dev);
	drm_set_desired_modes(dev);

	/* FIXME: command ring needs AGP space, do we own it at this point? */
	dev_priv->ring.Start = 0xa0000000;
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

	return 0;
}

int i915_driver_unload(drm_device_t *dev)
{
	drm_i915_private_t *dev_priv = dev->dev_private;

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

