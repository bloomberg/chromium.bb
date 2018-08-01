// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/drm/gpu/gbm_buffer.h"

#include <drm.h>
#include <drm_fourcc.h>
#include <fcntl.h>
#include <gbm.h>
#include <xf86drm.h>
#include <utility>

#include "base/files/platform_file.h"
#include "base/logging.h"
#include "base/posix/eintr_wrapper.h"
#include "base/strings/stringprintf.h"
#include "base/trace_event/trace_event.h"
#include "ui/gfx/buffer_format_util.h"
#include "ui/gfx/geometry/size_conversions.h"
#include "ui/gfx/gpu_fence.h"
#include "ui/gfx/native_pixmap_handle.h"
#include "ui/ozone/common/linux/drm_util_linux.h"
#include "ui/ozone/platform/drm/common/drm_util.h"
#include "ui/ozone/platform/drm/gpu/drm_window.h"
#include "ui/ozone/platform/drm/gpu/gbm_device.h"
#include "ui/ozone/platform/drm/gpu/gbm_surface_factory.h"
#include "ui/ozone/platform/drm/gpu/gbm_surfaceless.h"
#include "ui/ozone/public/ozone_platform.h"
#include "ui/ozone/public/surface_factory_ozone.h"

namespace ui {

GbmBuffer::GbmBuffer(const scoped_refptr<GbmDevice>& gbm,
                     struct gbm_bo* bo,
                     uint32_t format,
                     uint32_t flags,
                     uint64_t modifier,
                     std::vector<base::ScopedFD> fds,
                     const gfx::Size& size,
                     std::vector<gfx::NativePixmapPlane> planes)
    : drm_(gbm),
      gbm_bo_(bo,
              format,
              flags,
              modifier,
              std::move(fds),
              size,
              std::move(planes)) {}

GbmBuffer::~GbmBuffer() {}

std::unique_ptr<GbmBuffer> GbmBuffer::CreateBufferForBO(
    const scoped_refptr<GbmDevice>& gbm,
    struct gbm_bo* bo,
    uint32_t format,
    const gfx::Size& size,
    uint32_t flags) {
  DCHECK(bo);
  std::vector<base::ScopedFD> fds;
  std::vector<gfx::NativePixmapPlane> planes;

  const uint64_t modifier = gbm_bo_get_format_modifier(bo);
  for (size_t i = 0; i < gbm_bo_get_num_planes(bo); ++i) {
    // The fd returned by gbm_bo_get_fd is not ref-counted and need to be
    // kept open for the lifetime of the buffer.
    base::ScopedFD fd(gbm_bo_get_plane_fd(bo, i));

    // TODO(dcastagna): support multiple fds.
    // crbug.com/642410
    if (!i) {
      if (!fd.is_valid()) {
        PLOG(ERROR) << "Failed to export buffer to dma_buf";
        gbm_bo_destroy(bo);
        return nullptr;
      }
      fds.emplace_back(std::move(fd));
    }

    planes.emplace_back(gbm_bo_get_plane_stride(bo, i),
                        gbm_bo_get_plane_offset(bo, i),
                        gbm_bo_get_plane_size(bo, i), modifier);
  }
  return std::make_unique<GbmBuffer>(gbm, bo, format, flags, modifier,
                                     std::move(fds), size, std::move(planes));
}

// static
std::unique_ptr<GbmBuffer> GbmBuffer::CreateBufferWithModifiers(
    const scoped_refptr<GbmDevice>& gbm,
    uint32_t format,
    const gfx::Size& size,
    uint32_t flags,
    const std::vector<uint64_t>& modifiers) {
  TRACE_EVENT2("drm", "GbmBuffer::CreateBufferWithModifiers", "device",
               gbm->device_path().value(), "size", size.ToString());

  struct gbm_bo* bo =
      gbm_bo_create_with_modifiers(gbm->device(), size.width(), size.height(),
                                   format, modifiers.data(), modifiers.size());
  if (!bo)
    return nullptr;

  return CreateBufferForBO(gbm, bo, format, size, flags);
}

// static
std::unique_ptr<GbmBuffer> GbmBuffer::CreateBuffer(
    const scoped_refptr<GbmDevice>& gbm,
    uint32_t format,
    const gfx::Size& size,
    uint32_t flags) {
  TRACE_EVENT2("drm", "GbmBuffer::CreateBuffer", "device",
               gbm->device_path().value(), "size", size.ToString());

  struct gbm_bo* bo =
      gbm_bo_create(gbm->device(), size.width(), size.height(), format, flags);
  if (!bo)
    return nullptr;

  return CreateBufferForBO(gbm, bo, format, size, flags);
}

// static
std::unique_ptr<GbmBuffer> GbmBuffer::CreateBufferFromFds(
    const scoped_refptr<GbmDevice>& gbm,
    uint32_t format,
    const gfx::Size& size,
    std::vector<base::ScopedFD> fds,
    const std::vector<gfx::NativePixmapPlane>& planes) {
  TRACE_EVENT2("drm", "GbmBuffer::CreateBufferFromFD", "device",
               gbm->device_path().value(), "size", size.ToString());
  DCHECK_LE(fds.size(), planes.size());
  DCHECK_EQ(planes[0].offset, 0);

  // Try to use scanout if supported.
  int gbm_flags = GBM_BO_USE_SCANOUT | GBM_BO_USE_TEXTURING;
  if (!gbm_device_is_format_supported(gbm->device(), format, gbm_flags))
    gbm_flags &= ~GBM_BO_USE_SCANOUT;

  struct gbm_bo* bo = nullptr;
  if (gbm_device_is_format_supported(gbm->device(), format, gbm_flags)) {
    struct gbm_import_fd_planar_data fd_data;
    fd_data.width = size.width();
    fd_data.height = size.height();
    fd_data.format = format;

    DCHECK_LE(planes.size(), 3u);
    for (size_t i = 0; i < planes.size(); ++i) {
      fd_data.fds[i] = fds[i < fds.size() ? i : 0].get();
      fd_data.strides[i] = planes[i].stride;
      fd_data.offsets[i] = planes[i].offset;
      fd_data.format_modifiers[i] = planes[i].modifier;
    }

    // The fd passed to gbm_bo_import is not ref-counted and need to be
    // kept open for the lifetime of the buffer.
    bo = gbm_bo_import(gbm->device(), GBM_BO_IMPORT_FD_PLANAR, &fd_data,
                       gbm_flags);
    if (!bo) {
      LOG(ERROR) << "nullptr returned from gbm_bo_import";
      return nullptr;
    }
  }

  return std::make_unique<GbmBuffer>(gbm, bo, format, gbm_flags,
                                     planes[0].modifier, std::move(fds), size,
                                     std::move(planes));
}

GbmPixmap::GbmPixmap(GbmSurfaceFactory* surface_manager,
                     std::unique_ptr<GbmBuffer> buffer,
                     scoped_refptr<DrmFramebuffer> framebuffer)
    : surface_manager_(surface_manager),
      buffer_(std::move(buffer)),
      framebuffer_(std::move(framebuffer)) {}

gfx::NativePixmapHandle GbmPixmap::ExportHandle() {
  gfx::NativePixmapHandle handle;
  gfx::BufferFormat format =
      ui::GetBufferFormatFromFourCCFormat(buffer_->gbm_bo()->format());
  // TODO(dcastagna): Use gbm_bo_get_num_planes once all the formats we use are
  // supported by gbm.
  for (size_t i = 0; i < gfx::NumberOfPlanesForBufferFormat(format); ++i) {
    // Some formats (e.g: YVU_420) might have less than one fd per plane.
    if (i < buffer_->gbm_bo()->fd_count()) {
      base::ScopedFD scoped_fd(HANDLE_EINTR(dup(buffer_->gbm_bo()->GetFd(i))));
      if (!scoped_fd.is_valid()) {
        PLOG(ERROR) << "dup";
        return gfx::NativePixmapHandle();
      }
      handle.fds.emplace_back(
          base::FileDescriptor(scoped_fd.release(), true /* auto_close */));
    }
    handle.planes.emplace_back(buffer_->gbm_bo()->GetStride(i),
                               buffer_->gbm_bo()->GetOffset(i),
                               buffer_->gbm_bo()->GetPlaneSize(i),
                               buffer_->gbm_bo()->format_modifier());
  }
  return handle;
}

bool GbmPixmap::AreDmaBufFdsValid() const {
  return buffer_->gbm_bo()->AreFdsValid();
}

size_t GbmPixmap::GetDmaBufFdCount() const {
  return buffer_->gbm_bo()->fd_count();
}

int GbmPixmap::GetDmaBufFd(size_t plane) const {
  return buffer_->gbm_bo()->GetFd(plane);
}

int GbmPixmap::GetDmaBufPitch(size_t plane) const {
  return buffer_->gbm_bo()->GetStride(plane);
}

int GbmPixmap::GetDmaBufOffset(size_t plane) const {
  return buffer_->gbm_bo()->GetOffset(plane);
}

uint64_t GbmPixmap::GetDmaBufModifier(size_t plane) const {
  return buffer_->gbm_bo()->format_modifier();
}

gfx::BufferFormat GbmPixmap::GetBufferFormat() const {
  return ui::GetBufferFormatFromFourCCFormat(buffer_->gbm_bo()->format());
}

gfx::Size GbmPixmap::GetBufferSize() const {
  return buffer_->gbm_bo()->size();
}

uint32_t GbmPixmap::GetUniqueId() const {
  return buffer_->gbm_bo()->GetBoHandle();
}

bool GbmPixmap::ScheduleOverlayPlane(gfx::AcceleratedWidget widget,
                                     int plane_z_order,
                                     gfx::OverlayTransform plane_transform,
                                     const gfx::Rect& display_bounds,
                                     const gfx::RectF& crop_rect,
                                     bool enable_blend,
                                     std::unique_ptr<gfx::GpuFence> gpu_fence) {
  DCHECK(buffer_->gbm_bo()->flags() & GBM_BO_USE_SCANOUT);
  // |framebuffer_id| might be 0 if AddFramebuffer2 failed, in that case we
  // already logged the error in GbmBuffer ctor. We avoid logging the error
  // here since this method might be called every pageflip.
  if (framebuffer_) {
    surface_manager_->GetSurface(widget)->QueueOverlayPlane(DrmOverlayPlane(
        framebuffer_, plane_z_order, plane_transform, display_bounds, crop_rect,
        enable_blend, std::move(gpu_fence)));
  }

  return true;
}

GbmPixmap::~GbmPixmap() {}

}  // namespace ui
