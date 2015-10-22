// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/drm/gpu/drm_buffer.h"

#include <drm_fourcc.h>

#include "base/logging.h"
#include "ui/ozone/platform/drm/gpu/drm_device.h"

namespace ui {

namespace {

// Modesetting cannot happen from a buffer with transparencies. Return the size
// of a pixel without alpha.
uint8_t GetColorDepth(SkColorType type) {
  switch (type) {
    case kUnknown_SkColorType:
    case kAlpha_8_SkColorType:
      return 0;
    case kIndex_8_SkColorType:
      return 8;
    case kRGB_565_SkColorType:
      return 16;
    case kARGB_4444_SkColorType:
      return 12;
    case kN32_SkColorType:
      return 24;
    default:
      NOTREACHED();
      return 0;
  }
}

// We always ignore Alpha.
uint32_t GetFourCCCodeForSkColorType(SkColorType type) {
  switch (type) {
    case kUnknown_SkColorType:
    case kAlpha_8_SkColorType:
      return 0;
    case kIndex_8_SkColorType:
      return DRM_FORMAT_C8;
    case kRGB_565_SkColorType:
      return DRM_FORMAT_RGB565;
    case kARGB_4444_SkColorType:
      return DRM_FORMAT_XRGB4444;
    case kN32_SkColorType:
      return DRM_FORMAT_XRGB8888;
    default:
      NOTREACHED();
      return 0;
  }
}

}  // namespace

DrmBuffer::DrmBuffer(const scoped_refptr<DrmDevice>& drm) : drm_(drm) {
}

DrmBuffer::~DrmBuffer() {
  surface_.clear();

  if (framebuffer_ && !drm_->RemoveFramebuffer(framebuffer_))
    PLOG(ERROR) << "DrmBuffer: RemoveFramebuffer: fb " << framebuffer_;

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

  mmap_size_ = info.getSafeSize(stride_);
  if (!drm_->MapDumbBuffer(handle_, mmap_size_, &mmap_base_)) {
    PLOG(ERROR) << "DrmBuffer: MapDumbBuffer: handle " << handle_;
    return false;
  }

  if (should_register_framebuffer) {
    if (!drm_->AddFramebuffer(
            info.width(), info.height(), GetColorDepth(info.colorType()),
            info.bytesPerPixel() << 3, stride_, handle_, &framebuffer_)) {
      PLOG(ERROR) << "DrmBuffer: AddFramebuffer: handle " << handle_;
      return false;
    }

    fb_pixel_format_ = GetFourCCCodeForSkColorType(info.colorType());
  }

  surface_ =
      skia::AdoptRef(SkSurface::NewRasterDirect(info, mmap_base_, stride_));
  if (!surface_) {
    LOG(ERROR) << "DrmBuffer: Failed to create SkSurface: handle " << handle_;
    return false;
  }

  return true;
}

SkCanvas* DrmBuffer::GetCanvas() const {
  return surface_->getCanvas();
}

uint32_t DrmBuffer::GetFramebufferId() const {
  return framebuffer_;
}

uint32_t DrmBuffer::GetFramebufferPixelFormat() const {
  return fb_pixel_format_;
}

uint32_t DrmBuffer::GetHandle() const {
  return handle_;
}

gfx::Size DrmBuffer::GetSize() const {
  return gfx::Size(surface_->width(), surface_->height());
}

bool DrmBuffer::RequiresGlFinish() const {
  return false;
}

}  // namespace ui
