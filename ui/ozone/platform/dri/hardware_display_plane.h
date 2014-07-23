// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_DRI_HARDWARE_DISPLAY_PLANE_H_
#define UI_OZONE_PLATFORM_DRI_HARDWARE_DISPLAY_PLANE_H_

#include <stddef.h>
#include <stdint.h>
#include <xf86drmMode.h>

#include "base/basictypes.h"
#include "ui/ozone/platform/dri/scoped_drm_types.h"

namespace gfx {
class Rect;
}

namespace ui {

class DriWrapper;

class HardwareDisplayPlane {
 public:
  HardwareDisplayPlane(DriWrapper* drm,
                       drmModePropertySetPtr atomic_property_set,
                       ScopedDrmPlanePtr plane);

  ~HardwareDisplayPlane();

  bool Initialize();

  bool CanUseForCrtc(uint32_t crtc_id);
  bool SetPlaneData(uint32_t crtc_id,
                    uint32_t framebuffer,
                    const gfx::Rect& crtc_rect,
                    const gfx::Rect& src_rect);

  bool in_use() const { return in_use_; }
  void set_in_use(bool in_use) { in_use_ = in_use; }

 private:
  struct Property {
    Property();
    bool Initialize(DriWrapper* drm,
                    const char* name,
                    const ScopedDrmObjectPropertyPtr& plane_properties);
    uint32_t id_;
  };
  // Object containing the connection to the graphics device and wraps the API
  // calls to control it.
  DriWrapper* drm_;

  // Not owned.
  drmModePropertySetPtr property_set_;

  ScopedDrmPlanePtr plane_;
  uint32_t plane_id_;
  bool in_use_;

  Property crtc_prop_;
  Property fb_prop_;
  Property crtc_x_prop_;
  Property crtc_y_prop_;
  Property crtc_w_prop_;
  Property crtc_h_prop_;
  Property src_x_prop_;
  Property src_y_prop_;
  Property src_w_prop_;
  Property src_h_prop_;

  DISALLOW_COPY_AND_ASSIGN(HardwareDisplayPlane);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_DRI_HARDWARE_DISPLAY_PLANE_H_
