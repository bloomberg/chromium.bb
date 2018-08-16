// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_DRM_GPU_MOCK_SCANOUT_BUFFER_GENERATOR_H_
#define UI_OZONE_PLATFORM_DRM_GPU_MOCK_SCANOUT_BUFFER_GENERATOR_H_

#include "base/macros.h"

#include "ui/ozone/platform/drm/gpu/drm_framebuffer.h"
#include "ui/ozone/platform/drm/gpu/drm_framebuffer_generator.h"

namespace ui {

class MockDrmFramebufferGenerator : public DrmFramebufferGenerator {
 public:
  MockDrmFramebufferGenerator();
  ~MockDrmFramebufferGenerator() override;

  // DrmFramebufferGenerator:
  scoped_refptr<DrmFramebuffer> Create(const scoped_refptr<DrmDevice>& drm,
                                       uint32_t format,
                                       const std::vector<uint64_t>& modifiers,
                                       const gfx::Size& size) override;

  scoped_refptr<DrmFramebuffer> CreateWithModifier(
      const scoped_refptr<DrmDevice>& drm,
      uint32_t format,
      uint64_t modifier,
      const gfx::Size& size);

  void set_allocation_failure(bool allocation_failure) {
    allocation_failure_ = allocation_failure;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(MockDrmFramebufferGenerator);

  bool allocation_failure_ = false;
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_DRM_GPU_MOCK_SCANOUT_BUFFER_GENERATOR_H_
