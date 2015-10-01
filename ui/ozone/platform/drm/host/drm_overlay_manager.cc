// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/drm/host/drm_overlay_manager.h"

#include "base/command_line.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/ozone/platform/drm/host/drm_overlay_candidates_host.h"
#include "ui/ozone/platform/drm/host/drm_window_host_manager.h"
#include "ui/ozone/public/overlay_candidates_ozone.h"
#include "ui/ozone/public/ozone_switches.h"

namespace ui {

DrmOverlayManager::DrmOverlayManager(
    DrmGpuPlatformSupportHost* platform_support_host,
    DrmWindowHostManager* manager)
    : platform_support_host_(platform_support_host), window_manager_(manager) {
  is_supported_ = base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kOzoneTestSingleOverlaySupport);
}

DrmOverlayManager::~DrmOverlayManager() {
}

scoped_ptr<OverlayCandidatesOzone> DrmOverlayManager::CreateOverlayCandidates(
    gfx::AcceleratedWidget w) {
  if (!is_supported_)
    return nullptr;
  DrmWindowHost* window = window_manager_->GetWindow(w);
  DCHECK(window);
  return make_scoped_ptr(
      new DrmOverlayCandidatesHost(platform_support_host_, window));
}

}  // namespace ui
