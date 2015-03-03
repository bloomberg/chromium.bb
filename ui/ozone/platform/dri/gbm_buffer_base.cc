// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/dri/gbm_buffer_base.h"

#include <gbm.h>

#include "base/logging.h"
#include "ui/ozone/platform/dri/drm_device.h"

namespace ui {

namespace {

// Pixel configuration for the current buffer format.
// TODO(dnicoara) These will need to change once we query the hardware for
// supported configurations.
const uint8_t kColorDepth = 24;
const uint8_t kPixelDepth = 32;

}  // namespace

GbmBufferBase::GbmBufferBase(const scoped_refptr<DrmDevice>& drm,
                             gbm_bo* bo,
                             bool scanout)
    : drm_(drm), bo_(bo), framebuffer_(0) {
  if (scanout &&
      !drm_->AddFramebuffer(gbm_bo_get_width(bo), gbm_bo_get_height(bo),
                            kColorDepth, kPixelDepth, gbm_bo_get_stride(bo),
                            gbm_bo_get_handle(bo).u32, &framebuffer_))
    LOG(ERROR) << "Failed to register buffer";
}

GbmBufferBase::~GbmBufferBase() {
  if (framebuffer_)
    drm_->RemoveFramebuffer(framebuffer_);
}

uint32_t GbmBufferBase::GetFramebufferId() const {
  return framebuffer_;
}

uint32_t GbmBufferBase::GetHandle() const {
  return gbm_bo_get_handle(bo_).u32;
}

gfx::Size GbmBufferBase::GetSize() const {
  return gfx::Size(gbm_bo_get_width(bo_), gbm_bo_get_height(bo_));
}

}  // namespace ui
