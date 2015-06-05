// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/drm/host/drm_overlay_manager.h"

#include "base/command_line.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/ozone/platform/drm/host/drm_overlay_candidates_host.h"
#include "ui/ozone/public/overlay_candidates_ozone.h"
#include "ui/ozone/public/ozone_switches.h"

namespace ui {

DrmOverlayManager::DrmOverlayManager(
    bool allow_surfaceless,
    DrmGpuPlatformSupportHost* platform_support_host)
    : platform_support_host_(platform_support_host),
      allow_surfaceless_(allow_surfaceless) {
  is_supported_ = base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kOzoneTestSingleOverlaySupport);
}

DrmOverlayManager::~DrmOverlayManager() {
}

scoped_ptr<OverlayCandidatesOzone> DrmOverlayManager::CreateOverlayCandidates(
    gfx::AcceleratedWidget w) {
  if (!is_supported_)
    return nullptr;
  return make_scoped_ptr(
      new DrmOverlayCandidatesHost(w, platform_support_host_));
}

bool DrmOverlayManager::CanShowPrimaryPlaneAsOverlay() {
  return allow_surfaceless_;
}

}  // namespace ui
