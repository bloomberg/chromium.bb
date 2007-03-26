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
#include "nouveau_drm.h"

static int nouveau_init_card_mappings(drm_device_t *dev)
{
	drm_nouveau_private_t *dev_priv = dev->dev_private;
	int ret;

	/* resource 0 is mmio regs */
	/* resource 1 is linear FB */
	/* resource 2 is RAMIN (mmio regs + 0x1000000) */
	/* resource 6 is bios */

	/* map the mmio regs */
	ret = drm_addmap(dev, drm_get_resource_start(dev, 0),
			      drm_get_resource_len(dev, 0), 
			      _DRM_REGISTERS, _DRM_READ_ONLY, &dev_priv->mmio);
	if (ret) {
		DRM_ERROR("Unable to initialize the mmio mapping (%d). "
			  "Please report your setup to " DRIVER_EMAIL "\n",
			  ret);
		return 1;
	}
	DRM_DEBUG("regs mapped ok at 0x%lx\n", dev_priv->mmio->offset);

	/* map larger RAMIN aperture on NV40 cards */
	if (dev_priv->card_type >= NV_40) {
		int ramin_resource = 2;
		if (drm_get_resource_len(dev, ramin_resource) == 0)
			ramin_resource = 3;

		ret = drm_addmap(dev,
				 drm_get_resource_start(dev, ramin_resource),
				 drm_get_resource_len(dev, ramin_resource),
				 _DRM_REGISTERS, _DRM_READ_ONLY,
				 &dev_priv->ramin);
		if (ret) {
			DRM_ERROR("Failed to init RAMIN mapping, "
				  "limited instance memory available\n");
			dev_priv->ramin = NULL;
		}
	} else
		dev_priv->ramin = NULL;

	return 0;
}

static void nouveau_stub_takedown(drm_device_t *dev) {}
static int nouveau_init_engine_ptrs(drm_device_t *dev)
{
	drm_nouveau_private_t *dev_priv = dev->dev_private;
	struct nouveau_engine_func *engine = &dev_priv->Engine;

	switch (dev_priv->chipset & 0xf0) {
	case 0x00:
		engine->Mc.Init		= nv04_mc_init;
		engine->Mc.Takedown	= nv04_mc_takedown;
		engine->Timer.Init	= nv04_timer_init;
		engine->Timer.Takedown	= nv04_timer_takedown;
		engine->Fb.Init		= nv04_fb_init;
		engine->Fb.Takedown	= nv04_fb_takedown;
		engine->Graph.Init	= nv04_graph_init;
		engine->Graph.Takedown	= nv04_graph_takedown;
		engine->Fifo.Init	= nouveau_fifo_init;
		engine->Fifo.Takedown	= nouveau_stub_takedown;
		break;
	case 0x10:
		engine->Mc.Init		= nv04_mc_init;
		engine->Mc.Takedown	= nv04_mc_takedown;
		engine->Timer.Init	= nv04_timer_init;
		engine->Timer.Takedown	= nv04_timer_takedown;
		engine->Fb.Init		= nv10_fb_init;
		engine->Fb.Takedown	= nv10_fb_takedown;
		engine->Graph.Init	= nv10_graph_init;
		engine->Graph.Takedown	= nv10_graph_takedown;
		engine->Fifo.Init	= nouveau_fifo_init;
		engine->Fifo.Takedown	= nouveau_stub_takedown;
		break;
	case 0x20:
		engine->Mc.Init		= nv04_mc_init;
		engine->Mc.Takedown	= nv04_mc_takedown;
		engine->Timer.Init	= nv04_timer_init;
		engine->Timer.Takedown	= nv04_timer_takedown;
		engine->Fb.Init		= nv10_fb_init;
		engine->Fb.Takedown	= nv10_fb_takedown;
		engine->Graph.Init	= nv20_graph_init;
		engine->Graph.Takedown	= nv20_graph_takedown;
		engine->Fifo.Init	= nouveau_fifo_init;
		engine->Fifo.Takedown	= nouveau_stub_takedown;
		break;
	case 0x30:
		engine->Mc.Init		= nv04_mc_init;
		engine->Mc.Takedown	= nv04_mc_takedown;
		engine->Timer.Init	= nv04_timer_init;
		engine->Timer.Takedown	= nv04_timer_takedown;
		engine->Fb.Init		= nv10_fb_init;
		engine->Fb.Takedown	= nv10_fb_takedown;
		engine->Graph.Init	= nv30_graph_init;
		engine->Graph.Takedown	= nv30_graph_takedown;
		engine->Fifo.Init	= nouveau_fifo_init;
		engine->Fifo.Takedown	= nouveau_stub_takedown;
		break;
	case 0x40:
		engine->Mc.Init		= nv40_mc_init;
		engine->Mc.Takedown	= nv40_mc_takedown;
		engine->Timer.Init	= nv04_timer_init;
		engine->Timer.Takedown	= nv04_timer_takedown;
		engine->Fb.Init		= nv40_fb_init;
		engine->Fb.Takedown	= nv40_fb_takedown;
		engine->Graph.Init	= nv40_graph_init;
		engine->Graph.Takedown	= nv40_graph_takedown;
		engine->Fifo.Init	= nouveau_fifo_init;
		engine->Fifo.Takedown	= nouveau_stub_takedown;
		break;
	case 0x50:
	default:
		DRM_ERROR("NV%02x unsupported\n", dev_priv->chipset);
		return 1;
	}

	return 0;
}

