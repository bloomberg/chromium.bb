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

uint32_t GetFourCCCodeForSkColorType(SkColorType type) {
  switch (type) {
    case kUnknown_SkColorType:
    case kAlpha_8_SkColorType:
      return 0;
    case kRGB_565_SkColorType:
      return DRM_FORMAT_RGB565;
    case kARGB_4444_SkColorType:
      return DRM_FORMAT_ARGB4444;
    case kN32_SkColorType:
      return DRM_FORMAT_ARGB8888;
    default:
      NOTREACHED();
      return 0;
  }
}

scoped_refptr<DrmFramebuffer> AddFramebufferForDumbBuffer(
    const scoped_refptr<DrmDevice>& drm,
    uint32_t handle,
    uint32_t stride,
    const SkImageInfo& info) {
  DrmFramebuffer::AddFramebufferParams params;
  params.flags = 0;
  params.format = GetFourCCCodeForSkColorType(info.colorType());
  params.modifier = DRM_FORMAT_MOD_INVALID;
  params.width = info.width();
  params.height = info.height();
  params.num_planes = 1;
  params.handles[0] = handle;
  params.strides[0] = stride;
  return DrmFramebuffer::AddFramebuffer(drm, params);
}

}  // namespace

DrmBuffer::DrmBuffer(const scoped_refptr<DrmDevice>& drm) : drm_(drm) {
}

DrmBuffer::~DrmBuffer() {
  if (mmap_base_ && !drm_->UnmapDumbBuffer(mmap_base_, mmap_size_))
    PLOG(ERROR) << "DrmBuffer: UnmapDumbBuffer: handle " << handle_;

  if (handle_ && !drm_->DestroyDumbBuffer(handle_))
    PLOG(ERROR) << "DrmBuffer: DestroyDumbBuffer: handle " << handle_;
}

bool DrmBuffer::Initialize(const SkImageInfo& info,
                           bool should_register_framebuffer) {
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

  if (should_register_framebuffer) {
    framebuffer_ = AddFramebufferForDumbBuffer(drm_, handle_, stride_, info);
    if (!framebuffer_)
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
