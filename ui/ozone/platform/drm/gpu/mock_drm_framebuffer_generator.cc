// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/drm/gpu/mock_drm_framebuffer_generator.h"

#include <drm_fourcc.h>

#include "ui/ozone/platform/drm/common/drm_util.h"
#include "ui/ozone/platform/drm/gpu/drm_device.h"

namespace ui {

namespace {

uint32_t g_current_framebuffer_id = 1;

class MockDrmFramebuffer : public DrmFramebuffer {
 public:
  MockDrmFramebuffer(const gfx::Size& size,
                     uint32_t format,
                     uint64_t modifier,
                     const scoped_refptr<DrmDevice>& drm)
      : size_(size),
        format_(format),
        modifier_(modifier),
        id_(g_current_framebuffer_id++),
        opaque_id_(g_current_framebuffer_id++),
        drm_(drm) {}

  // DrmFramebuffer:
  uint32_t GetFramebufferId() const override { return id_; }
  uint32_t GetOpaqueFramebufferId() const override { return opaque_id_; }
  gfx::Size GetSize() const override { return size_; }
  uint32_t GetFramebufferPixelFormat() const override { return format_; }
  uint32_t GetOpaqueFramebufferPixelFormat() const override { return format_; }
  uint64_t GetFormatModifier() const override { return modifier_; }
  const DrmDevice* GetDrmDevice() const override { return drm_.get(); }

 private:
  ~MockDrmFramebuffer() override {}

  gfx::Size size_;
  uint32_t format_;
  uint64_t modifier_;
  uint32_t id_;
  uint32_t opaque_id_;
  scoped_refptr<DrmDevice> drm_;

  DISALLOW_COPY_AND_ASSIGN(MockDrmFramebuffer);
};

}  // namespace

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

  scoped_refptr<DrmFramebuffer> buffer(
      new MockDrmFramebuffer(size, format, modifier, drm));

  return buffer;
}

}  // namespace ui
