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
#include <strings.h>
#include <stdbool.h>
#include <assert.h>
#include <errno.h>
#include <fcntl.h>

#include <xf86drm.h>
#include <xf86atomic.h>
#include "libdrm_macros.h"
#include "libdrm_lists.h"
#include "nouveau_drm.h"

#include "nouveau.h"
#include "private.h"

#ifdef DEBUG
drm_private uint32_t nouveau_debug = 0;

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

int
nouveau_object_mthd(struct nouveau_object *obj,
		    uint32_t mthd, void *data, uint32_t size)
{
	return -ENODEV;
}

void
nouveau_object_sclass_put(struct nouveau_sclass **psclass)
{
	free(*psclass);
	*psclass = NULL;
}

int
nouveau_object_sclass_get(struct nouveau_object *obj,
			  struct nouveau_sclass **psclass)
{
	return abi16_sclass(obj, psclass);
}

int
nouveau_object_mclass(struct nouveau_object *obj,
		      const struct nouveau_mclass *mclass)
{
	struct nouveau_sclass *sclass;
	int ret = -ENODEV;
	int cnt, i, j;

	cnt = nouveau_object_sclass_get(obj, &sclass);
	if (cnt < 0)
		return cnt;

	for (i = 0; ret < 0 && mclass[i].oclass; i++) {
		for (j = 0; j < cnt; j++) {
			if (mclass[i].oclass  == sclass[j].oclass &&
			    mclass[i].version >= sclass[j].minver &&
			    mclass[i].version <= sclass[j].maxver) {
				ret = i;
				break;
			}
		}
	}

	nouveau_object_sclass_put(&sclass);
	return ret;
}

static void
nouveau_object_fini(struct nouveau_object *obj)
{
	if (obj->data) {
		abi16_delete(obj);
		free(obj->data);
		obj->data = NULL;
		return;
	}
}

static int
nouveau_object_init(struct nouveau_object *parent, uint32_t handle,
		    int32_t oclass, void *data, uint32_t size,
		    struct nouveau_object *obj)
{
	int (*func)(struct nouveau_object *);
	int ret = -ENOSYS;

	obj->parent = parent;
	obj->handle = handle;
	obj->oclass = oclass;
	obj->length = 0;
	obj->data = NULL;

	abi16_object(obj, &func);
	if (func) {
		obj->length = size ? size : sizeof(struct nouveau_object *);
		if (!(obj->data = malloc(obj->length)))
			return -ENOMEM;
		if (data)
			memcpy(obj->data, data, obj->length);
		*(struct nouveau_object **)obj->data = obj;

		ret = func(obj);
	}

	if (ret) {
		nouveau_object_fini(obj);
		return ret;
	}

	return 0;
}

int
nouveau_object_new(struct nouveau_object *parent, uint64_t handle,
		   uint32_t oclass, void *data, uint32_t length,
		   struct nouveau_object **pobj)
{
	struct nouveau_object *obj;
	int ret;

	if (!(obj = malloc(sizeof(*obj))))
		return -ENOMEM;

	ret = nouveau_object_init(parent, handle, oclass, data, length, obj);
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
	if (obj) {
		nouveau_object_fini(obj);
		free(obj);
		*pobj = NULL;
	}
}

void
nouveau_drm_del(struct nouveau_drm **pdrm)
{
	free(*pdrm);
	*pdrm = NULL;
}

