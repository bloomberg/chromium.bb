// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_GPU_WAYLAND_OVERLAY_MANAGER_H_
#define UI_OZONE_PLATFORM_WAYLAND_GPU_WAYLAND_OVERLAY_MANAGER_H_

#include "ui/gfx/native_widget_types.h"
#include "ui/ozone/public/overlay_candidates_ozone.h"
#include "ui/ozone/public/overlay_manager_ozone.h"

namespace ui {
class OverlaySurfaceCandidate;

// Ozone Wayland extension of the OverlayManagerOzone interface. It verifies the
// minimum validity of overlay candidates. Candidates' buffers are forwarded to
// Wayland server as Wayland subsurfaces. Actual HW overlay promotion happens
// later in Wayland Server.
class WaylandOverlayManager : public OverlayManagerOzone {
 public:
  WaylandOverlayManager();
  WaylandOverlayManager(const WaylandOverlayManager&) = delete;
  WaylandOverlayManager& operator=(const WaylandOverlayManager&) = delete;
  ~WaylandOverlayManager() override;

  // OverlayManagerOzone:
  std::unique_ptr<OverlayCandidatesOzone> CreateOverlayCandidates(
      gfx::AcceleratedWidget w) override;

  // Checks if overlay candidates can be displayed as overlays. Modifies
  // |candidates| to indicate if they can.
  void CheckOverlaySupport(std::vector<OverlaySurfaceCandidate>* candidates,
                           gfx::AcceleratedWidget widget);

 protected:
  // Perform basic validation to see if |candidate| is a valid request.
  bool CanHandleCandidate(const OverlaySurfaceCandidate& candidate,
                          gfx::AcceleratedWidget widget) const;
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_GPU_WAYLAND_OVERLAY_MANAGER_H_
