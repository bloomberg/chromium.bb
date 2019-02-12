// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_DRM_COMMON_DRM_OVERLAY_MANAGER_H_
#define UI_OZONE_PLATFORM_DRM_COMMON_DRM_OVERLAY_MANAGER_H_

#include <memory>
#include <vector>

#include "base/macros.h"
#include "ui/ozone/public/overlay_manager_ozone.h"

namespace ui {
class OverlayCandidatesOzone;
class OverlaySurfaceCandidate;

// Ozone DRM extension of the OverlayManagerOzone interface.
class DrmOverlayManager : public OverlayManagerOzone {
 public:
  DrmOverlayManager();
  ~DrmOverlayManager() override;

  void set_supports_overlays(bool supports_overlays) {
    supports_overlays_ = supports_overlays;
  }

  // OverlayManagerOzone:
  std::unique_ptr<OverlayCandidatesOzone> CreateOverlayCandidates(
      gfx::AcceleratedWidget w) override;
  bool SupportsOverlays() const override;

  // Resets the cache of OverlaySurfaceCandidates and if they can be displayed
  // as an overlay. For use when display configuration changes.
  virtual void ResetCache() = 0;

  // Checks if overlay candidates can be displayed as overlays. Modifies
  // |candidates| to indicate if they can.
  virtual void CheckOverlaySupport(
      std::vector<OverlaySurfaceCandidate>* candidates,
      gfx::AcceleratedWidget widget) = 0;

 private:
  // Whether we have DRM atomic capabilities and can support HW overlays.
  bool supports_overlays_ = false;

  DISALLOW_COPY_AND_ASSIGN(DrmOverlayManager);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_DRM_COMMON_DRM_OVERLAY_MANAGER_H_