int
nouveau_drm_new(int fd, struct nouveau_drm **pdrm)
{
	struct nouveau_drm *drm;
	drmVersionPtr ver;

#ifdef DEBUG
	debug_init(getenv("NOUVEAU_LIBDRM_DEBUG"));
#endif

	if (!(drm = calloc(1, sizeof(*drm))))
		return -ENOMEM;
	drm->fd = fd;

	if (!(ver = drmGetVersion(fd))) {
		nouveau_drm_del(&drm);
		return -EINVAL;
	}
	*pdrm = drm;

	drm->version = (ver->version_major << 24) |
		       (ver->version_minor << 8) |
		        ver->version_patchlevel;
	drm->nvif = false;
	drmFreeVersion(ver);
	return 0;
}

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
	struct nouveau_drm *drm;
	struct nouveau_device_priv *nvdev;
	struct nouveau_device *dev;
	uint64_t chipset, vram, gart, bousage;
	int ret;
	char *tmp;

	if (!(nvdev = calloc(1, sizeof(*nvdev))))
		return -ENOMEM;
	dev = &nvdev->base;

	ret = pthread_mutex_init(&nvdev->lock, NULL);
	if (ret) {
		free(nvdev);
		return ret;
	}

	ret = nouveau_drm_new(fd, &drm);
	if (ret) {
		nouveau_device_del(&dev);
		return ret;
	}
	drm->nvif = false;

	nvdev->base.object.parent = &drm->client;
	nvdev->base.object.oclass = NOUVEAU_DEVICE_CLASS;
	nvdev->base.object.length = ~0;
	nvdev->base.fd = drm->fd;
	nvdev->base.drm_version = drm->drm_version;
	nvdev->base.lib_version = drm->lib_version;

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
		struct nouveau_drm *drm = nouveau_drm(&nvdev->base.object);
		free(nvdev->client);
		nouveau_drm_del(&drm);
		pthread_mutex_destroy(&nvdev->lock);
		if (nvdev->close)
			drmClose(nvdev->base.fd);
		free(nvdev);
		*pdev = NULL;
	}
}

int
nouveau_getparam(struct nouveau_device *dev, uint64_t param, uint64_t *value)
{
	struct nouveau_drm *drm = nouveau_drm(&dev->object);
	struct drm_nouveau_getparam r = { .param = param };
	int fd = drm->fd, ret =
		drmCommandWriteRead(fd, DRM_NOUVEAU_GETPARAM, &r, sizeof(r));
	*value = r.value;
	return ret;
}

int
nouveau_setparam(struct nouveau_device *dev, uint64_t param, uint64_t value)
{
	struct nouveau_drm *drm = nouveau_drm(&dev->object);
	struct drm_nouveau_setparam r = { .param = param, .value = value };
	return drmCommandWrite(drm->fd, DRM_NOUVEAU_SETPARAM, &r, sizeof(r));
}

int
nouveau_client_new(struct nouveau_device *dev, struct nouveau_client **pclient)
{
	struct nouveau_device_priv *nvdev = nouveau_device(dev);
	struct nouveau_client_priv *pcli;
	int id = 0, i, ret = -ENOMEM;
	uint32_t *clients;

	pthread_mutex_lock(&nvdev->lock);

	for (i = 0; i < nvdev->nr_client; i++) {
		id = ffs(nvdev->client[i]) - 1;
		if (id >= 0)
			goto out;
	}

	clients = realloc(nvdev->client, sizeof(uint32_t) * (i + 1));
	if (!clients)
		goto unlock;
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

unlock:
	pthread_mutex_unlock(&nvdev->lock);
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
		pthread_mutex_lock(&nvdev->lock);
		nvdev->client[id / 32] &= ~(1 << (id % 32));
		pthread_mutex_unlock(&nvdev->lock);
		free(pcli->kref);
		free(pcli);
	}
}

static void
nouveau_bo_del(struct nouveau_bo *bo)
{
	struct nouveau_drm *drm = nouveau_drm(&bo->device->object);
	struct nouveau_device_priv *nvdev = nouveau_device(bo->device);
	struct nouveau_bo_priv *nvbo = nouveau_bo(bo);
	struct drm_gem_close req = { .handle = bo->handle };

	if (nvbo->head.next) {
		pthread_mutex_lock(&nvdev->lock);
		if (atomic_read(&nvbo->refcnt) == 0) {
			DRMLISTDEL(&nvbo->head);
			/*
			 * This bo has to be closed with the lock held because
			 * gem handles are not refcounted. If a shared bo is
			 * closed and re-opened in another thread a race
			 * against DRM_IOCTL_GEM_OPEN or drmPrimeFDToHandle
			 * might cause the bo to be closed accidentally while
			 * re-importing.
			 */
			drmIoctl(drm->fd, DRM_IOCTL_GEM_CLOSE, &req);
		}
		pthread_mutex_unlock(&nvdev->lock);
	} else {
		drmIoctl(drm->fd, DRM_IOCTL_GEM_CLOSE, &req);
	}
	if (bo->map)
		drm_munmap(bo->map, bo->size);
	free(nvbo);
}

