// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_DRM_GPU_GBM_BUFFER_BASE_H_
#define UI_OZONE_PLATFORM_DRM_GPU_GBM_BUFFER_BASE_H_

#include <stdint.h>

#include "base/macros.h"
#include "ui/ozone/platform/drm/gpu/scanout_buffer.h"

struct gbm_bo;

namespace ui {

class GbmDevice;

// Wrapper for a GBM buffer. The base class provides common functionality
// required to prepare the buffer for scanout. It does not provide any ownership
// of the buffer. Implementations of this base class should deal with buffer
// ownership.
class GbmBufferBase : public ScanoutBuffer {
 public:
  gbm_bo* bo() const { return bo_; }
  const scoped_refptr<GbmDevice>& drm() const { return drm_; }

  // ScanoutBuffer:
  uint32_t GetFramebufferId() const override;
  uint32_t GetOpaqueFramebufferId() const override;
  uint32_t GetHandle() const override;
  gfx::Size GetSize() const override;
  uint32_t GetFramebufferPixelFormat() const override;
  uint32_t GetOpaqueFramebufferPixelFormat() const override;
  uint64_t GetFormatModifier() const override;
  const DrmDevice* GetDrmDevice() const override;
  bool RequiresGlFinish() const override;

 protected:
  GbmBufferBase(const scoped_refptr<GbmDevice>& drm,
                gbm_bo* bo,
                uint32_t format,
                uint32_t flags,
                uint64_t modifier,
                uint32_t addfb_flags);
  ~GbmBufferBase() override;

 private:
  scoped_refptr<GbmDevice> drm_;
  gbm_bo* bo_;
  uint32_t framebuffer_ = 0;
  uint32_t framebuffer_pixel_format_ = 0;
  // If |opaque_framebuffer_pixel_format_| differs from
  // |framebuffer_pixel_format_| the following member is set to a valid fb,
  // otherwise it is set to 0.
  uint32_t opaque_framebuffer_ = 0;
  uint32_t opaque_framebuffer_pixel_format_ = 0;
  uint64_t format_modifier_ = 0;

  DISALLOW_COPY_AND_ASSIGN(GbmBufferBase);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_DRM_GPU_GBM_BUFFER_BASE_H_
