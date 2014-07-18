// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_DRI_GBM_BUFFER_BASE_H_
#define UI_OZONE_PLATFORM_DRI_GBM_BUFFER_BASE_H_

#include "ui/ozone/platform/dri/scanout_buffer.h"

struct gbm_bo;

namespace ui {

class DriWrapper;

// Wrapper for a GBM buffer. The base class provides common functionality
// required to prepare the buffer for scanout. It does not provide any ownership
// of the buffer. Implementations of this base class should deal with buffer
// ownership.
class GbmBufferBase : public ScanoutBuffer {
 public:
  gbm_bo* bo() const { return bo_; }

  // ScanoutBuffer:
  virtual uint32_t GetFramebufferId() const OVERRIDE;
  virtual uint32_t GetHandle() const OVERRIDE;
  virtual gfx::Size GetSize() const OVERRIDE;

 protected:
  GbmBufferBase(DriWrapper* dri, gbm_bo* bo, bool scanout);
  virtual ~GbmBufferBase();

 private:
  DriWrapper* dri_;
  gbm_bo* bo_;
  uint32_t framebuffer_;

  DISALLOW_COPY_AND_ASSIGN(GbmBufferBase);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_DRI_GBM_BUFFER_BASE_H_
