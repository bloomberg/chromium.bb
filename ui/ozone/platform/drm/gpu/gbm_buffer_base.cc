// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/drm/gpu/gbm_buffer_base.h"

#include <gbm.h>

#include "base/logging.h"
#include "ui/ozone/platform/drm/common/drm_util.h"
#include "ui/ozone/platform/drm/gpu/gbm_device.h"

namespace ui {

GbmBufferBase::GbmBufferBase(const scoped_refptr<GbmDevice>& drm,
                             gbm_bo* bo,
                             uint32_t format,
                             uint32_t flags,
                             uint64_t modifier,
                             uint32_t addfb_flags)
    : drm_(drm), bo_(bo) {
  if (flags & GBM_BO_USE_SCANOUT) {
    DCHECK(bo_);
    framebuffer_pixel_format_ = format;
    opaque_framebuffer_pixel_format_ = GetFourCCFormatForOpaqueFramebuffer(
        GetBufferFormatFromFourCCFormat(format));
    format_modifier_ = modifier;

    uint32_t handles[4] = {0};
    uint32_t strides[4] = {0};
    uint32_t offsets[4] = {0};
    uint64_t modifiers[4] = {0};

    for (size_t i = 0; i < gbm_bo_get_num_planes(bo); ++i) {
      handles[i] = gbm_bo_get_plane_handle(bo, i).u32;
      strides[i] = gbm_bo_get_plane_stride(bo, i);
      offsets[i] = gbm_bo_get_plane_offset(bo, i);
      modifiers[i] = modifier;
    }

    // AddFramebuffer2 only considers the modifiers if addfb_flags has
    // DRM_MODE_FB_MODIFIERS set. We only set that when we've created
    // a bo with modifiers, otherwise, we rely on the "no modifiers"
    // behavior doing the right thing.
    drm_->AddFramebuffer2(gbm_bo_get_width(bo), gbm_bo_get_height(bo),
                          framebuffer_pixel_format_, handles, strides, offsets,
                          modifiers, &framebuffer_, addfb_flags);
    if (opaque_framebuffer_pixel_format_ != framebuffer_pixel_format_) {
      drm_->AddFramebuffer2(gbm_bo_get_width(bo), gbm_bo_get_height(bo),
                            opaque_framebuffer_pixel_format_, handles, strides,
                            offsets, modifiers, &opaque_framebuffer_,
                            addfb_flags);
    }
  }
}

GbmBufferBase::~GbmBufferBase() {
  if (framebuffer_)
    drm_->RemoveFramebuffer(framebuffer_);
  if (opaque_framebuffer_)
    drm_->RemoveFramebuffer(opaque_framebuffer_);
}

uint32_t GbmBufferBase::GetFramebufferId() const {
  return framebuffer_;
}

uint32_t GbmBufferBase::GetOpaqueFramebufferId() const {
  return opaque_framebuffer_ ? opaque_framebuffer_ : framebuffer_;
}

uint32_t GbmBufferBase::GetHandle() const {
  return gbm_bo_get_handle(bo_).u32;
}

gfx::Size GbmBufferBase::GetSize() const {
  return gfx::Size(gbm_bo_get_width(bo_), gbm_bo_get_height(bo_));
}

uint32_t GbmBufferBase::GetFramebufferPixelFormat() const {
  DCHECK(framebuffer_);
  return framebuffer_pixel_format_;
}

uint32_t GbmBufferBase::GetOpaqueFramebufferPixelFormat() const {
  DCHECK(framebuffer_);
  return opaque_framebuffer_pixel_format_;
}

uint64_t GbmBufferBase::GetFormatModifier() const {
  DCHECK(framebuffer_);
  return format_modifier_;
}

const DrmDevice* GbmBufferBase::GetDrmDevice() const {
  return drm_.get();
}

bool GbmBufferBase::RequiresGlFinish() const {
  return !drm_->is_primary_device();
}

}  // namespace ui
