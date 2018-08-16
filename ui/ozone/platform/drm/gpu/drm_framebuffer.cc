// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/drm/gpu/drm_framebuffer.h"

#include "ui/ozone/common/linux/drm_util_linux.h"
#include "ui/ozone/platform/drm/common/drm_util.h"
#include "ui/ozone/platform/drm/gpu/drm_device.h"

namespace ui {

// static
scoped_refptr<DrmFramebuffer> DrmFramebuffer::AddFramebuffer(
    scoped_refptr<DrmDevice> drm_device,
    DrmFramebuffer::AddFramebufferParams params) {
  uint64_t modifiers[4] = {0};
  if (params.modifier != DRM_FORMAT_MOD_INVALID) {
    for (size_t i = 0; i < params.num_planes; ++i)
      modifiers[i] = params.modifier;
  }

  uint32_t framebuffer_id = 0;
  if (!drm_device->AddFramebuffer2(params.width, params.height, params.format,
                                   params.handles, params.strides,
                                   params.offsets, modifiers, &framebuffer_id,
                                   params.flags)) {
    DPLOG(WARNING) << "AddFramebuffer2";
    return nullptr;
  }

  uint32_t opaque_format = GetFourCCFormatForOpaqueFramebuffer(
      GetBufferFormatFromFourCCFormat(params.format));
  uint32_t opaque_framebuffer_id = 0;
  if (opaque_format != params.format &&
      !drm_device->AddFramebuffer2(params.width, params.height, opaque_format,
                                   params.handles, params.strides,
                                   params.offsets, modifiers,
                                   &opaque_framebuffer_id, params.flags)) {
    DPLOG(WARNING) << "AddFramebuffer2";
    drm_device->RemoveFramebuffer(framebuffer_id);
    return nullptr;
  }

  return base::MakeRefCounted<DrmFramebuffer>(
      std::move(drm_device), framebuffer_id, params.format,
      opaque_framebuffer_id, opaque_format, params.modifier,
      gfx::Size(params.width, params.height));
}

DrmFramebuffer::DrmFramebuffer(scoped_refptr<DrmDevice> drm_device,
                               uint32_t framebuffer_id,
                               uint32_t framebuffer_pixel_format,
                               uint32_t opaque_framebuffer_id,
                               uint32_t opaque_framebuffer_pixel_format,
                               uint64_t format_modifier,
                               const gfx::Size& size)
    : drm_device_(std::move(drm_device)),
      framebuffer_id_(framebuffer_id),
      framebuffer_pixel_format_(framebuffer_pixel_format),
      opaque_framebuffer_id_(opaque_framebuffer_id),
      opaque_framebuffer_pixel_format_(opaque_framebuffer_pixel_format),
      format_modifier_(format_modifier),
      size_(size) {}

DrmFramebuffer::~DrmFramebuffer() {
  if (!drm_device_->RemoveFramebuffer(framebuffer_id_))
    PLOG(WARNING) << "RemoveFramebuffer";
  if (opaque_framebuffer_id_ &&
      !drm_device_->RemoveFramebuffer(opaque_framebuffer_id_))
    PLOG(WARNING) << "RemoveFramebuffer";
}

}  // namespace ui
