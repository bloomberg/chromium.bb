// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/scenic/sysmem_native_pixmap.h"

#include "base/fuchsia/fuchsia_logging.h"
#include "base/logging.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/gpu_fence.h"
#include "ui/gfx/overlay_plane_data.h"
#include "ui/ozone/platform/scenic/scenic_surface.h"
#include "ui/ozone/platform/scenic/scenic_surface_factory.h"

namespace ui {

namespace {

zx::event GpuFenceToZxEvent(gfx::GpuFence fence) {
  DCHECK(!fence.GetGpuFenceHandle().is_null());
  return fence.GetGpuFenceHandle().Clone().owned_event;
}

}  // namespace

SysmemNativePixmap::SysmemNativePixmap(
    scoped_refptr<SysmemBufferCollection> collection,
    gfx::NativePixmapHandle handle)
    : collection_(collection), handle_(std::move(handle)) {}

SysmemNativePixmap::~SysmemNativePixmap() = default;

bool SysmemNativePixmap::AreDmaBufFdsValid() const {
  return false;
}

int SysmemNativePixmap::GetDmaBufFd(size_t plane) const {
  NOTREACHED();
  return -1;
}

uint32_t SysmemNativePixmap::GetDmaBufPitch(size_t plane) const {
  NOTREACHED();
  return 0u;
}

size_t SysmemNativePixmap::GetDmaBufOffset(size_t plane) const {
  NOTREACHED();
  return 0u;
}

size_t SysmemNativePixmap::GetDmaBufPlaneSize(size_t plane) const {
  NOTREACHED();
  return 0;
}

size_t SysmemNativePixmap::GetNumberOfPlanes() const {
  NOTREACHED();
  return 0;
}

uint64_t SysmemNativePixmap::GetBufferFormatModifier() const {
  NOTREACHED();
  return 0;
}

gfx::BufferFormat SysmemNativePixmap::GetBufferFormat() const {
  return collection_->format();
}

gfx::Size SysmemNativePixmap::GetBufferSize() const {
  return collection_->size();
}

uint32_t SysmemNativePixmap::GetUniqueId() const {
  return 0;
}

bool SysmemNativePixmap::ScheduleOverlayPlane(
    gfx::AcceleratedWidget widget,
    const gfx::OverlayPlaneData& overlay_plane_data,
    std::vector<gfx::GpuFence> acquire_fences,
    std::vector<gfx::GpuFence> release_fences) {
  DCHECK(collection_->scenic_overlay_view());
  ScenicOverlayView* overlay_view = collection_->scenic_overlay_view();
  const auto& buffer_collection_id = handle_.buffer_collection_id.value();
  if (!overlay_view->AttachToScenicSurface(widget, buffer_collection_id)) {
    DLOG(ERROR) << "Failed to attach to surface.";
    return false;
  }

  // Convert gfx::GpuFence to zx::event for PresentImage call.
  std::vector<zx::event> acquire_events;
  for (auto& fence : acquire_fences)
    acquire_events.push_back(GpuFenceToZxEvent(std::move(fence)));
  std::vector<zx::event> release_events;
  for (auto& fence : release_fences)
    release_events.push_back(GpuFenceToZxEvent(std::move(fence)));

  overlay_view->SetBlendMode(overlay_plane_data.enable_blend);
  overlay_view->PresentImage(handle_.buffer_index, std::move(acquire_events),
                             std::move(release_events));

  return true;
}

gfx::NativePixmapHandle SysmemNativePixmap::ExportHandle() {
  return gfx::CloneHandleForIPC(handle_);
}

const gfx::NativePixmapHandle& SysmemNativePixmap::PeekHandle() const {
  return handle_;
}

bool SysmemNativePixmap::SupportsOverlayPlane(
    gfx::AcceleratedWidget widget) const {
  if (!collection_->scenic_overlay_view())
    return false;

  return collection_->scenic_overlay_view()->CanAttachToAcceleratedWidget(
      widget);
}

}  // namespace ui
