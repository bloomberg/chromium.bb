// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_DRM_GPU_MOCK_DUMB_BUFFER_GENERATOR_H_
#define UI_OZONE_PLATFORM_DRM_GPU_MOCK_DUMB_BUFFER_GENERATOR_H_

#include "base/macros.h"

#include "ui/ozone/platform/drm/gpu/drm_framebuffer.h"
#include "ui/ozone/platform/drm/gpu/drm_framebuffer_generator.h"

namespace ui {

class DrmFramebuffer;

class MockDumbBufferGenerator : public DrmFramebufferGenerator {
 public:
  MockDumbBufferGenerator();
  ~MockDumbBufferGenerator() override;

  // DrmFramebufferGenerator:
  scoped_refptr<DrmFramebuffer> Create(const scoped_refptr<DrmDevice>& drm,
                                       uint32_t format,
                                       const std::vector<uint64_t>& modifiers,
                                       const gfx::Size& size) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(MockDumbBufferGenerator);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_DRM_GPU_MOCK_DUMB_BUFFER_GENERATOR_H_
