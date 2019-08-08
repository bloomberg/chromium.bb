// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/gpu/gbm_pixmap_wayland.h"

#include <drm_fourcc.h>
#include <gbm.h>
#include <xf86drmMode.h>
#include <memory>

#include "base/files/platform_file.h"
#include "base/logging.h"
#include "base/posix/eintr_wrapper.h"
#include "base/strings/stringprintf.h"
#include "base/trace_event/trace_event.h"
#include "ui/gfx/buffer_format_util.h"
#include "ui/gfx/buffer_usage_util.h"
#include "ui/gfx/geometry/size_conversions.h"
#include "ui/gfx/native_pixmap_handle.h"
#include "ui/ozone/common/linux/drm_util_linux.h"
#include "ui/ozone/common/linux/gbm_device.h"
#include "ui/ozone/platform/wayland/gpu/gbm_surfaceless_wayland.h"
#include "ui/ozone/platform/wayland/gpu/wayland_buffer_manager_gpu.h"
#include "ui/ozone/platform/wayland/gpu/wayland_surface_factory.h"
#include "ui/ozone/public/overlay_plane.h"
#include "ui/ozone/public/ozone_platform.h"

namespace ui {

GbmPixmapWayland::GbmPixmapWayland(WaylandSurfaceFactory* surface_manager,
                                   WaylandBufferManagerGpu* buffer_manager,
                                   gfx::AcceleratedWidget widget)
    : surface_manager_(surface_manager),
      buffer_manager_(buffer_manager),
      widget_(widget) {}

GbmPixmapWayland::~GbmPixmapWayland() {
  if (gbm_bo_ && widget_ != gfx::kNullAcceleratedWidget)
    buffer_manager_->DestroyBuffer(widget_, GetUniqueId());
}

bool GbmPixmapWayland::InitializeBuffer(gfx::Size size,
                                        gfx::BufferFormat format,
                                        gfx::BufferUsage usage) {
  TRACE_EVENT0("wayland", "GbmPixmapWayland::InitializeBuffer");

  if (!buffer_manager_->gbm_device())
    return false;

  uint32_t flags = 0;
  switch (usage) {
    case gfx::BufferUsage::GPU_READ:
      flags = GBM_BO_USE_LINEAR;
      break;
    case gfx::BufferUsage::SCANOUT:
      flags = GBM_BO_USE_RENDERING | GBM_BO_USE_SCANOUT;
      break;
    case gfx::BufferUsage::SCANOUT_CAMERA_READ_WRITE:
      flags = GBM_BO_USE_LINEAR | GBM_BO_USE_WRITE | GBM_BO_USE_SCANOUT;
      break;
    case gfx::BufferUsage::SCANOUT_CPU_READ_WRITE:
      flags = GBM_BO_USE_LINEAR | GBM_BO_USE_SCANOUT;
      break;
    case gfx::BufferUsage::SCANOUT_VDA_WRITE:
      flags = GBM_BO_USE_SCANOUT;
      break;
    case gfx::BufferUsage::GPU_READ_CPU_READ_WRITE:
      flags = GBM_BO_USE_LINEAR;
      break;
    default:
      NOTREACHED() << "Not supported buffer format";
      break;
  }

  const uint32_t fourcc_format = GetFourCCFormatFromBufferFormat(format);
  gbm_bo_ =
      buffer_manager_->gbm_device()->CreateBuffer(fourcc_format, size, flags);
  if (!gbm_bo_) {
    LOG(ERROR) << "Cannot create bo with format= "
               << gfx::BufferFormatToString(format) << " and usage "
               << gfx::BufferUsageToString(usage);
    return false;
  }

  // The pixmap can be created as a staging buffer and not be mapped to any of
  // the existing widgets.
  if (widget_ != gfx::kNullAcceleratedWidget)
    CreateDmabufBasedBuffer();
  return true;
}

bool GbmPixmapWayland::AreDmaBufFdsValid() const {
  return gbm_bo_->AreFdsValid();
}

int GbmPixmapWayland::GetDmaBufFd(size_t plane) const {
  return gbm_bo_->GetPlaneFd(plane);
}

int GbmPixmapWayland::GetDmaBufPitch(size_t plane) const {
  return gbm_bo_->GetPlaneStride(plane);
}

int GbmPixmapWayland::GetDmaBufOffset(size_t plane) const {
  return gbm_bo_->GetPlaneOffset(plane);
}

uint64_t GbmPixmapWayland::GetBufferFormatModifier() const {
  return gbm_bo_->GetFormatModifier();
}

gfx::BufferFormat GbmPixmapWayland::GetBufferFormat() const {
  return gbm_bo_->GetBufferFormat();
}

gfx::Size GbmPixmapWayland::GetBufferSize() const {
  return gbm_bo_->GetSize();
}

uint32_t GbmPixmapWayland::GetUniqueId() const {
  return gbm_bo_->GetHandle();
}

bool GbmPixmapWayland::ScheduleOverlayPlane(
    gfx::AcceleratedWidget widget,
    int plane_z_order,
    gfx::OverlayTransform plane_transform,
    const gfx::Rect& display_bounds,
    const gfx::RectF& crop_rect,
    bool enable_blend,
    std::unique_ptr<gfx::GpuFence> gpu_fence) {
  GbmSurfacelessWayland* surfaceless = surface_manager_->GetSurface(widget);
  DCHECK(surfaceless);
  surfaceless->QueueOverlayPlane(
      OverlayPlane(this, std::move(gpu_fence), plane_z_order, plane_transform,
                   display_bounds, crop_rect, enable_blend));
  return true;
}

gfx::NativePixmapHandle GbmPixmapWayland::ExportHandle() {
  gfx::NativePixmapHandle handle;
  gfx::BufferFormat format = GetBufferFormat();

  // TODO(dcastagna): Use gbm_bo_get_plane_count once all the formats we use are
  // supported by gbm.
  const size_t num_planes = gfx::NumberOfPlanesForBufferFormat(format);
  std::vector<base::ScopedFD> scoped_fds(num_planes);
  for (size_t i = 0; i < num_planes; ++i) {
    scoped_fds[i] = base::ScopedFD(HANDLE_EINTR(dup(GetDmaBufFd(i))));
    if (!scoped_fds[i].is_valid()) {
      PLOG(ERROR) << "dup";
      return gfx::NativePixmapHandle();
    }
  }

  for (size_t i = 0; i < num_planes; ++i) {
    handle.planes.emplace_back(GetDmaBufPitch(i), GetDmaBufOffset(i),
                               gbm_bo_->GetPlaneSize(i),
                               std::move(scoped_fds[i]));
  }
  handle.modifier = GetBufferFormatModifier();
  return handle;
}

void GbmPixmapWayland::CreateDmabufBasedBuffer() {
  uint64_t modifier = gbm_bo_->GetFormatModifier();

  std::vector<uint32_t> strides;
  std::vector<uint32_t> offsets;
  std::vector<uint64_t> modifiers;

  size_t plane_count = gbm_bo_->GetNumPlanes();
  for (size_t i = 0; i < plane_count; ++i) {
    strides.push_back(GetDmaBufPitch(i));
    offsets.push_back(GetDmaBufOffset(i));
    modifiers.push_back(modifier);
  }

  base::ScopedFD fd(HANDLE_EINTR(dup(GetDmaBufFd(0))));
  if (!fd.is_valid()) {
    PLOG(FATAL) << "dup";
    return;
  }
  // Asks Wayland to create a wl_buffer based on the |file| fd.
  buffer_manager_->CreateDmabufBasedBuffer(
      widget_, std::move(fd), GetBufferSize(), strides, offsets, modifiers,
      gbm_bo_->GetFormat(), plane_count, GetUniqueId());
}

}  // namespace ui
