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
#include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <assert.h>
#include <errno.h>
#include <sys/mman.h>
#include <fcntl.h>

#include <xf86drm.h>
#include <xf86atomic.h>
#include "libdrm_lists.h"
#include "nouveau_drm.h"

#include "nouveau.h"
#include "private.h"

#ifdef DEBUG
uint32_t nouveau_debug = 0;

static void
debug_init(char *args)
{
	if (args) {
		int n = strtol(args, NULL, 0);
		if (n >= 0)
			nouveau_debug = n;
	}
}
#endif

/* this is the old libdrm's version of nouveau_device_wrap(), the symbol
 * is kept here to prevent AIGLX from crashing if the DDX is linked against
 * the new libdrm, but the DRI driver against the old
 */
int
nouveau_device_open_existing(struct nouveau_device **pdev, int close, int fd,
			     drm_context_t ctx)
{
	return -EACCES;
}

int
nouveau_device_wrap(int fd, int close, struct nouveau_device **pdev)
{
	struct nouveau_device_priv *nvdev = calloc(1, sizeof(*nvdev));
	struct nouveau_device *dev = &nvdev->base;
	uint64_t chipset, vram, gart, bousage;
	drmVersionPtr ver;
	int ret;
	char *tmp;

#ifdef DEBUG
	debug_init(getenv("NOUVEAU_LIBDRM_DEBUG"));
#endif

	if (!nvdev)
		return -ENOMEM;
	nvdev->base.fd = fd;

	ver = drmGetVersion(fd);
	if (ver) dev->drm_version = (ver->version_major << 24) |
				    (ver->version_minor << 8) |
				     ver->version_patchlevel;
	drmFreeVersion(ver);

	if ( dev->drm_version != 0x00000010 &&
	    (dev->drm_version <  0x01000000 ||
	     dev->drm_version >= 0x02000000)) {
		nouveau_device_del(&dev);
		return -EINVAL;
	}

	ret = nouveau_getparam(dev, NOUVEAU_GETPARAM_CHIPSET_ID, &chipset);
	if (ret == 0)
	ret = nouveau_getparam(dev, NOUVEAU_GETPARAM_FB_SIZE, &vram);
	if (ret == 0)
	ret = nouveau_getparam(dev, NOUVEAU_GETPARAM_AGP_SIZE, &gart);
	if (ret) {
		nouveau_device_del(&dev);
		return ret;
	}

	ret = nouveau_getparam(dev, NOUVEAU_GETPARAM_HAS_BO_USAGE, &bousage);
	if (ret == 0)
		nvdev->have_bo_usage = (bousage != 0);

	nvdev->close = close;

	tmp = getenv("NOUVEAU_LIBDRM_VRAM_LIMIT_PERCENT");
	if (tmp)
		nvdev->vram_limit_percent = atoi(tmp);
	else
		nvdev->vram_limit_percent = 80;
	tmp = getenv("NOUVEAU_LIBDRM_GART_LIMIT_PERCENT");
	if (tmp)
		nvdev->gart_limit_percent = atoi(tmp);
	else
		nvdev->gart_limit_percent = 80;
	DRMINITLISTHEAD(&nvdev->bo_list);
	nvdev->base.object.oclass = NOUVEAU_DEVICE_CLASS;
	nvdev->base.lib_version = 0x01000000;
	nvdev->base.chipset = chipset;
	nvdev->base.vram_size = vram;
	nvdev->base.gart_size = gart;
	nvdev->base.vram_limit =
		(nvdev->base.vram_size * nvdev->vram_limit_percent) / 100;
	nvdev->base.gart_limit =
		(nvdev->base.gart_size * nvdev->gart_limit_percent) / 100;

	*pdev = &nvdev->base;
	return 0;
}

int
nouveau_device_open(const char *busid, struct nouveau_device **pdev)
{
	int ret = -ENODEV, fd = drmOpen("nouveau", busid);
	if (fd >= 0) {
		ret = nouveau_device_wrap(fd, 1, pdev);
		if (ret)
			drmClose(fd);
	}
	return ret;
}

