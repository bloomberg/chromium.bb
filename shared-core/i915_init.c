/*
 * Copyright (c) 2007 Intel Corporation
 *   Jesse Barnes <jesse.barnes@intel.com>
 *
 * Copyright © 2002, 2003 David Dawes <dawes@xfree86.org>
 *                   2004 Sylvain Meyer
 *
 * GPL/BSD dual license
 */
#include "drmP.h"
#include "drm.h"
#include "drm_sarea.h"
#include "i915_drm.h"
#include "i915_drv.h"

/**
 * i915_probe_agp - get AGP bootup configuration
 * @pdev: PCI device
 * @aperture_size: returns AGP aperture configured size
 * @preallocated_size: returns size of BIOS preallocated AGP space
 *
 * Since Intel integrated graphics are UMA, the BIOS has to set aside
 * some RAM for the framebuffer at early boot.  This code figures out
 * how much was set aside so we can use it for our own purposes.
 */
int i915_probe_agp(struct pci_dev *pdev, unsigned long *aperture_size,
		   unsigned long *preallocated_size)
{
	struct pci_dev *bridge_dev;
	u16 tmp = 0;
	unsigned long overhead;

	bridge_dev = pci_get_bus_and_slot(0, PCI_DEVFN(0,0));
	if (!bridge_dev) {
		DRM_ERROR("bridge device not found\n");
		return -1;
	}

	/* Get the fb aperture size and "stolen" memory amount. */
	pci_read_config_word(bridge_dev, INTEL_GMCH_CTRL, &tmp);
	pci_dev_put(bridge_dev);

	*aperture_size = 1024 * 1024;
	*preallocated_size = 1024 * 1024;

	switch (pdev->device) {
	case PCI_DEVICE_ID_INTEL_82830_CGC:
	case PCI_DEVICE_ID_INTEL_82845G_IG:
	case PCI_DEVICE_ID_INTEL_82855GM_IG:
	case PCI_DEVICE_ID_INTEL_82865_IG:
		if ((tmp & INTEL_GMCH_MEM_MASK) == INTEL_GMCH_MEM_64M)
			*aperture_size *= 64;
		else
			*aperture_size *= 128;
		break;
	default:
		/* 9xx supports large sizes, just look at the length */
		*aperture_size = pci_resource_len(pdev, 2);
		break;
	}

	/*
	 * Some of the preallocated space is taken by the GTT
	 * and popup.  GTT is 1K per MB of aperture size, and popup is 4K.
	 */
	overhead = (*aperture_size / 1024) + 4096;
	switch (tmp & INTEL_855_GMCH_GMS_MASK) {
	case INTEL_855_GMCH_GMS_STOLEN_1M:
		break; /* 1M already */
	case INTEL_855_GMCH_GMS_STOLEN_4M:
		*preallocated_size *= 4;
		break;
	case INTEL_855_GMCH_GMS_STOLEN_8M:
		*preallocated_size *= 8;
		break;
	case INTEL_855_GMCH_GMS_STOLEN_16M:
		*preallocated_size *= 16;
		break;
	case INTEL_855_GMCH_GMS_STOLEN_32M:
		*preallocated_size *= 32;
		break;
	case INTEL_915G_GMCH_GMS_STOLEN_48M:
		*preallocated_size *= 48;
		break;
	case INTEL_915G_GMCH_GMS_STOLEN_64M:
		*preallocated_size *= 64;
		break;
	case INTEL_855_GMCH_GMS_DISABLED:
		DRM_ERROR("video memory is disabled\n");
		return -1;
	default:
		DRM_ERROR("unexpected GMCH_GMS value: 0x%02x\n",
			tmp & INTEL_855_GMCH_GMS_MASK);
		return -1;
	}
	*preallocated_size -= overhead;

	return 0;
}

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
int i915_driver_load(struct drm_device *dev, unsigned long flags)
{
	struct drm_i915_private *dev_priv;
	unsigned long agp_size, prealloc_size;
	int size, ret;

	dev_priv = drm_alloc(sizeof(struct drm_i915_private), DRM_MEM_DRIVER);
	if (dev_priv == NULL)
		return -ENOMEM;

	memset(dev_priv, 0, sizeof(struct drm_i915_private));
	dev->dev_private = (void *)dev_priv;
//	dev_priv->flags = flags;

	/* i915 has 4 more counters */
	dev->counters += 4;
	dev->types[6] = _DRM_STAT_IRQ;
	dev->types[7] = _DRM_STAT_PRIMARY;
	dev->types[8] = _DRM_STAT_SECONDARY;
	dev->types[9] = _DRM_STAT_DMA;

	if (IS_I9XX(dev)) {
		pci_read_config_dword(dev->pdev, 0x5C, &dev_priv->stolen_base);
		DRM_DEBUG("stolen base %p\n", (void*)dev_priv->stolen_base);
	}

	if (IS_I9XX(dev)) {
		dev_priv->mmiobase = drm_get_resource_start(dev, 0);
		dev_priv->mmiolen = drm_get_resource_len(dev, 0);
		dev->mode_config.fb_base =
			drm_get_resource_start(dev, 2) & 0xff000000;
	} else if (drm_get_resource_start(dev, 1)) {
		dev_priv->mmiobase = drm_get_resource_start(dev, 1);
		dev_priv->mmiolen = drm_get_resource_len(dev, 1);
		dev->mode_config.fb_base =
			drm_get_resource_start(dev, 0) & 0xff000000;
	} else {
		DRM_ERROR("Unable to find MMIO registers\n");
		return -ENODEV;
	}

	DRM_DEBUG("fb_base: 0x%08lx\n", dev->mode_config.fb_base);

	ret = drm_addmap(dev, dev_priv->mmiobase, dev_priv->mmiolen,
			 _DRM_REGISTERS, _DRM_KERNEL|_DRM_READ_ONLY|_DRM_DRIVER, &dev_priv->mmio_map);
	if (ret != 0) {
		DRM_ERROR("Cannot add mapping for MMIO registers\n");
		return ret;
	}

#ifdef __linux__
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,25)
        intel_init_chipset_flush_compat(dev);
