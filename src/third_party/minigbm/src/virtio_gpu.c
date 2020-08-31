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
#include <xf86drm.h>

#include "drv_priv.h"
#include "helpers.h"
#include "util.h"
#include "virgl_hw.h"
#include "virtgpu_drm.h"

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
							DRM_FORMAT_NV12,
							DRM_FORMAT_YVU420_ANDROID };

static const uint32_t texture_source_formats[] = { DRM_FORMAT_NV12, DRM_FORMAT_R8, DRM_FORMAT_RG88,
						   DRM_FORMAT_YVU420_ANDROID };

struct virtio_gpu_priv {
	int has_3d;
	int caps_is_v2;
	union virgl_caps caps;
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

static bool virtio_gpu_supports_format(struct virgl_supported_format_mask *supported,
				       uint32_t drm_format)
{
	uint32_t virgl_format = translate_format(drm_format);
	if (!virgl_format) {
		return false;
	}

	uint32_t bitmask_index = virgl_format / 32;
	uint32_t bit_index = virgl_format % 32;
	return supported->bitmask[bitmask_index] & (1 << bit_index);
}

// Adds the given buffer combination to the list of supported buffer combinations if the
// combination is supported by the virtio backend.
static void virtio_gpu_add_combination(struct driver *drv, uint32_t drm_format,
				       struct format_metadata *metadata, uint64_t use_flags)
{
	struct virtio_gpu_priv *priv = (struct virtio_gpu_priv *)drv->priv;

	if (priv->has_3d && priv->caps.max_version >= 1) {
		if ((use_flags & BO_USE_RENDERING) &&
		    !virtio_gpu_supports_format(&priv->caps.v1.render, drm_format)) {
			drv_log("Skipping unsupported render format: %d\n", drm_format);
			return;
		}

		if ((use_flags & BO_USE_TEXTURE) &&
		    !virtio_gpu_supports_format(&priv->caps.v1.sampler, drm_format)) {
			drv_log("Skipping unsupported texture format: %d\n", drm_format);
			return;
		}
		if ((use_flags & BO_USE_SCANOUT) && priv->caps_is_v2 &&
		    !virtio_gpu_supports_format(&priv->caps.v2.scanout, drm_format)) {
			drv_log("Unsupported scanout format: %d\n", drm_format);
			use_flags &= ~BO_USE_SCANOUT;
		}
	}