static int nouveau_card_init(drm_device_t *dev)
{
	drm_nouveau_private_t *dev_priv = dev->dev_private;
	struct nouveau_engine_func *engine;
	int ret;

	/* Map any PCI resources we need on the card */
	ret = nouveau_init_card_mappings(dev);
	if (ret) return ret;

	/* Determine exact chipset we're running on */
	if (dev_priv->card_type < NV_10)
		dev_priv->chipset = dev_priv->card_type;
	else
		dev_priv->chipset =
			(NV_READ(NV03_PMC_BOOT_0) & 0x0ff00000) >> 20;

	/* Initialise internal driver API hooks */
	ret = nouveau_init_engine_ptrs(dev);
	if (ret) return ret;
	engine = &dev_priv->Engine;

	/* Initialise instance memory, must happen before mem_init so we
	 * know exactly how much VRAM we're able to use for "normal"
	 * purposes.
	 */
	ret = nouveau_instmem_init(dev);
	if (ret) return ret;

	/* Setup the memory manager */
	ret = nouveau_mem_init(dev);
	if (ret) return ret;

	/* Parse BIOS tables / Run init tables? */

	/* PMC */
	ret = engine->Mc.Init(dev);
	if (ret) return ret;

	/* PTIMER */
	ret = engine->Timer.Init(dev);
	if (ret) return ret;

	/* PFB */
	ret = engine->Fb.Init(dev);
	if (ret) return ret;

	/* PGRAPH */
	ret = engine->Graph.Init(dev);
	if (ret) return ret;

	/* PFIFO */
	ret = engine->Fifo.Init(dev);
	if (ret) return ret;

	/* what about PVIDEO/PCRTC/PRAMDAC etc? */

	return 0;
}

/* here a client dies, release the stuff that was allocated for its filp */
void nouveau_preclose(drm_device_t * dev, DRMFILE filp)
{
	drm_nouveau_private_t *dev_priv = dev->dev_private;

	nouveau_mem_release(filp,dev_priv->fb_heap);
	nouveau_mem_release(filp,dev_priv->agp_heap);
	nouveau_fifo_cleanup(dev, filp);
}

/* first module load, setup the mmio/fb mapping */
int nouveau_firstopen(struct drm_device *dev)
{
	int ret;

	ret = nouveau_card_init(dev);
	if (ret) {
		DRM_ERROR("nouveau_card_init() failed! (%d)\n", ret);
		return ret;
	}

	return 0;
}

int nouveau_load(struct drm_device *dev, unsigned long flags)
{
	drm_nouveau_private_t *dev_priv;
	int ret;

	if (flags==NV_UNKNOWN)
		return DRM_ERR(EINVAL);

	dev_priv = drm_alloc(sizeof(drm_nouveau_private_t), DRM_MEM_DRIVER);
	if (!dev_priv)                   
		return DRM_ERR(ENOMEM);

	memset(dev_priv, 0, sizeof(drm_nouveau_private_t));
	dev_priv->card_type=flags&NOUVEAU_FAMILY;
	dev_priv->flags=flags&NOUVEAU_FLAGS;

	dev->dev_private = (void *)dev_priv;

#if 0
	ret = nouveau_card_init(dev);
	if (ret) {
		DRM_ERROR("nouveau_card_init() failed! (%d)\n", ret);
		return ret;
	}
#endif

	return 0;
}

void nouveau_lastclose(struct drm_device *dev)
{
	drm_nouveau_private_t *dev_priv = dev->dev_private;
	if(dev_priv->fb_mtrr>0)
	{
		drm_mtrr_del(dev_priv->fb_mtrr, drm_get_resource_start(dev, 1),nouveau_mem_fb_amount(dev), DRM_MTRR_WC);
		dev_priv->fb_mtrr=0;
	}
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
	case NOUVEAU_GETPARAM_FB_SIZE:
		getparam.value=dev_priv->fb_available_size;
		break;
	case NOUVEAU_GETPARAM_AGP_SIZE:
		getparam.value=dev_priv->agp_available_size;
		break;
	default:
		DRM_ERROR("unknown parameter %lld\n", getparam.param);
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
			DRM_ERROR("invalid CMDBUF_LOCATION value=%lld\n",
					setparam.value);
			return DRM_ERR(EINVAL);
		}
		dev_priv->config.cmdbuf.location = setparam.value;
		break;
	case NOUVEAU_SETPARAM_CMDBUF_SIZE:
		dev_priv->config.cmdbuf.size = setparam.value;
		break;
	default:
		DRM_ERROR("unknown parameter %lld\n", setparam.param);
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