#endif
#endif

	/*
	 * Initialize the memory manager for local and AGP space
	 */
	drm_bo_driver_init(dev);

	if (drm_core_check_feature(dev, DRIVER_MODESET)) {
		i915_probe_agp(dev->pdev, &agp_size, &prealloc_size);
		printk("setting up %ld bytes of VRAM space\n", prealloc_size);
		printk("setting up %ld bytes of TT space\n", (agp_size - prealloc_size));
		drm_bo_init_mm(dev, DRM_BO_MEM_VRAM, 0, prealloc_size >> PAGE_SHIFT, 1);
		drm_bo_init_mm(dev, DRM_BO_MEM_TT, prealloc_size >> PAGE_SHIFT, (agp_size - prealloc_size) >> PAGE_SHIFT, 1);
		
		I915_WRITE(LP_RING + RING_LEN, 0);
		I915_WRITE(LP_RING + RING_HEAD, 0);
		I915_WRITE(LP_RING + RING_TAIL, 0);

		size = PRIMARY_RINGBUFFER_SIZE;
		ret = drm_buffer_object_create(dev, size, drm_bo_type_kernel,
					       DRM_BO_FLAG_READ | DRM_BO_FLAG_WRITE |
					       DRM_BO_FLAG_MEM_VRAM |
					       DRM_BO_FLAG_NO_EVICT,
					       DRM_BO_HINT_DONT_FENCE, 0x1, 0,
					       &dev_priv->ring_buffer);
		if (ret < 0) {
			DRM_ERROR("Unable to allocate or pin ring buffer\n");
			return -EINVAL;
		}
		
		/* remap the buffer object properly */
		dev_priv->ring.Start = dev_priv->ring_buffer->offset;
		dev_priv->ring.End = dev_priv->ring.Start + size;
		dev_priv->ring.Size = size;
		dev_priv->ring.tail_mask = dev_priv->ring.Size - 1;

		/* FIXME: need wrapper with PCI mem checks */
		ret = drm_mem_reg_ioremap(dev, &dev_priv->ring_buffer->mem,
					  (void **) &dev_priv->ring.virtual_start);
		if (ret)
			DRM_ERROR("error mapping ring buffer: %d\n", ret);
		
		DRM_DEBUG("ring start %08lX, %p, %08lX\n", dev_priv->ring.Start,
			  dev_priv->ring.virtual_start, dev_priv->ring.Size);

	//

		memset((void *)(dev_priv->ring.virtual_start), 0, dev_priv->ring.Size);
		
		I915_WRITE(LP_RING + RING_START, dev_priv->ring.Start);
		I915_WRITE(LP_RING + RING_LEN,
			   ((dev_priv->ring.Size - 4096) & RING_NR_PAGES) |
			   (RING_NO_REPORT | RING_VALID));

		/* We are using separate values as placeholders for mechanisms for
		 * private backbuffer/depthbuffer usage.
		 */
		dev_priv->use_mi_batchbuffer_start = 0;
		
		/* Allow hardware batchbuffers unless told otherwise.
		 */
		dev_priv->allow_batchbuffer = 1;

		/* Program Hardware Status Page */
		if (!IS_G33(dev)) {
			dev_priv->status_page_dmah = 
				drm_pci_alloc(dev, PAGE_SIZE, PAGE_SIZE, 0xffffffff);
			
			if (!dev_priv->status_page_dmah) {
				dev->dev_private = (void *)dev_priv;
				i915_dma_cleanup(dev);
				DRM_ERROR("Can not allocate hardware status page\n");
				return -ENOMEM;
			}
			dev_priv->hw_status_page = dev_priv->status_page_dmah->vaddr;
			dev_priv->dma_status_page = dev_priv->status_page_dmah->busaddr;
			
			memset(dev_priv->hw_status_page, 0, PAGE_SIZE);
			
			I915_WRITE(I915REG_HWS_PGA, dev_priv->dma_status_page);
		}
		DRM_DEBUG("Enabled hardware status page\n");

		dev_priv->wq = create_singlethread_workqueue("i915");
		if (dev_priv == 0) {
		  DRM_DEBUG("Error\n");
		}

		intel_modeset_init(dev);
		drm_initial_config(dev, false);

		drm_mm_print(&dev->bm.man[DRM_BO_MEM_VRAM].manager, "VRAM");
		drm_mm_print(&dev->bm.man[DRM_BO_MEM_TT].manager, "TT");

		drm_irq_install(dev);
	}

	return 0;
}

