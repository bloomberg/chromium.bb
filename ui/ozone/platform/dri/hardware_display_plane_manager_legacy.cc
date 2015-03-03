// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/dri/hardware_display_plane_manager_legacy.h"

#include "base/bind.h"
#include "ui/ozone/platform/dri/crtc_controller.h"
#include "ui/ozone/platform/dri/drm_device.h"
#include "ui/ozone/platform/dri/scanout_buffer.h"

namespace ui {

HardwareDisplayPlaneManagerLegacy::HardwareDisplayPlaneManagerLegacy() {
}

HardwareDisplayPlaneManagerLegacy::~HardwareDisplayPlaneManagerLegacy() {
}

bool HardwareDisplayPlaneManagerLegacy::Commit(
    HardwareDisplayPlaneList* plane_list,
    bool is_sync) {
  if (plane_list->plane_list.empty())  // No assigned planes, nothing to do.
    return true;
  bool ret = true;
  // The order of operations here (set new planes, pageflip, clear old planes)
  // is designed to minimze the chance of a significant artifact occurring.
  // The planes must be updated first because the main plane no longer contains
  // their content. The old planes are removed last because the previous primary
  // plane used them as overlays and thus didn't contain their content, so we
  // must first flip to the new primary plane, which does. The error here will
  // be the delta of (new contents, old contents), but it should be barely
  // noticeable.
  for (const auto& flip : plane_list->legacy_page_flips) {
    // Permission Denied is a legitimate error
    for (const auto& plane : flip.planes) {
      if (!drm_->PageFlipOverlay(flip.crtc_id, plane.framebuffer, plane.bounds,
                                 plane.src_rect, plane.plane)) {
        LOG(ERROR) << "Cannot display plane on overlay: error="
                   << strerror(errno) << "crtc=" << flip.crtc
                   << " plane=" << plane.plane;
        ret = false;
        flip.crtc->PageFlipFailed();
        break;
      }
    }
    if (!drm_->PageFlip(flip.crtc_id, flip.framebuffer, is_sync,
                        base::Bind(&CrtcController::OnPageFlipEvent,
                                   flip.crtc->AsWeakPtr()))) {
      if (errno != EACCES) {
        LOG(ERROR) << "Cannot page flip: error='" << strerror(errno) << "'"
                   << " crtc=" << flip.crtc_id
                   << " framebuffer=" << flip.framebuffer
                   << " is_sync=" << is_sync;
        LOG(ERROR) << "Failed to commit planes";
        ret = false;
      }
      flip.crtc->PageFlipFailed();
    }
  }
  // For each element in |old_plane_list|, if it hasn't been reclaimed (by
  // this or any other HDPL), clear the overlay contents.
  for (HardwareDisplayPlane* plane : plane_list->old_plane_list) {
    if (!plane->in_use()) {
      // This plane is being released, so we need to zero it.
      if (!drm_->PageFlipOverlay(plane->owning_crtc(), 0, gfx::Rect(),
                                 gfx::Rect(), plane->plane_id())) {
        LOG(ERROR) << "Cannot free overlay: error=" << strerror(errno)
                   << "crtc=" << plane->owning_crtc()
                   << " plane=" << plane->plane_id();
        ret = false;
        break;
      }
    }
  }
  plane_list->plane_list.swap(plane_list->old_plane_list);
  plane_list->plane_list.clear();
  plane_list->legacy_page_flips.clear();
  plane_list->committed = true;
  return ret;
}

bool HardwareDisplayPlaneManagerLegacy::SetPlaneData(
    HardwareDisplayPlaneList* plane_list,
    HardwareDisplayPlane* hw_plane,
    const OverlayPlane& overlay,
    uint32_t crtc_id,
    const gfx::Rect& src_rect,
    CrtcController* crtc) {
  if (plane_list->legacy_page_flips.empty() ||
      plane_list->legacy_page_flips.back().crtc_id != crtc_id) {
    plane_list->legacy_page_flips.push_back(
        HardwareDisplayPlaneList::PageFlipInfo(
            crtc_id, overlay.buffer->GetFramebufferId(), hw_plane->plane_id(),
            crtc));
  } else {
    plane_list->legacy_page_flips.back().planes.push_back(
        HardwareDisplayPlaneList::PageFlipInfo::Plane(
            hw_plane->plane_id(), overlay.buffer->GetFramebufferId(),
            overlay.display_bounds, src_rect));
  }
  return true;
}

}  // namespace ui
