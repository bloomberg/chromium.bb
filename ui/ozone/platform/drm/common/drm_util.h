// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_DRM_COMMON_DRM_UTIL_H_
#define UI_OZONE_PLATFORM_DRM_COMMON_DRM_UTIL_H_

#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/scoped_vector.h"
#include "ui/ozone/common/gpu/ozone_gpu_message_params.h"
#include "ui/ozone/platform/drm/common/scoped_drm_types.h"

typedef struct _drmModeModeInfo drmModeModeInfo;

namespace gfx {
class Point;
}

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
ScopedVector<HardwareDisplayControllerInfo> GetAvailableDisplayControllerInfos(
    int fd);

bool SameMode(const drmModeModeInfo& lhs, const drmModeModeInfo& rhs);

DisplayMode_Params CreateDisplayModeParams(const drmModeModeInfo& mode);

// |info| provides the DRM information related to the display, |fd| is the
// connection to the DRM device and |index| provides a unique identifier for the
// display. |index| will be used to generate the display id (it may be the id if
// the monitor's EDID lacks the necessary identifiers).
DisplaySnapshot_Params CreateDisplaySnapshotParams(
    HardwareDisplayControllerInfo* info,
    int fd,
    size_t display_index,
    const gfx::Point& origin);

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_DRM_COMMON_DRM_UTIL_H_
