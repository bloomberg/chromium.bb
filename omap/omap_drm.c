/* -*- mode: C; c-file-style: "k&r"; tab-width 4; indent-tabs-mode: t; -*- */

/*
 * Copyright (C) 2011 Texas Instruments, Inc
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
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * Authors:
 *    Rob Clark <rob@ti.com>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <linux/stddef.h>
#include <errno.h>
#include <sys/mman.h>
#include <fcntl.h>

#include <xf86drm.h>

#include "omap_drm.h"
#include "omap_drmif.h"

#define __round_mask(x, y) ((__typeof__(x))((y)-1))
#define round_up(x, y) ((((x)-1) | __round_mask(x, y))+1)
#define PAGE_SIZE 4096

struct omap_device {
	int fd;
};

/* a GEM buffer object allocated from the DRM device */
struct omap_bo {
	struct omap_device	*dev;
	void		*map;		/* userspace mmap'ing (if there is one) */
	uint32_t	size;
	uint32_t	handle;
	uint32_t	name;		/* flink global handle (DRI2 name) */
	uint64_t	offset;		/* offset to mmap() */
	int		fd;		/* dmabuf handle */
};

struct omap_device * omap_device_new(int fd)
{
	struct omap_device *dev = calloc(sizeof(*dev), 1);
	if (!dev)
		return NULL;
	dev->fd = fd;
	return dev;
}

void omap_device_del(struct omap_device *dev)
{
	free(dev);
}

int omap_get_param(struct omap_device *dev, uint64_t param, uint64_t *value)
{
	struct drm_omap_param req = {
			.param = param,
	};
	int ret;

	ret = drmCommandWriteRead(dev->fd, DRM_OMAP_GET_PARAM, &req, sizeof(req));
	if (ret) {
		return ret;
	}

	*value = req.value;

	return 0;
}

int omap_set_param(struct omap_device *dev, uint64_t param, uint64_t value)
{
	struct drm_omap_param req = {
			.param = param,
			.value = value,
	};
	return drmCommandWrite(dev->fd, DRM_OMAP_SET_PARAM, &req, sizeof(req));
}

/* allocate a new buffer object */
static struct omap_bo * omap_bo_new_impl(struct omap_device *dev,
		union omap_gem_size size, uint32_t flags)
{
	struct omap_bo *bo = NULL;
	struct drm_omap_gem_new req = {
			.size = size,
			.flags = flags,
	};

	if (size.bytes == 0) {
		goto fail;
	}

	bo = calloc(sizeof(*bo), 1);
	if (!bo) {
		goto fail;
	}

	bo->dev = dev;

	if (flags & OMAP_BO_TILED) {
		bo->size = round_up(size.tiled.width, PAGE_SIZE) * size.tiled.height;
	} else {
		bo->size = size.bytes;
	}

	if (drmCommandWriteRead(dev->fd, DRM_OMAP_GEM_NEW, &req, sizeof(req))) {
		goto fail;
	}

	bo->handle = req.handle;

	return bo;

fail:
	free(bo);
	return NULL;
}


/* allocate a new (un-tiled) buffer object */
struct omap_bo * omap_bo_new(struct omap_device *dev,
		uint32_t size, uint32_t flags)
{
	union omap_gem_size gsize = {
			.bytes = size,
	};
	if (flags & OMAP_BO_TILED) {
		return NULL;
	}
	return omap_bo_new_impl(dev, gsize, flags);
}

/* allocate a new buffer object */
struct omap_bo * omap_bo_new_tiled(struct omap_device *dev,
		uint32_t width, uint32_t height, uint32_t flags)
{
	union omap_gem_size gsize = {
			.tiled = {
				.width = width,
				.height = height,
			},
	};
	if (!(flags & OMAP_BO_TILED)) {
		return NULL;
	}
	return omap_bo_new_impl(dev, gsize, flags);
}

/* get buffer info */
static int get_buffer_info(struct omap_bo *bo)
{
	struct drm_omap_gem_info req = {
			.handle = bo->handle,
	};
	int ret = drmCommandWriteRead(bo->dev->fd, DRM_OMAP_GEM_INFO,
			&req, sizeof(req));
	if (ret) {
		return ret;
	}

	/* really all we need for now is mmap offset */
	bo->offset = req.offset;
	bo->size = req.size;

	return 0;
}

/* import a buffer object from DRI2 name */
struct omap_bo * omap_bo_from_name(struct omap_device *dev, uint32_t name)
{
	struct omap_bo *bo;
	struct drm_gem_open req = {
			.name = name,
	};

	bo = calloc(sizeof(*bo), 1);
	if (!bo) {
		goto fail;
	}

	if (drmIoctl(dev->fd, DRM_IOCTL_GEM_OPEN, &req)) {
		goto fail;
	}

	bo->dev = dev;
	bo->name = name;
	bo->handle = req.handle;

	return bo;

fail:
	free(bo);
	return NULL;
}

/* destroy a buffer object */
void omap_bo_del(struct omap_bo *bo)
{
	if (!bo) {
		return;
	}

	if (bo->map) {
		munmap(bo->map, bo->size);
	}

	if (bo->handle) {
		struct drm_gem_close req = {
				.handle = bo->handle,
		};

		drmIoctl(bo->dev->fd, DRM_IOCTL_GEM_CLOSE, &req);
	}

	free(bo);
}

/* get the global flink/DRI2 buffer name */
int omap_bo_get_name(struct omap_bo *bo, uint32_t *name)
{
	if (!bo->name) {
		struct drm_gem_flink req = {
				.handle = bo->handle,
		};
		int ret;

		ret = drmIoctl(bo->dev->fd, DRM_IOCTL_GEM_FLINK, &req);
		if (ret) {
			return ret;
		}

		bo->name = req.name;
	}

	*name = bo->name;

	return 0;
}

uint32_t omap_bo_handle(struct omap_bo *bo)
{
	return bo->handle;
}

int omap_bo_dmabuf(struct omap_bo *bo)
{
	if (!bo->fd) {
		struct drm_prime_handle req = {
				.handle = bo->handle,
				.flags = DRM_CLOEXEC,
		};
		int ret;

		ret = drmIoctl(bo->dev->fd, DRM_IOCTL_PRIME_HANDLE_TO_FD, &req);
		if (ret) {
			return ret;
		}

		bo->fd = req.fd;
	}
	return bo->fd;
}

uint32_t omap_bo_size(struct omap_bo *bo)
{
	if (!bo->size) {
		get_buffer_info(bo);
	}
	return bo->size;
}

void * omap_bo_map(struct omap_bo *bo)
{
	if (!bo->map) {
		if (!bo->offset) {
			get_buffer_info(bo);
		}

		bo->map = mmap(0, bo->size, PROT_READ | PROT_WRITE,
				MAP_SHARED, bo->dev->fd, bo->offset);
		if (bo->map == MAP_FAILED) {
			bo->map = NULL;
		}
	}
	return bo->map;
}

int omap_bo_cpu_prep(struct omap_bo *bo, enum omap_gem_op op)
{
	struct drm_omap_gem_cpu_prep req = {
			.handle = bo->handle,
			.op = op,
	};
	return drmCommandWrite(bo->dev->fd,
			DRM_OMAP_GEM_CPU_PREP, &req, sizeof(req));
}

int omap_bo_cpu_fini(struct omap_bo *bo, enum omap_gem_op op)
{
	struct drm_omap_gem_cpu_fini req = {
			.handle = bo->handle,
			.op = op,
			.nregions = 0,
	};
	return drmCommandWrite(bo->dev->fd,
			DRM_OMAP_GEM_CPU_FINI, &req, sizeof(req));
}