void
nouveau_device_del(struct nouveau_device **pdev)
{
	struct nouveau_device_priv *nvdev = nouveau_device(*pdev);
	if (nvdev) {
		if (nvdev->close)
			drmClose(nvdev->base.fd);
		free(nvdev->client);
		free(nvdev);
		*pdev = NULL;
	}
}

int
nouveau_getparam(struct nouveau_device *dev, uint64_t param, uint64_t *value)
{
	struct drm_nouveau_getparam r = { param, 0 };
	int fd = dev->fd, ret =
		drmCommandWriteRead(fd, DRM_NOUVEAU_GETPARAM, &r, sizeof(r));
	*value = r.value;
	return ret;
}

int
nouveau_setparam(struct nouveau_device *dev, uint64_t param, uint64_t value)
{
	struct drm_nouveau_setparam r = { param, value };
	return drmCommandWrite(dev->fd, DRM_NOUVEAU_SETPARAM, &r, sizeof(r));
}

int
nouveau_client_new(struct nouveau_device *dev, struct nouveau_client **pclient)
{
	struct nouveau_device_priv *nvdev = nouveau_device(dev);
	struct nouveau_client_priv *pcli;
	int id = 0, i, ret = -ENOMEM;
	uint32_t *clients;

	for (i = 0; i < nvdev->nr_client; i++) {
		id = ffs(nvdev->client[i]) - 1;
		if (id >= 0)
			goto out;
	}

	clients = realloc(nvdev->client, sizeof(uint32_t) * (i + 1));
	if (!clients)
		return ret;
	nvdev->client = clients;
	nvdev->client[i] = 0;
	nvdev->nr_client++;

out:
	pcli = calloc(1, sizeof(*pcli));
	if (pcli) {
		nvdev->client[i] |= (1 << id);
		pcli->base.device = dev;
		pcli->base.id = (i * 32) + id;
		ret = 0;
	}

	*pclient = &pcli->base;
	return ret;
}

void
nouveau_client_del(struct nouveau_client **pclient)
{
	struct nouveau_client_priv *pcli = nouveau_client(*pclient);
	struct nouveau_device_priv *nvdev;
	if (pcli) {
		int id = pcli->base.id;
		nvdev = nouveau_device(pcli->base.device);
		nvdev->client[id / 32] &= ~(1 << (id % 32));
		free(pcli->kref);
		free(pcli);
	}
}

int
nouveau_object_new(struct nouveau_object *parent, uint64_t handle,
		   uint32_t oclass, void *data, uint32_t length,
		   struct nouveau_object **pobj)
{
	struct nouveau_device *dev;
	struct nouveau_object *obj;
	int ret = -EINVAL;

	if (length == 0)
		length = sizeof(struct nouveau_object *);
	obj = malloc(sizeof(*obj) + length);
	obj->parent = parent;
	obj->handle = handle;
	obj->oclass = oclass;
	obj->length = length;
	obj->data = obj + 1;
	if (data)
		memcpy(obj->data, data, length);
	*(struct nouveau_object **)obj->data = obj;

	dev = nouveau_object_find(obj, NOUVEAU_DEVICE_CLASS);
	switch (parent->oclass) {
	case NOUVEAU_DEVICE_CLASS:
		switch (obj->oclass) {
		case NOUVEAU_FIFO_CHANNEL_CLASS:
		{
			if (dev->chipset < 0xc0)
				ret = abi16_chan_nv04(obj);
			else
			if (dev->chipset < 0xe0)
				ret = abi16_chan_nvc0(obj);
			else
				ret = abi16_chan_nve0(obj);
		}
			break;
		default:
			break;
		}
		break;
	case NOUVEAU_FIFO_CHANNEL_CLASS:
		switch (obj->oclass) {
		case NOUVEAU_NOTIFIER_CLASS:
			ret = abi16_ntfy(obj);
			break;
		default:
			ret = abi16_engobj(obj);
			break;
		}
	default:
		break;
	}

	if (ret) {
		free(obj);
		return ret;
	}

	*pobj = obj;
	return 0;
}

