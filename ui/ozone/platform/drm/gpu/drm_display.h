// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_DRM_GPU_DRM_DISPLAY_H_
#define UI_OZONE_PLATFORM_DRM_GPU_DRM_DISPLAY_H_

#include <vector>

#include "base/memory/ref_counted.h"
#include "ui/display/types/display_constants.h"

typedef struct _drmModeModeInfo drmModeModeInfo;

namespace gfx {
class Point;
}

namespace ui {

class DrmDevice;
class ScreenManager;

struct GammaRampRGBEntry;

class DrmDisplay {
 public:
  DrmDisplay(ScreenManager* screen_manager,
             int64_t display_id,
             const scoped_refptr<DrmDevice>& drm,
             uint32_t crtc,
             uint32_t connector,
             const std::vector<drmModeModeInfo>& modes);
  ~DrmDisplay();

  int64_t display_id() const { return display_id_; }
  scoped_refptr<DrmDevice> drm() const { return drm_; }
  uint32_t crtc() const { return crtc_; }
  uint32_t connector() const { return connector_; }
  const std::vector<drmModeModeInfo>& modes() const { return modes_; }

  bool Configure(const drmModeModeInfo* mode, const gfx::Point& origin);
  bool GetHDCPState(HDCPState* state);
  bool SetHDCPState(HDCPState state);
  void SetGammaRamp(const std::vector<GammaRampRGBEntry>& lut);

 private:
  ScreenManager* screen_manager_;  // Not owned.

  int64_t display_id_;
  scoped_refptr<DrmDevice> drm_;
  uint32_t crtc_;
  uint32_t connector_;
  std::vector<drmModeModeInfo> modes_;

  DISALLOW_COPY_AND_ASSIGN(DrmDisplay);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_DRM_GPU_DRM_DISPLAY_H_
