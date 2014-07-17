// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/dri/dri_buffer.h"

#include "base/logging.h"
#include "ui/ozone/platform/dri/dri_wrapper.h"

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

DriBuffer::DriBuffer(DriWrapper* dri)
    : dri_(dri), handle_(0), framebuffer_(0) {}

DriBuffer::~DriBuffer() {
  if (!surface_)
    return;

  if (framebuffer_)
    dri_->RemoveFramebuffer(framebuffer_);

  SkImageInfo info;
  void* pixels = const_cast<void*>(surface_->peekPixels(&info, NULL));
  if (!pixels)
    return;

  dri_->DestroyDumbBuffer(info, handle_, stride_, pixels);
}

bool DriBuffer::Initialize(const SkImageInfo& info) {
  void* pixels = NULL;
  if (!dri_->CreateDumbBuffer(info, &handle_, &stride_, &pixels)) {
    VLOG(2) << "Cannot create drm dumb buffer";
    return false;
  }

  if (!dri_->AddFramebuffer(info.width(),
                            info.height(),
                            GetColorDepth(info.colorType()),
                            info.bytesPerPixel() << 3,
                            stride_,
                            handle_,
                            &framebuffer_)) {
    VLOG(2) << "Failed to register framebuffer: " << strerror(errno);
    return false;
  }

  surface_ = skia::AdoptRef(SkSurface::NewRasterDirect(info, pixels, stride_));
  if (!surface_) {
    VLOG(2) << "Cannot install Skia pixels for drm buffer";
    return false;
  }

  return true;
}

}  // namespace ui
