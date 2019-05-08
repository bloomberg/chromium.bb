// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/linux/native_pixmap_dmabuf.h"

#include <utility>

#include "base/posix/eintr_wrapper.h"

namespace gfx {

NativePixmapDmaBuf::NativePixmapDmaBuf(const gfx::Size& size,
                                       gfx::BufferFormat format,
                                       gfx::NativePixmapHandle handle)
    : size_(size), format_(format), planes_(std::move(handle.planes)) {}

NativePixmapDmaBuf::~NativePixmapDmaBuf() {}

bool NativePixmapDmaBuf::AreDmaBufFdsValid() const {
  if (planes_.empty())
    return false;

  for (const auto& plane : planes_) {
    if (!plane.fd.is_valid())
      return false;
  }
  return true;
}

int NativePixmapDmaBuf::GetDmaBufFd(size_t plane) const {
  DCHECK_LT(plane, planes_.size());
  return planes_[plane].fd.get();
}

int NativePixmapDmaBuf::GetDmaBufPitch(size_t plane) const {
  DCHECK_LT(plane, planes_.size());
  return planes_[plane].stride;
}

int NativePixmapDmaBuf::GetDmaBufOffset(size_t plane) const {
  DCHECK_LT(plane, planes_.size());
  return planes_[plane].offset;
}

uint64_t NativePixmapDmaBuf::GetBufferFormatModifier() const {
  // Modifiers must be the same on all the planes. Return the modifier of the
  // first plane.
  // TODO(crbug.com/957381): Move modifier variable to NativePixmapHandle from
  // NativePixmapPlane.
  return planes_[0].modifier;
}

gfx::BufferFormat NativePixmapDmaBuf::GetBufferFormat() const {
  return format_;
}

gfx::Size NativePixmapDmaBuf::GetBufferSize() const {
  return size_;
}

uint32_t NativePixmapDmaBuf::GetUniqueId() const {
  return 0;
}

bool NativePixmapDmaBuf::ScheduleOverlayPlane(
    gfx::AcceleratedWidget widget,
    int plane_z_order,
    gfx::OverlayTransform plane_transform,
    const gfx::Rect& display_bounds,
    const gfx::RectF& crop_rect,
    bool enable_blend,
    std::unique_ptr<gfx::GpuFence> gpu_fence) {
  return false;
}

gfx::NativePixmapHandle NativePixmapDmaBuf::ExportHandle() {
  return gfx::NativePixmapHandle();
}

}  // namespace gfx
