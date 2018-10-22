// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/drm/gpu/drm_buffer.h"

#include <drm_fourcc.h>

#include "base/logging.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "ui/ozone/platform/drm/gpu/drm_device.h"

namespace ui {

namespace {

}  // namespace

DrmBuffer::DrmBuffer(const scoped_refptr<DrmDevice>& drm) : drm_(drm) {
}

DrmBuffer::~DrmBuffer() {
  if (mmap_base_ && !drm_->UnmapDumbBuffer(mmap_base_, mmap_size_))
    PLOG(ERROR) << "DrmBuffer: UnmapDumbBuffer: handle " << handle_;

  if (handle_ && !drm_->DestroyDumbBuffer(handle_))
    PLOG(ERROR) << "DrmBuffer: DestroyDumbBuffer: handle " << handle_;
}

bool DrmBuffer::Initialize(const SkImageInfo& info) {
  if (!drm_->CreateDumbBuffer(info, &handle_, &stride_)) {
    PLOG(ERROR) << "DrmBuffer: CreateDumbBuffer: width " << info.width()
                << " height " << info.height();
    return false;
  }

  mmap_size_ = info.computeByteSize(stride_);
  if (!drm_->MapDumbBuffer(handle_, mmap_size_, &mmap_base_)) {
    PLOG(ERROR) << "DrmBuffer: MapDumbBuffer: handle " << handle_;
    return false;
  }

  surface_ = SkSurface::MakeRasterDirect(info, mmap_base_, stride_);
  if (!surface_) {
    LOG(ERROR) << "DrmBuffer: Failed to create SkSurface: handle " << handle_;
    return false;
  }

  return true;
}

SkCanvas* DrmBuffer::GetCanvas() const {
  return surface_->getCanvas();
}

uint32_t DrmBuffer::GetHandle() const {
  return handle_;
}

gfx::Size DrmBuffer::GetSize() const {
  return gfx::Size(surface_->width(), surface_->height());
}

}  // namespace ui
