// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/drm/gpu/mock_drm_framebuffer_generator.h"

#include "ui/ozone/platform/drm/common/drm_util.h"
#include "ui/ozone/platform/drm/gpu/mock_drm_framebuffer.h"

namespace ui {

MockDrmFramebufferGenerator::MockDrmFramebufferGenerator() {}

MockDrmFramebufferGenerator::~MockDrmFramebufferGenerator() {}

scoped_refptr<DrmFramebuffer> MockDrmFramebufferGenerator::Create(
    const scoped_refptr<DrmDevice>& drm,
    uint32_t format,
    const std::vector<uint64_t>& modifiers,
    const gfx::Size& size) {
  return CreateWithModifier(
      drm, format, modifiers.empty() ? DRM_FORMAT_MOD_NONE : modifiers.front(),
      size);
}

scoped_refptr<DrmFramebuffer> MockDrmFramebufferGenerator::CreateWithModifier(
    const scoped_refptr<DrmDevice>& drm,
    uint32_t format,
    uint64_t modifier,
    const gfx::Size& size) {
  if (allocation_failure_)
    return nullptr;

  scoped_refptr<MockDrmFramebuffer> buffer(
      new MockDrmFramebuffer(size, format, modifier, drm));

  return buffer;
}

}  // namespace ui
