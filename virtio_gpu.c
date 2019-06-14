/*
 * Copyright 2017 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <virtgpu_drm.h>
#include <xf86drm.h>

#include "drv_priv.h"
#include "helpers.h"
#include "util.h"
#include "virgl_hw.h"

#ifndef PAGE_SIZE
#define PAGE_SIZE 0x1000
#endif
#define PIPE_TEXTURE_2D 2

#define MESA_LLVMPIPE_TILE_ORDER 6
#define MESA_LLVMPIPE_TILE_SIZE (1 << MESA_LLVMPIPE_TILE_ORDER)

static const uint32_t render_target_formats[] = { DRM_FORMAT_ABGR8888, DRM_FORMAT_ARGB8888,
						  DRM_FORMAT_RGB565, DRM_FORMAT_XBGR8888,
						  DRM_FORMAT_XRGB8888 };

static const uint32_t dumb_texture_source_formats[] = { DRM_FORMAT_R8, DRM_FORMAT_YVU420,
							DRM_FORMAT_YVU420_ANDROID };

static const uint32_t texture_source_formats[] = { DRM_FORMAT_NV12, DRM_FORMAT_R8, DRM_FORMAT_RG88,
						   DRM_FORMAT_YVU420_ANDROID };

struct virtio_gpu_priv {
	int has_3d;
};

static uint32_t translate_format(uint32_t drm_fourcc)
{
	switch (drm_fourcc) {
	case DRM_FORMAT_XRGB8888:
		return VIRGL_FORMAT_B8G8R8X8_UNORM;
	case DRM_FORMAT_ARGB8888:
		return VIRGL_FORMAT_B8G8R8A8_UNORM;
	case DRM_FORMAT_XBGR8888:
		return VIRGL_FORMAT_R8G8B8X8_UNORM;
	case DRM_FORMAT_ABGR8888:
		return VIRGL_FORMAT_R8G8B8A8_UNORM;
	case DRM_FORMAT_RGB565:
		return VIRGL_FORMAT_B5G6R5_UNORM;
	case DRM_FORMAT_R8:
		return VIRGL_FORMAT_R8_UNORM;
	case DRM_FORMAT_RG88:
		return VIRGL_FORMAT_R8G8_UNORM;
	case DRM_FORMAT_NV12:
		return VIRGL_FORMAT_NV12;
	case DRM_FORMAT_YVU420:
	case DRM_FORMAT_YVU420_ANDROID:
		return VIRGL_FORMAT_YV12;
	default:
		return 0;
	}
}

static int virtio_dumb_bo_create(struct bo *bo, uint32_t width, uint32_t height, uint32_t format,
				 uint64_t use_flags)
{
	if (bo->format != DRM_FORMAT_R8) {
		width = ALIGN(width, MESA_LLVMPIPE_TILE_SIZE);
		height = ALIGN(height, MESA_LLVMPIPE_TILE_SIZE);
	}

	return drv_dumb_bo_create(bo, width, height, format, use_flags);
}

static inline void handle_flag(uint64_t *flag, uint64_t check_flag, uint32_t *bind,
			       uint32_t virgl_bind)
{
	if ((*flag) & check_flag) {
		(*flag) &= ~check_flag;
		(*bind) |= virgl_bind;
	}
}

static uint32_t use_flags_to_bind(uint64_t use_flags)
{
	/* In crosvm, VIRGL_BIND_SHARED means minigbm will allocate, not virglrenderer. */
	uint32_t bind = VIRGL_BIND_SHARED;

	handle_flag(&use_flags, BO_USE_TEXTURE, &bind, VIRGL_BIND_SAMPLER_VIEW);
	handle_flag(&use_flags, BO_USE_RENDERING, &bind, VIRGL_BIND_RENDER_TARGET);
	handle_flag(&use_flags, BO_USE_SCANOUT, &bind, VIRGL_BIND_SCANOUT);
	// TODO (b/12983436): handle other use flags.
	if (use_flags) {
		drv_log("Unhandled bo use flag: %llx\n", (unsigned long long)use_flags);
	}

	return bind;
}