int i915_driver_unload(struct drm_device *dev)
{
	struct drm_i915_private *dev_priv = dev->dev_private;

	I915_WRITE(LP_RING + RING_LEN, 0);


	if (drm_core_check_feature(dev, DRIVER_MODESET)) {
		drm_irq_uninstall(dev);
		intel_modeset_cleanup(dev);
	}

#if 0
	if (dev_priv->ring.virtual_start) {
		drm_core_ioremapfree(&dev_priv->ring.map, dev);
	}
#endif
	if (dev_priv->sarea_kmap.virtual) {
		drm_bo_kunmap(&dev_priv->sarea_kmap);
		dev_priv->sarea_kmap.virtual = NULL;
		dev->primary->master->lock.hw_lock = NULL;
		dev->sigdata.lock = NULL;
	}

	if (dev_priv->sarea_bo) {
		mutex_lock(&dev->struct_mutex);
		drm_bo_usage_deref_locked(&dev_priv->sarea_bo);
		mutex_unlock(&dev->struct_mutex);
		dev_priv->sarea_bo = NULL;
	}

	if (dev_priv->status_page_dmah) {
		drm_pci_free(dev, dev_priv->status_page_dmah);
		dev_priv->status_page_dmah = NULL;
		dev_priv->hw_status_page = NULL;
		dev_priv->dma_status_page = 0;
		/* Need to rewrite hardware status page */
		I915_WRITE(I915REG_HWS_PGA, 0x1ffff000);
	}

	if (dev_priv->status_gfx_addr) {
		dev_priv->status_gfx_addr = 0;
		drm_core_ioremapfree(&dev_priv->hws_map, dev);
		I915_WRITE(I915REG_HWS_PGA, 0x1ffff000);
	}

	if (drm_core_check_feature(dev, DRIVER_MODESET)) {
		drm_mem_reg_iounmap(dev, &dev_priv->ring_buffer->mem,
				    dev_priv->ring.virtual_start);

		DRM_DEBUG("usage is %d\n", atomic_read(&dev_priv->ring_buffer->usage));
		mutex_lock(&dev->struct_mutex);
		drm_bo_usage_deref_locked(&dev_priv->ring_buffer);

		if (drm_bo_clean_mm(dev, DRM_BO_MEM_TT, 1)) {
			DRM_ERROR("Memory manager type 3 not clean. "
				  "Delaying takedown\n");
		}
		if (drm_bo_clean_mm(dev, DRM_BO_MEM_VRAM, 1)) {
			DRM_ERROR("Memory manager type 3 not clean. "
				  "Delaying takedown\n");
		}
		mutex_unlock(&dev->struct_mutex);
	}

	drm_bo_driver_finish(dev);

#ifdef __linux__
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,25)
        intel_init_chipset_flush_compat(dev);
