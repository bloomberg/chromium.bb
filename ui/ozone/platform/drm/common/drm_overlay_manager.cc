// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/drm/common/drm_overlay_manager.h"

#include "ui/ozone/platform/drm/common/drm_overlay_candidates.h"
#include "ui/ozone/public/overlay_surface_candidate.h"

namespace ui {

DrmOverlayManager::DrmOverlayManager() = default;

DrmOverlayManager::~DrmOverlayManager() = default;

std::unique_ptr<OverlayCandidatesOzone>
DrmOverlayManager::CreateOverlayCandidates(gfx::AcceleratedWidget widget) {
  return std::make_unique<DrmOverlayCandidates>(this, widget);
}

bool DrmOverlayManager::SupportsOverlays() const {
  return supports_overlays_;
}

}  // namespace ui