static int virtio_virgl_bo_create(struct bo *bo, uint32_t width, uint32_t height, uint32_t format,
				  uint64_t use_flags)
{
	int ret;
	uint32_t stride;
	struct drm_virtgpu_resource_create res_create;

	stride = drv_stride_from_format(format, width, 0);
	drv_bo_from_format(bo, stride, height, format);

	/*
	 * Setting the target is intended to ensure this resource gets bound as a 2D
	 * texture in the host renderer's GL state. All of these resource properties are
	 * sent unchanged by the kernel to the host, which in turn sends them unchanged to
	 * virglrenderer. When virglrenderer makes a resource, it will convert the target
	 * enum to the equivalent one in GL and then bind the resource to that target.
	 */
	memset(&res_create, 0, sizeof(res_create));

	res_create.target = PIPE_TEXTURE_2D;
	res_create.format = translate_format(format);
	res_create.bind = use_flags_to_bind(use_flags);
	res_create.width = width;
	res_create.height = height;

	/* For virgl 3D */
	res_create.depth = 1;
	res_create.array_size = 1;
	res_create.last_level = 0;
	res_create.nr_samples = 0;

	res_create.size = ALIGN(bo->total_size, PAGE_SIZE); // PAGE_SIZE = 0x1000
	ret = drmIoctl(bo->drv->fd, DRM_IOCTL_VIRTGPU_RESOURCE_CREATE, &res_create);
	if (ret) {
		drv_log("DRM_IOCTL_VIRTGPU_RESOURCE_CREATE failed with %s\n", strerror(errno));
		return ret;
	}

	for (uint32_t plane = 0; plane < bo->num_planes; plane++)
		bo->handles[plane].u32 = res_create.bo_handle;

	return 0;
}

static void *virtio_virgl_bo_map(struct bo *bo, struct vma *vma, size_t plane, uint32_t map_flags)
{
	int ret;
	struct drm_virtgpu_map gem_map;

	memset(&gem_map, 0, sizeof(gem_map));
	gem_map.handle = bo->handles[0].u32;

	ret = drmIoctl(bo->drv->fd, DRM_IOCTL_VIRTGPU_MAP, &gem_map);
	if (ret) {
		drv_log("DRM_IOCTL_VIRTGPU_MAP failed with %s\n", strerror(errno));
		return MAP_FAILED;
	}

	vma->length = bo->total_size;
	return mmap(0, bo->total_size, drv_get_prot(map_flags), MAP_SHARED, bo->drv->fd,
		    gem_map.offset);
}

static int virtio_gpu_init(struct driver *drv)
{
	int ret;
	struct virtio_gpu_priv *priv;
	struct drm_virtgpu_getparam args;

	priv = calloc(1, sizeof(*priv));
	drv->priv = priv;

	memset(&args, 0, sizeof(args));
	args.param = VIRTGPU_PARAM_3D_FEATURES;
	args.value = (uint64_t)(uintptr_t)&priv->has_3d;
	ret = drmIoctl(drv->fd, DRM_IOCTL_VIRTGPU_GETPARAM, &args);
	if (ret) {
		drv_log("virtio 3D acceleration is not available\n");
		/* Be paranoid */
		priv->has_3d = 0;
	}

	/* This doesn't mean host can scanout everything, it just means host
	 * hypervisor can show it. */
	drv_add_combinations(drv, render_target_formats, ARRAY_SIZE(render_target_formats),
			     &LINEAR_METADATA, BO_USE_RENDER_MASK | BO_USE_SCANOUT);

	if (priv->has_3d) {
		drv_add_combinations(drv, texture_source_formats,
				     ARRAY_SIZE(texture_source_formats), &LINEAR_METADATA,
				     BO_USE_TEXTURE_MASK);
	} else {
		drv_add_combinations(drv, dumb_texture_source_formats,
				     ARRAY_SIZE(dumb_texture_source_formats), &LINEAR_METADATA,
				     BO_USE_TEXTURE_MASK);
		drv_add_combination(drv, DRM_FORMAT_NV12, &LINEAR_METADATA,
				    BO_USE_SW_MASK | BO_USE_LINEAR);
	}

	/* Android CTS tests require this. */
	drv_add_combination(drv, DRM_FORMAT_BGR888, &LINEAR_METADATA, BO_USE_SW_MASK);

	drv_modify_combination(drv, DRM_FORMAT_NV12, &LINEAR_METADATA,
			       BO_USE_CAMERA_READ | BO_USE_CAMERA_WRITE);
	drv_modify_combination(drv, DRM_FORMAT_R8, &LINEAR_METADATA,
			       BO_USE_CAMERA_READ | BO_USE_CAMERA_WRITE);

	return drv_modify_linear_combinations(drv);
}

static void virtio_gpu_close(struct driver *drv)
{
	free(drv->priv);
	drv->priv = NULL;
}

static int virtio_gpu_bo_create(struct bo *bo, uint32_t width, uint32_t height, uint32_t format,
				uint64_t use_flags)
{
	struct virtio_gpu_priv *priv = (struct virtio_gpu_priv *)bo->drv->priv;
	if (priv->has_3d)
		return virtio_virgl_bo_create(bo, width, height, format, use_flags);
	else
		return virtio_dumb_bo_create(bo, width, height, format, use_flags);
}

