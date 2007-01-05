/* 
 * Copyright 2005 Stephane Marchesin
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

#include "drmP.h"
#include "drm.h"
#include "drm_sarea.h"
#include "nouveau_drv.h"

/* here a client dies, release the stuff that was allocated for its filp */
void nouveau_preclose(drm_device_t * dev, DRMFILE filp)
{
	drm_nouveau_private_t *dev_priv = dev->dev_private;

	nouveau_mem_release(filp,dev_priv->fb_heap);
	nouveau_mem_release(filp,dev_priv->agp_heap);
	nouveau_object_cleanup(dev, filp);
	nouveau_fifo_cleanup(dev, filp);
}

/* first module load, setup the mmio/fb mapping */
int nouveau_firstopen(struct drm_device *dev)
{
	int ret;
	drm_nouveau_private_t *dev_priv = dev->dev_private;

	/* resource 0 is mmio regs */
	/* resource 1 is linear FB */
	/* resource 2 is RAMIN (mmio regs + 0x1000000) */
	/* resource 6 is bios */

	/* map the mmio regs */
	ret = drm_addmap(dev, drm_get_resource_start(dev, 0), drm_get_resource_len(dev, 0), 
			_DRM_REGISTERS, _DRM_READ_ONLY, &dev_priv->mmio);
	if (dev_priv->mmio)
	{
		DRM_INFO("regs mapped ok at 0x%lx\n",dev_priv->mmio->offset);
	}
	else
	{
		DRM_ERROR("Unable to initialize the mmio mapping. Please report your setup to " DRIVER_EMAIL "\n");
		return 1; 
	}

	DRM_INFO("%lld MB of video ram detected\n",nouveau_mem_fb_amount(dev)>>20);

	/* Clear RAMIN
	 * Determine locations for RAMHT/FC/RO
	 * Initialise PFIFO
	 */
	ret = nouveau_fifo_init(dev);
	if (ret) return ret;

#if __OS_HAS_MTRR
	/* setup a mtrr over the FB */
	dev_priv->fb_mtrr=drm_mtrr_add(drm_get_resource_start(dev, 1),nouveau_mem_fb_amount(dev), DRM_MTRR_WC);
#endif

	/* FIXME: doesn't belong here, and have no idea what it's for.. */
	if (dev_priv->card_type >= NV_40)
		nv40_graph_init(dev);

	return 0;
}

int nouveau_load(struct drm_device *dev, unsigned long flags)
{
	drm_nouveau_private_t *dev_priv;

	if (flags==NV_UNKNOWN)
		return DRM_ERR(EINVAL);

	dev_priv = drm_alloc(sizeof(drm_nouveau_private_t), DRM_MEM_DRIVER);
	if (!dev_priv)                   
		return DRM_ERR(ENOMEM);

	memset(dev_priv, 0, sizeof(drm_nouveau_private_t));
	dev_priv->card_type=flags&NOUVEAU_FAMILY;
	dev_priv->flags=flags&NOUVEAU_FLAGS;

	dev->dev_private = (void *)dev_priv;

	return 0;
}

void nouveau_lastclose(struct drm_device *dev)
{
#if __OS_HAS_MTRR
	drm_nouveau_private_t *dev_priv = dev->dev_private;
	if(dev_priv->fb_mtrr>0)
		drm_mtrr_del(dev_priv->fb_mtrr, drm_get_resource_start(dev, 1),nouveau_mem_fb_amount(dev), DRM_MTRR_WC);
#endif
}

int nouveau_unload(struct drm_device *dev)
{
	drm_free(dev->dev_private, sizeof(*dev->dev_private), DRM_MEM_DRIVER);
	dev->dev_private = NULL;
	return 0;
}

int nouveau_ioctl_getparam(DRM_IOCTL_ARGS)
{
	DRM_DEVICE;
	drm_nouveau_private_t *dev_priv = dev->dev_private;
	drm_nouveau_getparam_t getparam;

	DRM_COPY_FROM_USER_IOCTL(getparam, (drm_nouveau_getparam_t __user *)data,
			sizeof(getparam));

	switch (getparam.param) {
	case NOUVEAU_GETPARAM_PCI_VENDOR:
		getparam.value=dev->pci_vendor;
		break;
	case NOUVEAU_GETPARAM_PCI_DEVICE:
		getparam.value=dev->pci_device;
		break;
	case NOUVEAU_GETPARAM_BUS_TYPE:
		if (drm_device_is_agp(dev))
			getparam.value=NV_AGP;
		else if (drm_device_is_pcie(dev))
			getparam.value=NV_PCIE;
		else
			getparam.value=NV_PCI;
		break;
	case NOUVEAU_GETPARAM_FB_PHYSICAL:
		getparam.value=dev_priv->fb_phys;
		break;
	case NOUVEAU_GETPARAM_AGP_PHYSICAL:
		getparam.value=dev_priv->agp_phys;
		break;
	default:
		DRM_ERROR("unknown parameter %d\n", getparam.param);
		return DRM_ERR(EINVAL);
	}

	DRM_COPY_TO_USER_IOCTL((drm_nouveau_getparam_t __user *)data, getparam,
			sizeof(getparam));
	return 0;
}

int nouveau_ioctl_setparam(DRM_IOCTL_ARGS)
{
	DRM_DEVICE;
	drm_nouveau_private_t *dev_priv = dev->dev_private;
	drm_nouveau_setparam_t setparam;

	DRM_COPY_FROM_USER_IOCTL(setparam, (drm_nouveau_setparam_t __user *)data,
			sizeof(setparam));

	switch (setparam.param) {
	case NOUVEAU_SETPARAM_CMDBUF_LOCATION:
		switch (setparam.value) {
		case NOUVEAU_MEM_AGP:
		case NOUVEAU_MEM_FB:
			break;
		default:
			DRM_ERROR("invalid CMDBUF_LOCATION value=%d\n", setparam.value);
			return DRM_ERR(EINVAL);
		}
		dev_priv->config.cmdbuf.location = setparam.value;
		break;
	case NOUVEAU_SETPARAM_CMDBUF_SIZE:
		dev_priv->config.cmdbuf.size = setparam.value;
		break;
	default:
		DRM_ERROR("unknown parameter %d\n", setparam.param);
		return DRM_ERR(EINVAL);
	}

	return 0;
}

/* waits for idle */
void nouveau_wait_for_idle(struct drm_device *dev)
{
	drm_nouveau_private_t *dev_priv=dev->dev_private;
	switch(dev_priv->card_type)
	{
		case NV_03:
			while(NV_READ(NV03_PGRAPH_STATUS));
			break;
		default:
			while(NV_READ(NV04_PGRAPH_STATUS));
			break;
	}
}

