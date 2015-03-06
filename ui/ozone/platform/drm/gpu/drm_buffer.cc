// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/drm/gpu/drm_buffer.h"

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

}  // namespace

DrmBuffer::DrmBuffer(const scoped_refptr<DrmDevice>& drm)
    : drm_(drm), handle_(0), framebuffer_(0) {
}

DrmBuffer::~DrmBuffer() {
  if (!surface_)
    return;

  if (framebuffer_)
    drm_->RemoveFramebuffer(framebuffer_);

  SkImageInfo info;
  void* pixels = const_cast<void*>(surface_->peekPixels(&info, NULL));
  if (!pixels)
    return;

  drm_->DestroyDumbBuffer(info, handle_, stride_, pixels);
}

bool DrmBuffer::Initialize(const SkImageInfo& info,
                           bool should_register_framebuffer) {
  void* pixels = NULL;
  if (!drm_->CreateDumbBuffer(info, &handle_, &stride_, &pixels)) {
    VLOG(2) << "Cannot create drm dumb buffer";
    return false;
  }

  if (should_register_framebuffer &&
      !drm_->AddFramebuffer(
          info.width(), info.height(), GetColorDepth(info.colorType()),
          info.bytesPerPixel() << 3, stride_, handle_, &framebuffer_)) {
    VPLOG(2) << "Failed to register framebuffer";
    return false;
  }

  surface_ = skia::AdoptRef(SkSurface::NewRasterDirect(info, pixels, stride_));
  if (!surface_) {
    VLOG(2) << "Cannot install Skia pixels for drm buffer";
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

uint32_t DrmBuffer::GetHandle() const {
  return handle_;
}

gfx::Size DrmBuffer::GetSize() const {
  return gfx::Size(surface_->width(), surface_->height());
}

DrmBufferGenerator::DrmBufferGenerator() {
}

DrmBufferGenerator::~DrmBufferGenerator() {
}

scoped_refptr<ScanoutBuffer> DrmBufferGenerator::Create(
    const scoped_refptr<DrmDevice>& drm,
    const gfx::Size& size) {
  scoped_refptr<DrmBuffer> buffer(new DrmBuffer(drm));
  SkImageInfo info = SkImageInfo::MakeN32Premul(size.width(), size.height());
  if (!buffer->Initialize(info, true /* should_register_framebuffer */))
    return NULL;

  return buffer;
}

}  // namespace ui
