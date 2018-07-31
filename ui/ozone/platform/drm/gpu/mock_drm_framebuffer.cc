// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/drm/gpu/mock_drm_framebuffer.h"
#include "ui/ozone/platform/drm/gpu/mock_drm_device.h"

namespace ui {

namespace {

uint32_t g_current_framebuffer_id = 1;

}  // namespace

MockDrmFramebuffer::MockDrmFramebuffer(const gfx::Size& size,
                                       uint32_t format,
                                       uint64_t modifier,
                                       const scoped_refptr<DrmDevice>& drm)
    : size_(size),
      format_(format),
      modifier_(modifier),
      id_(g_current_framebuffer_id++),
      opaque_id_(g_current_framebuffer_id++),
      drm_(drm) {}

MockDrmFramebuffer::~MockDrmFramebuffer() {}

uint32_t MockDrmFramebuffer::GetFramebufferId() const {
  return id_;
}

uint32_t MockDrmFramebuffer::GetOpaqueFramebufferId() const {
  return opaque_id_;
}

gfx::Size MockDrmFramebuffer::GetSize() const {
  return size_;
}

uint32_t MockDrmFramebuffer::GetFramebufferPixelFormat() const {
  return format_;
}

uint32_t MockDrmFramebuffer::GetOpaqueFramebufferPixelFormat() const {
  return format_;
}

uint64_t MockDrmFramebuffer::GetFormatModifier() const {
  return modifier_;
}

const DrmDevice* MockDrmFramebuffer::GetDrmDevice() const {
  return drm_.get();
}

}  // namespace ui