int
nouveau_bo_new(struct nouveau_device *dev, uint32_t flags, uint32_t align,
	       uint64_t size, union nouveau_bo_config *config,
	       struct nouveau_bo **pbo)
{
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

	*pbo = bo;
	return 0;
}

static int
nouveau_bo_wrap_locked(struct nouveau_device *dev, uint32_t handle,
		       struct nouveau_bo **pbo, int name)
{
	struct nouveau_drm *drm = nouveau_drm(&dev->object);
	struct nouveau_device_priv *nvdev = nouveau_device(dev);
	struct drm_nouveau_gem_info req = { .handle = handle };
	struct nouveau_bo_priv *nvbo;
	int ret;

	DRMLISTFOREACHENTRY(nvbo, &nvdev->bo_list, head) {
		if (nvbo->base.handle == handle) {
			if (atomic_inc_return(&nvbo->refcnt) == 1) {
				/*
				 * Uh oh, this bo is dead and someone else
				 * will free it, but because refcnt is
				 * now non-zero fortunately they won't
				 * call the ioctl to close the bo.
				 *
				 * Remove this bo from the list so other
				 * calls to nouveau_bo_wrap_locked will
				 * see our replacement nvbo.
				 */
				DRMLISTDEL(&nvbo->head);
				if (!name)
					name = nvbo->name;
				break;
			}

			*pbo = &nvbo->base;
			return 0;
		}
	}

	ret = drmCommandWriteRead(drm->fd, DRM_NOUVEAU_GEM_INFO,
				  &req, sizeof(req));
	if (ret)
		return ret;

	nvbo = calloc(1, sizeof(*nvbo));
	if (nvbo) {
		atomic_set(&nvbo->refcnt, 1);
		nvbo->base.device = dev;
		abi16_bo_info(&nvbo->base, &req);
		nvbo->name = name;
		DRMLISTADD(&nvbo->head, &nvdev->bo_list);
		*pbo = &nvbo->base;
		return 0;
	}

	return -ENOMEM;
}

static void
nouveau_bo_make_global(struct nouveau_bo_priv *nvbo)
{
	if (!nvbo->head.next) {
		struct nouveau_device_priv *nvdev = nouveau_device(nvbo->base.device);
		pthread_mutex_lock(&nvdev->lock);
		if (!nvbo->head.next)
			DRMLISTADD(&nvbo->head, &nvdev->bo_list);
		pthread_mutex_unlock(&nvdev->lock);
	}
}

int
nouveau_bo_wrap(struct nouveau_device *dev, uint32_t handle,
		struct nouveau_bo **pbo)
{
	struct nouveau_device_priv *nvdev = nouveau_device(dev);
	int ret;
	pthread_mutex_lock(&nvdev->lock);
	ret = nouveau_bo_wrap_locked(dev, handle, pbo, 0);
	pthread_mutex_unlock(&nvdev->lock);
	return ret;
}

int
nouveau_bo_name_ref(struct nouveau_device *dev, uint32_t name,
		    struct nouveau_bo **pbo)
{
	struct nouveau_drm *drm = nouveau_drm(&dev->object);
	struct nouveau_device_priv *nvdev = nouveau_device(dev);
	struct nouveau_bo_priv *nvbo;
	struct drm_gem_open req = { .name = name };
	int ret;

	pthread_mutex_lock(&nvdev->lock);
	DRMLISTFOREACHENTRY(nvbo, &nvdev->bo_list, head) {
		if (nvbo->name == name) {
			ret = nouveau_bo_wrap_locked(dev, nvbo->base.handle,
						     pbo, name);
			pthread_mutex_unlock(&nvdev->lock);
			return ret;
		}
	}

