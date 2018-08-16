// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/drm/gpu/mock_dumb_buffer_generator.h"

#include "third_party/skia/include/core/SkImageInfo.h"
#include "ui/ozone/platform/drm/gpu/drm_buffer.h"
#include "ui/ozone/platform/drm/gpu/drm_device.h"

namespace ui {

namespace {

uint32_t GetFourCCCodeForSkColorType(SkColorType type) {
  switch (type) {
    case kUnknown_SkColorType:
    case kAlpha_8_SkColorType:
      return 0;
    case kRGB_565_SkColorType:
      return DRM_FORMAT_RGB565;
    case kARGB_4444_SkColorType:
      return DRM_FORMAT_ARGB4444;
    case kN32_SkColorType:
      return DRM_FORMAT_ARGB8888;
    default:
      NOTREACHED();
      return 0;
  }
}

scoped_refptr<DrmFramebuffer> AddFramebufferForDumbBuffer(
    const scoped_refptr<DrmDevice>& drm,
    uint32_t handle,
    uint32_t stride,
    const SkImageInfo& info) {
  DrmFramebuffer::AddFramebufferParams params;
  params.flags = 0;
  params.format = GetFourCCCodeForSkColorType(info.colorType());
  params.modifier = DRM_FORMAT_MOD_INVALID;
  params.width = info.width();
  params.height = info.height();
  params.num_planes = 1;
  params.handles[0] = handle;
  params.strides[0] = stride;
  return DrmFramebuffer::AddFramebuffer(drm, params);
}

}  // namespace

MockDumbBufferGenerator::MockDumbBufferGenerator() {}

MockDumbBufferGenerator::~MockDumbBufferGenerator() {}

scoped_refptr<DrmFramebuffer> MockDumbBufferGenerator::Create(
    const scoped_refptr<DrmDevice>& drm,
    uint32_t format,
    const std::vector<uint64_t>& modifiers,
    const gfx::Size& size) {
  std::unique_ptr<DrmBuffer> buffer(new DrmBuffer(drm));
  SkImageInfo info = SkImageInfo::MakeN32Premul(size.width(), size.height());
  if (!buffer->Initialize(info))
    return NULL;

  return AddFramebufferForDumbBuffer(drm, buffer->GetHandle(), buffer->stride(),
                                     info);
}

}  // namespace ui
