// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/dri/gbm_buffer.h"

#include <gbm.h>

#include "base/logging.h"
#include "ui/ozone/platform/dri/dri_wrapper.h"
#include "ui/ozone/platform/dri/hardware_display_controller.h"

namespace ui {
namespace {
// Pixel configuration for the current buffer format.
// TODO(dnicoara) These will need to change once we query the hardware for
// supported configurations.
const uint8_t kColorDepth = 24;
const uint8_t kPixelDepth = 32;

int GetGbmFormatFromBufferFormat(SurfaceFactoryOzone::BufferFormat fmt) {
  switch (fmt) {
    case SurfaceFactoryOzone::UNKNOWN:
      return 0;
    // TODO(alexst): Setting this to XRGB for now to allow presentation
    // as a primary plane but disallowing overlay transparency. Address this
    // to allow both use cases.
    case SurfaceFactoryOzone::RGBA_8888:
      return GBM_FORMAT_XRGB8888;
    case SurfaceFactoryOzone::RGB_888:
      return GBM_FORMAT_RGB888;
  }
  return 0;
}
}

GbmBuffer::GbmBuffer(gbm_device* device, DriWrapper* dri, const gfx::Size& size)
    : gbm_device_(device),
      bo_(NULL),
      handle_(0),
      framebuffer_(0),
      dri_(dri),
      size_(size) {
}

GbmBuffer::~GbmBuffer() {
  if (framebuffer_)
    dri_->RemoveFramebuffer(framebuffer_);
  if (bo_)
    gbm_bo_destroy(bo_);
}

bool GbmBuffer::InitializeBuffer(SurfaceFactoryOzone::BufferFormat format,
                                 bool scanout) {
  unsigned flags = GBM_BO_USE_RENDERING;
  if (scanout)
    flags |= GBM_BO_USE_SCANOUT;
  bo_ = gbm_bo_create(gbm_device_,
                      size_.width(),
                      size_.height(),
                      GetGbmFormatFromBufferFormat(format),
                      flags);
  if (!bo_)
    return false;

  gbm_bo_set_user_data(bo_, this, NULL);
  handle_ = gbm_bo_get_handle(bo_).u32;

  if (scanout &&
      !dri_->AddFramebuffer(size_.width(),
                            size_.height(),
                            kColorDepth,
                            kPixelDepth,
                            gbm_bo_get_stride(bo_),
                            handle_,
                            &framebuffer_)) {
    return false;
  }
  return true;
}

bool GbmBuffer::Initialize() {
  return bo_ != NULL;
}

uint32_t GbmBuffer::GetFramebufferId() const {
  return framebuffer_;
}

uint32_t GbmBuffer::GetHandle() const {
  return handle_;
}

gfx::Size GbmBuffer::Size() const {
  return size_;
}

void GbmBuffer::PreSwapBuffers() {
}

void GbmBuffer::SwapBuffers() {
}

GbmPixmap::GbmPixmap(gbm_device* device, DriWrapper* dri, const gfx::Size& size)
    : buffer_(device, dri, size) {
}

GbmPixmap::~GbmPixmap() {
}

void* GbmPixmap::GetEGLClientBuffer() {
  return buffer_.bo();
}

int GbmPixmap::GetDmaBufFd() {
  return -1;
}

}  // namespace ui