	ret = drmIoctl(drm->fd, DRM_IOCTL_GEM_OPEN, &req);
	if (ret == 0) {
		ret = nouveau_bo_wrap_locked(dev, req.handle, pbo, name);
	}

	pthread_mutex_unlock(&nvdev->lock);
	return ret;
}

int
nouveau_bo_name_get(struct nouveau_bo *bo, uint32_t *name)
{
	struct drm_gem_flink req = { .handle = bo->handle };
	struct nouveau_drm *drm = nouveau_drm(&bo->device->object);
	struct nouveau_bo_priv *nvbo = nouveau_bo(bo);

	*name = nvbo->name;
	if (!*name) {
		int ret = drmIoctl(drm->fd, DRM_IOCTL_GEM_FLINK, &req);

		if (ret) {
			*name = 0;
			return ret;
		}
		nvbo->name = *name = req.name;

		nouveau_bo_make_global(nvbo);
	}
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
	struct nouveau_drm *drm = nouveau_drm(&dev->object);
	struct nouveau_device_priv *nvdev = nouveau_device(dev);
	int ret;
	unsigned int handle;

	nouveau_bo_ref(NULL, bo);

	pthread_mutex_lock(&nvdev->lock);
	ret = drmPrimeFDToHandle(drm->fd, prime_fd, &handle);
	if (ret == 0) {
		ret = nouveau_bo_wrap_locked(dev, handle, bo, 0);
	}
	pthread_mutex_unlock(&nvdev->lock);
	return ret;
}

int
nouveau_bo_set_prime(struct nouveau_bo *bo, int *prime_fd)
{
	struct nouveau_drm *drm = nouveau_drm(&bo->device->object);
	struct nouveau_bo_priv *nvbo = nouveau_bo(bo);
	int ret;

	ret = drmPrimeHandleToFD(drm->fd, nvbo->base.handle, DRM_CLOEXEC, prime_fd);
	if (ret)
		return ret;

	nouveau_bo_make_global(nvbo);
	return 0;
}

int
nouveau_bo_wait(struct nouveau_bo *bo, uint32_t access,
		struct nouveau_client *client)
{
	struct nouveau_drm *drm = nouveau_drm(&bo->device->object);
	struct nouveau_bo_priv *nvbo = nouveau_bo(bo);
	struct drm_nouveau_gem_cpu_prep req;
	struct nouveau_pushbuf *push;
	int ret = 0;

	if (!(access & NOUVEAU_BO_RDWR))
		return 0;

	push = cli_push_get(client, bo);
	if (push && push->channel)
		nouveau_pushbuf_kick(push, push->channel);

	if (!nvbo->head.next && !(nvbo->access & NOUVEAU_BO_WR) &&
				!(access & NOUVEAU_BO_WR))
		return 0;

	req.handle = bo->handle;
	req.flags = 0;
	if (access & NOUVEAU_BO_WR)
		req.flags |= NOUVEAU_GEM_CPU_PREP_WRITE;
	if (access & NOUVEAU_BO_NOBLOCK)
		req.flags |= NOUVEAU_GEM_CPU_PREP_NOWAIT;

	ret = drmCommandWrite(drm->fd, DRM_NOUVEAU_GEM_CPU_PREP,
			      &req, sizeof(req));
	if (ret == 0)
		nvbo->access = 0;
	return ret;
}

int
nouveau_bo_map(struct nouveau_bo *bo, uint32_t access,
	       struct nouveau_client *client)
{
	struct nouveau_drm *drm = nouveau_drm(&bo->device->object);
	struct nouveau_bo_priv *nvbo = nouveau_bo(bo);
	if (bo->map == NULL) {
		bo->map = drm_mmap(0, bo->size, PROT_READ | PROT_WRITE,
			       MAP_SHARED, drm->fd, nvbo->map_handle);
		if (bo->map == MAP_FAILED) {
			bo->map = NULL;
			return -errno;
		}
	}
	return nouveau_bo_wait(bo, access, client);
}