void
nouveau_object_del(struct nouveau_object **pobj)
{
	struct nouveau_object *obj = *pobj;
	struct nouveau_device *dev;
	if (obj) {
		dev = nouveau_object_find(obj, NOUVEAU_DEVICE_CLASS);
		if (obj->oclass == NOUVEAU_FIFO_CHANNEL_CLASS) {
			struct drm_nouveau_channel_free req;
			req.channel = obj->handle;
			drmCommandWrite(dev->fd, DRM_NOUVEAU_CHANNEL_FREE,
					&req, sizeof(req));
		} else {
			struct drm_nouveau_gpuobj_free req;
			req.channel = obj->parent->handle;
			req.handle  = obj->handle;
			drmCommandWrite(dev->fd, DRM_NOUVEAU_GPUOBJ_FREE,
					&req, sizeof(req));
		}
	}
	free(obj);
	*pobj = NULL;
}

void *
nouveau_object_find(struct nouveau_object *obj, uint32_t pclass)
{
	while (obj && obj->oclass != pclass) {
		obj = obj->parent;
		if (pclass == NOUVEAU_PARENT_CLASS)
			break;
	}
	return obj;
}

static void
nouveau_bo_del(struct nouveau_bo *bo)
{
	struct nouveau_bo_priv *nvbo = nouveau_bo(bo);
	struct drm_gem_close req = { bo->handle };
	DRMLISTDEL(&nvbo->head);
	if (bo->map)
		munmap(bo->map, bo->size);
	drmIoctl(bo->device->fd, DRM_IOCTL_GEM_CLOSE, &req);
	free(nvbo);
}

int
nouveau_bo_new(struct nouveau_device *dev, uint32_t flags, uint32_t align,
	       uint64_t size, union nouveau_bo_config *config,
	       struct nouveau_bo **pbo)
{
	struct nouveau_device_priv *nvdev = nouveau_device(dev);
	struct nouveau_bo_priv *nvbo = calloc(1, sizeof(*nvbo));
	struct nouveau_bo *bo = &nvbo->base;
	int ret;

	if (!nvbo)
		return -ENOMEM;
	atomic_set(&nvbo->refcnt, 1);
	bo->device = dev;
	bo->flags = flags;
	bo->size = size;

	ret = abi16_bo_init(bo, align, config);
	if (ret) {
		free(nvbo);
		return ret;
	}

	DRMLISTADD(&nvbo->head, &nvdev->bo_list);

	*pbo = bo;
	return 0;
}

int
nouveau_bo_wrap(struct nouveau_device *dev, uint32_t handle,
		struct nouveau_bo **pbo)
{
	struct nouveau_device_priv *nvdev = nouveau_device(dev);
	struct drm_nouveau_gem_info req = { .handle = handle };
	struct nouveau_bo_priv *nvbo;
	int ret;

	DRMLISTFOREACHENTRY(nvbo, &nvdev->bo_list, head) {
		if (nvbo->base.handle == handle) {
			*pbo = NULL;
			nouveau_bo_ref(&nvbo->base, pbo);
			return 0;
		}
	}

	ret = drmCommandWriteRead(dev->fd, DRM_NOUVEAU_GEM_INFO,
				  &req, sizeof(req));
	if (ret)
		return ret;

	nvbo = calloc(1, sizeof(*nvbo));
	if (nvbo) {
		atomic_set(&nvbo->refcnt, 1);
		nvbo->base.device = dev;
		abi16_bo_info(&nvbo->base, &req);
		DRMLISTADD(&nvbo->head, &nvdev->bo_list);
		*pbo = &nvbo->base;
		return 0;
	}

	return -ENOMEM;
}

int
nouveau_bo_name_ref(struct nouveau_device *dev, uint32_t name,
		    struct nouveau_bo **pbo)
{
	struct nouveau_device_priv *nvdev = nouveau_device(dev);
	struct nouveau_bo_priv *nvbo;
	struct drm_gem_open req = { .name = name };
	int ret;

