/*
 * Copyright 2012 Red Hat Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE COPYRIGHT HOLDER(S) OR AUTHOR(S) BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 *
 * Authors: Ben Skeggs
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <stdlib.h>
#include <stdint.h>
#include <stddef.h>

#include "private.h"


drm_private int
abi16_chan_nv04(struct nouveau_object *obj)
{
	struct nouveau_device *dev = (struct nouveau_device *)obj->parent;
	struct nv04_fifo *nv04 = obj->data;
	struct drm_nouveau_channel_alloc req = {nv04->vram, nv04->gart};
	int ret;

	ret = drmCommandWriteRead(dev->fd, DRM_NOUVEAU_CHANNEL_ALLOC,
				  &req, sizeof(req));
	if (ret)
		return ret;

	nv04->base.channel = req.channel;
	nv04->base.pushbuf = req.pushbuf_domains;
	nv04->notify = req.notifier_handle;
	nv04->base.object->handle = req.channel;
	nv04->base.object->length = sizeof(*nv04);
	return 0;
}

drm_private int
abi16_chan_nvc0(struct nouveau_object *obj)
{
	struct nouveau_device *dev = (struct nouveau_device *)obj->parent;
	struct drm_nouveau_channel_alloc req = {};
	struct nvc0_fifo *nvc0 = obj->data;
	int ret;

	ret = drmCommandWriteRead(dev->fd, DRM_NOUVEAU_CHANNEL_ALLOC,
				  &req, sizeof(req));
	if (ret)
		return ret;

	nvc0->base.channel = req.channel;
	nvc0->base.pushbuf = req.pushbuf_domains;
	nvc0->notify = req.notifier_handle;
	nvc0->base.object->handle = req.channel;
	nvc0->base.object->length = sizeof(*nvc0);
	return 0;
}

drm_private int
abi16_chan_nve0(struct nouveau_object *obj)
{
	struct nouveau_device *dev = (struct nouveau_device *)obj->parent;
	struct drm_nouveau_channel_alloc req = {};
	struct nve0_fifo *nve0 = obj->data;
	int ret;

	if (obj->length > offsetof(struct nve0_fifo, engine)) {
		req.fb_ctxdma_handle = 0xffffffff;
		req.tt_ctxdma_handle = nve0->engine;
	}

	ret = drmCommandWriteRead(dev->fd, DRM_NOUVEAU_CHANNEL_ALLOC,
				  &req, sizeof(req));
	if (ret)
		return ret;

	nve0->base.channel = req.channel;
	nve0->base.pushbuf = req.pushbuf_domains;
	nve0->notify = req.notifier_handle;
	nve0->base.object->handle = req.channel;
	nve0->base.object->length = sizeof(*nve0);
	return 0;
}

drm_private int
abi16_engobj(struct nouveau_object *obj)
{
	struct drm_nouveau_grobj_alloc req = {
		obj->parent->handle, obj->handle, obj->oclass
	};
	struct nouveau_device *dev;
	int ret;

	dev = nouveau_object_find(obj, NOUVEAU_DEVICE_CLASS);
	ret = drmCommandWrite(dev->fd, DRM_NOUVEAU_GROBJ_ALLOC,
			      &req, sizeof(req));
	if (ret)
		return ret;

	obj->length = sizeof(struct nouveau_object *);
	return 0;
}

drm_private int
abi16_ntfy(struct nouveau_object *obj)
{
	struct nv04_notify *ntfy = obj->data;
	struct drm_nouveau_notifierobj_alloc req = {
		obj->parent->handle, ntfy->object->handle, ntfy->length
	};
	struct nouveau_device *dev;
	int ret;

	dev = nouveau_object_find(obj, NOUVEAU_DEVICE_CLASS);
	ret = drmCommandWriteRead(dev->fd, DRM_NOUVEAU_NOTIFIEROBJ_ALLOC,
				  &req, sizeof(req));
	if (ret)
		return ret;

	ntfy->offset = req.offset;
	ntfy->object->length = sizeof(*ntfy);
	return 0;
}

drm_private void
abi16_bo_info(struct nouveau_bo *bo, struct drm_nouveau_gem_info *info)
{
	struct nouveau_bo_priv *nvbo = nouveau_bo(bo);

	nvbo->map_handle = info->map_handle;
	bo->handle = info->handle;
	bo->size = info->size;
	bo->offset = info->offset;

	bo->flags = 0;
	if (info->domain & NOUVEAU_GEM_DOMAIN_VRAM)
		bo->flags |= NOUVEAU_BO_VRAM;
	if (info->domain & NOUVEAU_GEM_DOMAIN_GART)
		bo->flags |= NOUVEAU_BO_GART;
	if (!(info->tile_flags & NOUVEAU_GEM_TILE_NONCONTIG))
		bo->flags |= NOUVEAU_BO_CONTIG;
	if (nvbo->map_handle)
		bo->flags |= NOUVEAU_BO_MAP;

	if (bo->device->chipset >= 0xc0) {
		bo->config.nvc0.memtype   = (info->tile_flags & 0xff00) >> 8;
		bo->config.nvc0.tile_mode = info->tile_mode;
	} else
	if (bo->device->chipset >= 0x80 || bo->device->chipset == 0x50) {
		bo->config.nv50.memtype   = (info->tile_flags & 0x07f00) >> 8 |
					    (info->tile_flags & 0x30000) >> 9;
		bo->config.nv50.tile_mode = info->tile_mode << 4;
	} else {
		bo->config.nv04.surf_flags = info->tile_flags & 7;
		bo->config.nv04.surf_pitch = info->tile_mode;
	}
}

drm_private int
abi16_bo_init(struct nouveau_bo *bo, uint32_t alignment,
	      union nouveau_bo_config *config)
{
	struct nouveau_device *dev = bo->device;
	struct drm_nouveau_gem_new req = {};
	struct drm_nouveau_gem_info *info = &req.info;
	int ret;

	if (bo->flags & NOUVEAU_BO_VRAM)
		info->domain |= NOUVEAU_GEM_DOMAIN_VRAM;
	if (bo->flags & NOUVEAU_BO_GART)
		info->domain |= NOUVEAU_GEM_DOMAIN_GART;
	if (!info->domain)
		info->domain |= NOUVEAU_GEM_DOMAIN_VRAM |
				NOUVEAU_GEM_DOMAIN_GART;

	if (bo->flags & NOUVEAU_BO_MAP)
		info->domain |= NOUVEAU_GEM_DOMAIN_MAPPABLE;

	if (bo->flags & NOUVEAU_BO_COHERENT)
		info->domain |= NOUVEAU_GEM_DOMAIN_COHERENT;

	if (!(bo->flags & NOUVEAU_BO_CONTIG))
		info->tile_flags = NOUVEAU_GEM_TILE_NONCONTIG;

	info->size = bo->size;
	req.align = alignment;

	if (config) {
		if (dev->chipset >= 0xc0) {
			info->tile_flags = (config->nvc0.memtype & 0xff) << 8;
			info->tile_mode  = config->nvc0.tile_mode;
		} else
		if (dev->chipset >= 0x80 || dev->chipset == 0x50) {
			info->tile_flags = (config->nv50.memtype & 0x07f) << 8 |
					   (config->nv50.memtype & 0x180) << 9;
			info->tile_mode  = config->nv50.tile_mode >> 4;
		} else {
			info->tile_flags = config->nv04.surf_flags & 7;
			info->tile_mode  = config->nv04.surf_pitch;
		}
	}

	if (!nouveau_device(dev)->have_bo_usage)
		info->tile_flags &= 0x0000ff00;

	ret = drmCommandWriteRead(dev->fd, DRM_NOUVEAU_GEM_NEW,
				  &req, sizeof(req));
	if (ret == 0)
		abi16_bo_info(bo, &req.info);
	return ret;
}