static int virtio_gpu_bo_destroy(struct bo *bo)
{
	struct virtio_gpu_priv *priv = (struct virtio_gpu_priv *)bo->drv->priv;
	if (priv->has_3d)
		return drv_gem_bo_destroy(bo);
	else
		return drv_dumb_bo_destroy(bo);
}

static void *virtio_gpu_bo_map(struct bo *bo, struct vma *vma, size_t plane, uint32_t map_flags)
{
	struct virtio_gpu_priv *priv = (struct virtio_gpu_priv *)bo->drv->priv;
	if (priv->has_3d)
		return virtio_virgl_bo_map(bo, vma, plane, map_flags);
	else
		return drv_dumb_bo_map(bo, vma, plane, map_flags);
}

static int virtio_gpu_bo_invalidate(struct bo *bo, struct mapping *mapping)
{
	int ret;
	struct drm_virtgpu_3d_transfer_from_host xfer;
	struct virtio_gpu_priv *priv = (struct virtio_gpu_priv *)bo->drv->priv;

	if (!priv->has_3d)
		return 0;

	memset(&xfer, 0, sizeof(xfer));
	xfer.bo_handle = mapping->vma->handle;
	xfer.box.x = mapping->rect.x;
	xfer.box.y = mapping->rect.y;
	xfer.box.w = mapping->rect.width;
	xfer.box.h = mapping->rect.height;
	xfer.box.d = 1;

	ret = drmIoctl(bo->drv->fd, DRM_IOCTL_VIRTGPU_TRANSFER_FROM_HOST, &xfer);
	if (ret) {
		drv_log("DRM_IOCTL_VIRTGPU_TRANSFER_FROM_HOST failed with %s\n", strerror(errno));
		return -errno;
	}

	return 0;
}

static int virtio_gpu_bo_flush(struct bo *bo, struct mapping *mapping)
{
	int ret;
	struct drm_virtgpu_3d_transfer_to_host xfer;
	struct virtio_gpu_priv *priv = (struct virtio_gpu_priv *)bo->drv->priv;

	if (!priv->has_3d)
		return 0;

	if (!(mapping->vma->map_flags & BO_MAP_WRITE))
		return 0;

	memset(&xfer, 0, sizeof(xfer));
	xfer.bo_handle = mapping->vma->handle;
	xfer.box.x = mapping->rect.x;
	xfer.box.y = mapping->rect.y;
	xfer.box.w = mapping->rect.width;
	xfer.box.h = mapping->rect.height;
	xfer.box.d = 1;

	ret = drmIoctl(bo->drv->fd, DRM_IOCTL_VIRTGPU_TRANSFER_TO_HOST, &xfer);
	if (ret) {
		drv_log("DRM_IOCTL_VIRTGPU_TRANSFER_TO_HOST failed with %s\n", strerror(errno));
		return -errno;
	}

	return 0;
}

static uint32_t virtio_gpu_resolve_format(struct driver *drv, uint32_t format, uint64_t use_flags)
{
	struct virtio_gpu_priv *priv = (struct virtio_gpu_priv *)drv->priv;

	switch (format) {
	case DRM_FORMAT_FLEX_IMPLEMENTATION_DEFINED:
		/* Camera subsystem requires NV12. */
		if (use_flags & (BO_USE_CAMERA_READ | BO_USE_CAMERA_WRITE))
			return DRM_FORMAT_NV12;
		/*HACK: See b/28671744 */
		return DRM_FORMAT_XBGR8888;
	case DRM_FORMAT_FLEX_YCbCr_420_888:
		/*
		 * All of our host drivers prefer NV12 as their flexible media format.
		 * If that changes, this will need to be modified.
		 */
		if (priv->has_3d)
			return DRM_FORMAT_NV12;
		else
			return DRM_FORMAT_YVU420;
	default:
		return format;
	}
}

const struct backend backend_virtio_gpu = {
	.name = "virtio_gpu",
	.init = virtio_gpu_init,
	.close = virtio_gpu_close,
	.bo_create = virtio_gpu_bo_create,
	.bo_destroy = virtio_gpu_bo_destroy,
	.bo_import = drv_prime_bo_import,
	.bo_map = virtio_gpu_bo_map,
	.bo_unmap = drv_bo_munmap,
	.bo_invalidate = virtio_gpu_bo_invalidate,
	.bo_flush = virtio_gpu_bo_flush,
	.resolve_format = virtio_gpu_resolve_format,
};
