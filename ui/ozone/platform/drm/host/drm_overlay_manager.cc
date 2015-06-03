// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/drm/host/drm_overlay_manager.h"

#include "base/command_line.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/ozone/public/overlay_candidates_ozone.h"
#include "ui/ozone/public/ozone_switches.h"

namespace ui {
namespace {

class SingleOverlay : public OverlayCandidatesOzone {
 public:
  SingleOverlay() {}
  ~SingleOverlay() override {}

  void CheckOverlaySupport(OverlaySurfaceCandidateList* candidates) override {
    if (candidates->size() == 2) {
      OverlayCandidatesOzone::OverlaySurfaceCandidate* first =
          &(*candidates)[0];
      OverlayCandidatesOzone::OverlaySurfaceCandidate* second =
          &(*candidates)[1];
      OverlayCandidatesOzone::OverlaySurfaceCandidate* overlay;
      if (first->plane_z_order == 0) {
        overlay = second;
      } else if (second->plane_z_order == 0) {
        overlay = first;
      } else {
        NOTREACHED();
        return;
      }
      // 0.01 constant chosen to match DCHECKs in gfx::ToNearestRect and avoid
      // that code asserting on quads that we accept.
      if (overlay->plane_z_order > 0 &&
          IsTransformSupported(overlay->transform) &&
          gfx::IsNearestRectWithinDistance(overlay->display_rect, 0.01f)) {
        overlay->overlay_handled = true;
      }
    }
  }

 private:
  bool IsTransformSupported(gfx::OverlayTransform transform) {
    switch (transform) {
      case gfx::OVERLAY_TRANSFORM_NONE:
        return true;
      default:
        return false;
    }
  }

  DISALLOW_COPY_AND_ASSIGN(SingleOverlay);
};

}  // namespace

DrmOverlayManager::DrmOverlayManager(bool allow_surfaceless)
    : allow_surfaceless_(allow_surfaceless) {
  is_supported_ = base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kOzoneTestSingleOverlaySupport);
}

DrmOverlayManager::~DrmOverlayManager() {
}

scoped_ptr<OverlayCandidatesOzone> DrmOverlayManager::CreateOverlayCandidates(
    gfx::AcceleratedWidget w) {
  if (!is_supported_)
    return nullptr;
  return make_scoped_ptr(new SingleOverlay());
}

bool DrmOverlayManager::CanShowPrimaryPlaneAsOverlay() {
  return allow_surfaceless_;
}

}  // namespace ui