	DRMLISTFOREACHENTRY(nvbo, &nvdev->bo_list, head) {
		if (nvbo->name == name) {
			*pbo = NULL;
			nouveau_bo_ref(&nvbo->base, pbo);
			return 0;
		}
	}

	ret = drmIoctl(dev->fd, DRM_IOCTL_GEM_OPEN, &req);
	if (ret == 0) {
		ret = nouveau_bo_wrap(dev, req.handle, pbo);
		nouveau_bo((*pbo))->name = name;
	}

	return ret;
}

int
nouveau_bo_name_get(struct nouveau_bo *bo, uint32_t *name)
{
	struct drm_gem_flink req = { .handle = bo->handle };
	struct nouveau_bo_priv *nvbo = nouveau_bo(bo);
	if (!nvbo->name) {
		int ret = drmIoctl(bo->device->fd, DRM_IOCTL_GEM_FLINK, &req);
		if (ret)
			return ret;
		nvbo->name = req.name;
	}
	*name = nvbo->name;
	return 0;
}

void
nouveau_bo_ref(struct nouveau_bo *bo, struct nouveau_bo **pref)
{
	struct nouveau_bo *ref = *pref;
	if (bo) {
		atomic_inc(&nouveau_bo(bo)->refcnt);
	}
	if (ref) {
		if (atomic_dec_and_test(&nouveau_bo(ref)->refcnt))
			nouveau_bo_del(ref);
	}
	*pref = bo;
}

int
nouveau_bo_prime_handle_ref(struct nouveau_device *dev, int prime_fd,
			    struct nouveau_bo **bo)
{
	int ret;
	unsigned int handle;

	ret = drmPrimeFDToHandle(dev->fd, prime_fd, &handle);
	if (ret) {
		nouveau_bo_ref(NULL, bo);
		return ret;
	}

	ret = nouveau_bo_wrap(dev, handle, bo);
	if (ret) {
		nouveau_bo_ref(NULL, bo);
		return ret;
	}

	return 0;
}

int
nouveau_bo_set_prime(struct nouveau_bo *bo, int *prime_fd)
{
	struct nouveau_bo_priv *nvbo = nouveau_bo(bo);
	int ret;

	ret = drmPrimeHandleToFD(bo->device->fd, nvbo->base.handle, DRM_CLOEXEC, prime_fd);
	if (ret)
		return ret;
	return 0;
}

int
nouveau_bo_wait(struct nouveau_bo *bo, uint32_t access,
		struct nouveau_client *client)
{
	struct nouveau_bo_priv *nvbo = nouveau_bo(bo);
	struct drm_nouveau_gem_cpu_prep req;
	struct nouveau_pushbuf *push;
	int ret = 0;

	if (!(access & NOUVEAU_BO_RDWR))
		return 0;

	push = cli_push_get(client, bo);
	if (push && push->channel)
		nouveau_pushbuf_kick(push, push->channel);

	if (!nvbo->name && !(nvbo->access & NOUVEAU_BO_WR) &&
			   !(      access & NOUVEAU_BO_WR))
		return 0;

	req.handle = bo->handle;
	req.flags = 0;
	if (access & NOUVEAU_BO_WR)
		req.flags |= NOUVEAU_GEM_CPU_PREP_WRITE;
	if (access & NOUVEAU_BO_NOBLOCK)
		req.flags |= NOUVEAU_GEM_CPU_PREP_NOWAIT;

	ret = drmCommandWrite(bo->device->fd, DRM_NOUVEAU_GEM_CPU_PREP,
			      &req, sizeof(req));
	if (ret == 0)
		nvbo->access = 0;
	return ret;
}

int
nouveau_bo_map(struct nouveau_bo *bo, uint32_t access,
	       struct nouveau_client *client)
{
	struct nouveau_bo_priv *nvbo = nouveau_bo(bo);
	if (bo->map == NULL) {
		bo->map = mmap(0, bo->size, PROT_READ | PROT_WRITE,
			       MAP_SHARED, bo->device->fd, nvbo->map_handle);
		if (bo->map == MAP_FAILED) {
			bo->map = NULL;
			return -errno;
		}
	}
	return nouveau_bo_wait(bo, access, client);
}
