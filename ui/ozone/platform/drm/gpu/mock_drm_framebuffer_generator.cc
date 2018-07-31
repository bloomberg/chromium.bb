// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/drm/gpu/mock_drm_framebuffer_generator.h"

#include <drm_fourcc.h>

#include "ui/ozone/platform/drm/common/drm_util.h"
#include "ui/ozone/platform/drm/gpu/drm_device.h"

namespace ui {

namespace {

uint32_t g_current_mock_buffer_handle = 0x1111;

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

  DrmFramebuffer::AddFramebufferParams params;
  params.format = format;
  params.modifier = modifier;
  params.width = size.width();
  params.height = size.height();
  params.num_planes = 1;
  params.handles[0] = g_current_mock_buffer_handle++;

  return DrmFramebuffer::AddFramebuffer(drm, params);
}

}  // namespace ui