	drv_add_combination(drv, drm_format, metadata, use_flags);
}

// Adds each given buffer combination to the list of supported buffer combinations if the
// combination supported by the virtio backend.
static void virtio_gpu_add_combinations(struct driver *drv, const uint32_t *drm_formats,
					uint32_t num_formats, struct format_metadata *metadata,
					uint64_t use_flags)
{
	uint32_t i;

	for (i = 0; i < num_formats; i++) {
		virtio_gpu_add_combination(drv, drm_formats[i], metadata, use_flags);
	}
}

static int virtio_dumb_bo_create(struct bo *bo, uint32_t width, uint32_t height, uint32_t format,
				 uint64_t use_flags)
{
	if (bo->meta.format != DRM_FORMAT_R8) {
		width = ALIGN(width, MESA_LLVMPIPE_TILE_SIZE);
		height = ALIGN(height, MESA_LLVMPIPE_TILE_SIZE);
	}

	return drv_dumb_bo_create_ex(bo, width, height, format, use_flags, BO_QUIRK_DUMB32BPP);
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
	handle_flag(&use_flags, BO_USE_CURSOR, &bind, VIRGL_BIND_CURSOR);
	handle_flag(&use_flags, BO_USE_LINEAR, &bind, VIRGL_BIND_LINEAR);

	handle_flag(&use_flags, BO_USE_SW_READ_OFTEN, &bind, VIRGL_BIND_LINEAR);
	handle_flag(&use_flags, BO_USE_SW_READ_RARELY, &bind, VIRGL_BIND_LINEAR);
	handle_flag(&use_flags, BO_USE_SW_WRITE_OFTEN, &bind, VIRGL_BIND_LINEAR);
	handle_flag(&use_flags, BO_USE_SW_WRITE_RARELY, &bind, VIRGL_BIND_LINEAR);

	// All host drivers only support linear camera buffer formats. If
	// that changes, this will need to be modified.
	handle_flag(&use_flags, BO_USE_CAMERA_READ, &bind, VIRGL_BIND_LINEAR);
	handle_flag(&use_flags, BO_USE_CAMERA_WRITE, &bind, VIRGL_BIND_LINEAR);

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

	res_create.size = ALIGN(bo->meta.total_size, PAGE_SIZE); // PAGE_SIZE = 0x1000
	ret = drmIoctl(bo->drv->fd, DRM_IOCTL_VIRTGPU_RESOURCE_CREATE, &res_create);
	if (ret) {
		drv_log("DRM_IOCTL_VIRTGPU_RESOURCE_CREATE failed with %s\n", strerror(errno));
		return ret;
	}

	for (uint32_t plane = 0; plane < bo->meta.num_planes; plane++)
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

	vma->length = bo->meta.total_size;
	return mmap(0, bo->meta.total_size, drv_get_prot(map_flags), MAP_SHARED, bo->drv->fd,
		    gem_map.offset);
}

static int virtio_gpu_get_caps(struct driver *drv, union virgl_caps *caps, int *caps_is_v2)
{
	int ret;
	struct drm_virtgpu_get_caps cap_args;
	struct drm_virtgpu_getparam param_args;
	uint32_t can_query_v2 = 0;

	memset(&param_args, 0, sizeof(param_args));
	param_args.param = VIRTGPU_PARAM_CAPSET_QUERY_FIX;
	param_args.value = (uint64_t)(uintptr_t)&can_query_v2;
	ret = drmIoctl(drv->fd, DRM_IOCTL_VIRTGPU_GETPARAM, &param_args);
	if (ret) {
		drv_log("DRM_IOCTL_VIRTGPU_GETPARAM failed with %s\n", strerror(errno));
	}

	*caps_is_v2 = 0;
	memset(&cap_args, 0, sizeof(cap_args));
	cap_args.addr = (unsigned long long)caps;
	if (can_query_v2) {
		*caps_is_v2 = 1;
		cap_args.cap_set_id = 2;
		cap_args.size = sizeof(union virgl_caps);
	} else {
		cap_args.cap_set_id = 1;
		cap_args.size = sizeof(struct virgl_caps_v1);
	}

	ret = drmIoctl(drv->fd, DRM_IOCTL_VIRTGPU_GET_CAPS, &cap_args);
	if (ret) {
		drv_log("DRM_IOCTL_VIRTGPU_GET_CAPS failed with %s\n", strerror(errno));
		*caps_is_v2 = 0;

		// Fallback to v1
		cap_args.cap_set_id = 1;
		cap_args.size = sizeof(struct virgl_caps_v1);

		ret = drmIoctl(drv->fd, DRM_IOCTL_VIRTGPU_GET_CAPS, &cap_args);
		if (ret) {
			drv_log("DRM_IOCTL_VIRTGPU_GET_CAPS failed with %s\n", strerror(errno));
		}
	}

	return ret;
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

	if (priv->has_3d) {
		virtio_gpu_get_caps(drv, &priv->caps, &priv->caps_is_v2);

		/* This doesn't mean host can scanout everything, it just means host
		 * hypervisor can show it. */
		virtio_gpu_add_combinations(drv, render_target_formats,
					    ARRAY_SIZE(render_target_formats), &LINEAR_METADATA,
					    BO_USE_RENDER_MASK | BO_USE_SCANOUT);
		virtio_gpu_add_combinations(drv, texture_source_formats,
					    ARRAY_SIZE(texture_source_formats), &LINEAR_METADATA,
					    BO_USE_TEXTURE_MASK);
	} else {
		/* Virtio primary plane only allows this format. */
		virtio_gpu_add_combination(drv, DRM_FORMAT_XRGB8888, &LINEAR_METADATA,
					   BO_USE_RENDER_MASK | BO_USE_SCANOUT);
		/* Virtio cursor plane only allows this format and Chrome cannot live without
		 * ARGB888 renderable format. */
		virtio_gpu_add_combination(drv, DRM_FORMAT_ARGB8888, &LINEAR_METADATA,
					   BO_USE_RENDER_MASK | BO_USE_CURSOR);
		/* Android needs more, but they cannot be bound as scanouts anymore after
		 * "drm/virtio: fix DRM_FORMAT_* handling" */
		virtio_gpu_add_combinations(drv, render_target_formats,
					    ARRAY_SIZE(render_target_formats), &LINEAR_METADATA,
					    BO_USE_RENDER_MASK);
		virtio_gpu_add_combinations(drv, dumb_texture_source_formats,
					    ARRAY_SIZE(dumb_texture_source_formats),
					    &LINEAR_METADATA, BO_USE_TEXTURE_MASK);
		virtio_gpu_add_combination(drv, DRM_FORMAT_NV12, &LINEAR_METADATA,
					   BO_USE_SW_MASK | BO_USE_LINEAR);
	}

	/* Android CTS tests require this. */
	virtio_gpu_add_combination(drv, DRM_FORMAT_BGR888, &LINEAR_METADATA, BO_USE_SW_MASK);

	drv_modify_combination(drv, DRM_FORMAT_NV12, &LINEAR_METADATA,
			       BO_USE_CAMERA_READ | BO_USE_CAMERA_WRITE | BO_USE_HW_VIDEO_DECODER |
				   BO_USE_HW_VIDEO_ENCODER);
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
	struct drm_virtgpu_3d_wait waitcmd;

	if (!priv->has_3d)
		return 0;

	// Invalidate is only necessary if the host writes to the buffer.
	if ((bo->meta.use_flags & (BO_USE_RENDERING | BO_USE_CAMERA_WRITE |
				   BO_USE_HW_VIDEO_ENCODER | BO_USE_HW_VIDEO_DECODER)) == 0)
		return 0;

	memset(&xfer, 0, sizeof(xfer));
	xfer.bo_handle = mapping->vma->handle;
	xfer.box.x = mapping->rect.x;
	xfer.box.y = mapping->rect.y;
	xfer.box.w = mapping->rect.width;
	xfer.box.h = mapping->rect.height;
	xfer.box.d = 1;

	if ((bo->meta.use_flags & BO_USE_RENDERING) == 0) {
		// Unfortunately, the kernel doesn't actually pass the guest layer_stride and
		// guest stride to the host (compare virtio_gpu.h and virtgpu_drm.h). For gbm
		// based resources, we can work around this by using the level field to pass
		// the stride to virglrenderer's gbm transfer code. However, we need to avoid
		// doing this for resources which don't rely on that transfer code, which is
		// resources with the BO_USE_RENDERING flag set.
		// TODO(b/145993887): Send also stride when the patches are landed
		xfer.level = bo->meta.strides[0];
	}

	ret = drmIoctl(bo->drv->fd, DRM_IOCTL_VIRTGPU_TRANSFER_FROM_HOST, &xfer);
	if (ret) {
		drv_log("DRM_IOCTL_VIRTGPU_TRANSFER_FROM_HOST failed with %s\n", strerror(errno));
		return -errno;
	}

	// The transfer needs to complete before invalidate returns so that any host changes
	// are visible and to ensure the host doesn't overwrite subsequent guest changes.
	// TODO(b/136733358): Support returning fences from transfers
	memset(&waitcmd, 0, sizeof(waitcmd));
	waitcmd.handle = mapping->vma->handle;
	ret = drmIoctl(bo->drv->fd, DRM_IOCTL_VIRTGPU_WAIT, &waitcmd);
	if (ret) {
		drv_log("DRM_IOCTL_VIRTGPU_WAIT failed with %s\n", strerror(errno));
		return -errno;
	}

	return 0;
}

static int virtio_gpu_bo_flush(struct bo *bo, struct mapping *mapping)
{
	int ret;
	struct drm_virtgpu_3d_transfer_to_host xfer;
	struct virtio_gpu_priv *priv = (struct virtio_gpu_priv *)bo->drv->priv;
	struct drm_virtgpu_3d_wait waitcmd;

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

	// Unfortunately, the kernel doesn't actually pass the guest layer_stride and
	// guest stride to the host (compare virtio_gpu.h and virtgpu_drm.h). We can use
	// the level to work around this.
	xfer.level = bo->meta.strides[0];

	ret = drmIoctl(bo->drv->fd, DRM_IOCTL_VIRTGPU_TRANSFER_TO_HOST, &xfer);
	if (ret) {
		drv_log("DRM_IOCTL_VIRTGPU_TRANSFER_TO_HOST failed with %s\n", strerror(errno));
		return -errno;
	}

	// If the buffer is only accessed by the host GPU, then the flush is ordered
	// with subsequent commands. However, if other host hardware can access the
	// buffer, we need to wait for the transfer to complete for consistency.
	// TODO(b/136733358): Support returning fences from transfers
	if (bo->meta.use_flags & BO_USE_NON_GPU_HW) {
		memset(&waitcmd, 0, sizeof(waitcmd));
		waitcmd.handle = mapping->vma->handle;

		ret = drmIoctl(bo->drv->fd, DRM_IOCTL_VIRTGPU_WAIT, &waitcmd);
		if (ret) {
			drv_log("DRM_IOCTL_VIRTGPU_WAIT failed with %s\n", strerror(errno));
			return -errno;
		}
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

static int virtio_gpu_resource_info(struct bo *bo, uint32_t strides[DRV_MAX_PLANES],
				    uint32_t offsets[DRV_MAX_PLANES])
{
	int ret;
	struct drm_virtgpu_resource_info res_info;
	struct virtio_gpu_priv *priv = (struct virtio_gpu_priv *)bo->drv->priv;

	if (!priv->has_3d)
		return 0;

	memset(&res_info, 0, sizeof(res_info));
	res_info.bo_handle = bo->handles[0].u32;
	ret = drmIoctl(bo->drv->fd, DRM_IOCTL_VIRTGPU_RESOURCE_INFO, &res_info);
	if (ret) {
		drv_log("DRM_IOCTL_VIRTGPU_RESOURCE_INFO failed with %s\n", strerror(errno));
		return ret;
	}

	for (uint32_t plane = 0; plane < bo->meta.num_planes; plane++) {
		/*
		 * Currently, kernel v4.14 (Betty) doesn't have the extended resource info
		 * ioctl.
		 */
		if (res_info.strides[plane]) {
			strides[plane] = res_info.strides[plane];
			offsets[plane] = res_info.offsets[plane];
		}
	}

	return 0;
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
	.resource_info = virtio_gpu_resource_info,
};