#endif
#endif

        DRM_DEBUG("%p\n", dev_priv->mmio_map);
        drm_rmmap(dev, dev_priv->mmio_map);

	drm_free(dev_priv, sizeof(*dev_priv), DRM_MEM_DRIVER);

	dev->dev_private = NULL;
	return 0;
}

int i915_master_create(struct drm_device *dev, struct drm_master *master)
{
	struct drm_i915_master_private *master_priv;
	unsigned long sareapage;
	int ret;

	master_priv = drm_calloc(1, sizeof(*master_priv), DRM_MEM_DRIVER);
	if (!master_priv)
		return -ENOMEM;

	/* prebuild the SAREA */
	sareapage = max(SAREA_MAX, PAGE_SIZE);
	ret = drm_addmap(dev, 0, sareapage, _DRM_SHM, _DRM_CONTAINS_LOCK|_DRM_DRIVER,
			 &master_priv->sarea);
	if (ret) {
		DRM_ERROR("SAREA setup failed\n");
		return ret;
	}
	master_priv->sarea_priv = master_priv->sarea->handle + sizeof(struct drm_sarea);
	master_priv->sarea_priv->pf_current_page = 0;

	master->driver_priv = master_priv;
	return 0;
}

void i915_master_destroy(struct drm_device *dev, struct drm_master *master)
{
	struct drm_i915_master_private *master_priv = master->driver_priv;

	if (!master_priv)
		return;

        drm_rmmap(dev, master_priv->sarea);
	drm_free(master_priv, sizeof(*master_priv), DRM_MEM_DRIVER);

	master->driver_priv = NULL;
}

void i915_driver_preclose(struct drm_device * dev, struct drm_file *file_priv)
{
        struct drm_i915_private *dev_priv = dev->dev_private;
	if (drm_core_check_feature(dev, DRIVER_MODESET))
		i915_mem_release(dev, file_priv, dev_priv->agp_heap);
}

void i915_driver_lastclose(struct drm_device * dev)
{
	struct drm_i915_private *dev_priv = dev->dev_private;

	if (drm_core_check_feature(dev, DRIVER_MODESET))
		return;

	if (dev_priv->agp_heap)
		i915_mem_takedown(&(dev_priv->agp_heap));
	
	i915_dma_cleanup(dev);
}

int i915_driver_firstopen(struct drm_device *dev)
{
	if (drm_core_check_feature(dev, DRIVER_MODESET))
		return 0;

	drm_bo_driver_init(dev);
	return 0;
}
