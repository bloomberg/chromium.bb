// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_DRI_HARDWARE_DISPLAY_PLANE_MANAGER_H_
#define UI_OZONE_PLATFORM_DRI_HARDWARE_DISPLAY_PLANE_MANAGER_H_

#include <stddef.h>
#include <stdint.h>
#include <xf86drmMode.h>
#include <vector>

#include "base/basictypes.h"
#include "base/memory/scoped_vector.h"
#include "ui/ozone/platform/dri/hardware_display_plane.h"
#include "ui/ozone/platform/dri/scoped_drm_types.h"

namespace gfx {
class Rect;
}  // namespace gfx

namespace ui {

class DriWrapper;

class HardwareDisplayPlaneManager {
 public:
  HardwareDisplayPlaneManager(DriWrapper* drm);

  ~HardwareDisplayPlaneManager();

  bool Initialize();

  const ScopedVector<HardwareDisplayPlane>& get_planes() { return planes_; }

 private:
  // Object containing the connection to the graphics device and wraps the API
  // calls to control it.
  DriWrapper* drm_;

  drmModePropertySetPtr property_set_;
  ScopedVector<HardwareDisplayPlane> planes_;

  DISALLOW_COPY_AND_ASSIGN(HardwareDisplayPlaneManager);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_DRI_HARDWARE_DISPLAY_PLANE_MANAGER_H_
