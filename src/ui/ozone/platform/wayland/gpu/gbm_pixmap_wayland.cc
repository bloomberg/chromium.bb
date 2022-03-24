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
#include "base/trace_event/trace_event.h"
#include "ui/gfx/buffer_format_util.h"
#include "ui/gfx/buffer_usage_util.h"
#include "ui/gfx/geometry/size_conversions.h"
#include "ui/gfx/linux/drm_util_linux.h"
#include "ui/gfx/linux/gbm_device.h"
#include "ui/gfx/linux/gbm_util.h"
#include "ui/gfx/native_pixmap_handle.h"
#include "ui/ozone/platform/wayland/gpu/gbm_surfaceless_wayland.h"
#include "ui/ozone/platform/wayland/gpu/wayland_buffer_manager_gpu.h"
#include "ui/ozone/public/overlay_plane.h"
#include "ui/ozone/public/ozone_platform.h"

namespace ui {

GbmPixmapWayland::GbmPixmapWayland(WaylandBufferManagerGpu* buffer_manager)
    : buffer_manager_(buffer_manager),
      buffer_id_(buffer_manager->AllocateBufferID()) {}

GbmPixmapWayland::~GbmPixmapWayland() {
  if (gbm_bo_ && widget_ != gfx::kNullAcceleratedWidget)
    buffer_manager_->DestroyBuffer(widget_, buffer_id_);
}

bool GbmPixmapWayland::InitializeBuffer(
    gfx::AcceleratedWidget widget,
    gfx::Size size,
    gfx::BufferFormat format,
    gfx::BufferUsage usage,
    absl::optional<gfx::Size> visible_area_size) {
  DCHECK(!visible_area_size ||
         ((visible_area_size.value().width() <= size.width()) &&
          (visible_area_size.value().height() <= size.height())));
  TRACE_EVENT0("wayland", "GbmPixmapWayland::InitializeBuffer");

  widget_ = widget;

  auto* gbm_device = buffer_manager_->GetGbmDevice();
  if (!gbm_device)
    return false;

  const uint32_t fourcc_format = GetFourCCFormatFromBufferFormat(format);
  const uint32_t gbm_flags = ui::BufferUsageToGbmFlags(usage);
  auto modifiers = buffer_manager_->GetModifiersForBufferFormat(format);

  // Create buffer object without format modifiers unless they are explicitly
  // advertised by the Wayland compositor, via linux-dmabuf protocol.
  if (modifiers.empty()) {
    gbm_bo_ = gbm_device->CreateBuffer(fourcc_format, size, gbm_flags);
  } else {
    // When buffer |usage| implies on GBM_BO_USE_LINEAR, pass in
    // DRM_FORMAT_MOD_LINEAR, i.e: no tiling, when creating gbm buffers,
    // otherwise it fails to create BOs.
    if (gbm_flags & GBM_BO_USE_LINEAR)
      modifiers = {DRM_FORMAT_MOD_LINEAR};
    gbm_bo_ = gbm_device->CreateBufferWithModifiers(fourcc_format, size,
                                                    gbm_flags, modifiers);
  }

  if (!gbm_bo_) {
    LOG(ERROR) << "Cannot create bo with format= "
               << gfx::BufferFormatToString(format)
               << " and usage=" << gfx::BufferUsageToString(usage);
    return false;
  }

  DVLOG(3) << "Created gbm bo. format= " << gfx::BufferFormatToString(format)
           << " usage=" << gfx::BufferUsageToString(usage);

  visible_area_size_ = visible_area_size ? visible_area_size.value() : size;
  if (widget_ != gfx::kNullAcceleratedWidget)
    CreateDmabufBasedBuffer();
  return true;
}

bool GbmPixmapWayland::InitializeBufferFromHandle(
    gfx::AcceleratedWidget widget,
    gfx::Size size,
    gfx::BufferFormat format,
    gfx::NativePixmapHandle handle) {
  TRACE_EVENT0("wayland", "GbmPixmapWayland::InitializeBufferFromHandle");
  auto* gbm_device = buffer_manager_->GetGbmDevice();
  if (!gbm_device)
    return false;

  widget_ = widget;

  // Create a buffer object from handle.
  gbm_bo_ = gbm_device->CreateBufferFromHandle(
      GetFourCCFormatFromBufferFormat(format), size, std::move(handle));
  if (!gbm_bo_) {
    LOG(ERROR) << "Cannot create bo with format= "
               << gfx::BufferFormatToString(format);
    return false;
  }

  DVLOG(3) << "Created gbm bo. format= " << gfx::BufferFormatToString(format);

  visible_area_size_ = size;
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

uint32_t GbmPixmapWayland::GetDmaBufPitch(size_t plane) const {
  return gbm_bo_->GetPlaneStride(plane);
}

size_t GbmPixmapWayland::GetDmaBufOffset(size_t plane) const {
  return gbm_bo_->GetPlaneOffset(plane);
}

size_t GbmPixmapWayland::GetDmaBufPlaneSize(size_t plane) const {
  return gbm_bo_->GetPlaneSize(plane);
}

size_t GbmPixmapWayland::GetNumberOfPlanes() const {
  return gbm_bo_->GetNumPlanes();
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
    const gfx::OverlayPlaneData& overlay_plane_data,
    std::vector<gfx::GpuFence> acquire_fences,
    std::vector<gfx::GpuFence> release_fences) {
  DCHECK_NE(widget, gfx::kNullAcceleratedWidget);

  if (widget_ == gfx::kNullAcceleratedWidget)
    CreateDmabufBasedBuffer();

  widget_ = widget;

  auto* surface = buffer_manager_->GetSurface(widget);
  // This must never be hit.
  DCHECK(surface);
  GbmSurfacelessWayland* surfaceless =
      static_cast<GbmSurfacelessWayland*>(surface);
  DCHECK(surfaceless);

  DCHECK(acquire_fences.empty() || acquire_fences.size() == 1u);
  surfaceless->QueueOverlayPlane(
      OverlayPlane(this,
                   acquire_fences.empty() ? nullptr
                                          : std::make_unique<gfx::GpuFence>(
                                                std::move(acquire_fences[0])),
                   overlay_plane_data),
      buffer_id_);
  return true;
}

gfx::NativePixmapHandle GbmPixmapWayland::ExportHandle() {
  gfx::NativePixmapHandle handle;
  gfx::BufferFormat format = GetBufferFormat();

  // TODO(dcastagna): Use gbm_bo_get_plane_count once all the formats we use are
  // supported by gbm.
  const size_t num_planes = gfx::NumberOfPlanesForLinearBufferFormat(format);
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
      std::move(fd), visible_area_size_, strides, offsets, modifiers,
      gbm_bo_->GetFormat(), plane_count, buffer_id_);
}

}  // namespace ui
