// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_DRM_GPU_DRM_DISPLAY_MODE_H_
#define UI_OZONE_PLATFORM_DRM_GPU_DRM_DISPLAY_MODE_H_

#include <stdint.h>
#include <stdlib.h>
#include <xf86drmMode.h>

#include "ui/display/types/display_mode.h"

namespace ui {

class DrmDisplayMode : public DisplayMode {
 public:
  DrmDisplayMode(const drmModeModeInfo& mode);
  ~DrmDisplayMode() override;

  // Native details about this mode. Only used internally in the DRM
  // implementation.
  drmModeModeInfo mode_info() const { return mode_info_; }

 private:
  drmModeModeInfo mode_info_;

  DISALLOW_COPY_AND_ASSIGN(DrmDisplayMode);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_DRM_GPU_DRM_DISPLAY_MODE_H_
