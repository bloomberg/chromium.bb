// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_DRI_DRI_UTIL_H_
#define UI_OZONE_PLATFORM_DRI_DRI_UTIL_H_

#include "base/macros.h"
#include "base/memory/scoped_vector.h"
#include "ui/ozone/platform/dri/scoped_drm_types.h"

typedef struct _drmModeModeInfo drmModeModeInfo;

namespace ui {

// Representation of the information required to initialize and configure a
// native display.
class HardwareDisplayControllerInfo {
 public:
  HardwareDisplayControllerInfo(ScopedDrmConnectorPtr connector,
                                ScopedDrmCrtcPtr crtc);
  ~HardwareDisplayControllerInfo();

  drmModeConnector* connector() const { return connector_.get(); }
  drmModeCrtc* crtc() const { return crtc_.get(); }

 private:
  ScopedDrmConnectorPtr connector_;
  ScopedDrmCrtcPtr crtc_;

  DISALLOW_COPY_AND_ASSIGN(HardwareDisplayControllerInfo);
};

// Looks-up and parses the native display configurations returning all available
// displays.
ScopedVector<HardwareDisplayControllerInfo>
GetAvailableDisplayControllerInfos(int fd);

bool SameMode(const drmModeModeInfo& lhs, const drmModeModeInfo& rhs);

// Memory maps a DRM buffer.
bool MapDumbBuffer(int fd,
                   uint32_t handle,
                   uint32_t size,
                   void** pixels);

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_DRI_DRI_UTIL_H_
