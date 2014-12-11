// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/dri/hardware_display_plane_manager_legacy.h"

#include "ui/ozone/platform/dri/crtc_controller.h"
#include "ui/ozone/platform/dri/dri_wrapper.h"
#include "ui/ozone/platform/dri/scanout_buffer.h"

namespace ui {

HardwareDisplayPlaneManagerLegacy::HardwareDisplayPlaneManagerLegacy() {
}

HardwareDisplayPlaneManagerLegacy::~HardwareDisplayPlaneManagerLegacy() {
}

bool HardwareDisplayPlaneManagerLegacy::Commit(
    HardwareDisplayPlaneList* plane_list) {
  bool ret = true;
  plane_list->plane_list.swap(plane_list->old_plane_list);
  plane_list->plane_list.clear();
  for (const auto& flip : plane_list->legacy_page_flips) {
    // Permission Denied is a legitimate error
    if (!drm_->PageFlip(flip.crtc_id, flip.framebuffer, flip.crtc)) {
      if (errno == EACCES)
        continue;
      LOG(ERROR) << "Cannot page flip: error='" << strerror(errno) << "'"
                 << " crtc=" << flip.crtc_id
                 << " framebuffer=" << flip.framebuffer;
      ret = false;
      flip.crtc->PageFlipFailed();
    } else {
      for (const auto& plane : flip.planes) {
        if (!drm_->PageFlipOverlay(flip.crtc_id, plane.framebuffer,
                                   plane.bounds, plane.src_rect, plane.plane)) {
          LOG(ERROR) << "Cannot display plane on overlay: error="
                     << strerror(errno);
          ret = false;
          flip.crtc->PageFlipFailed();
          break;
        }
      }
    }
  }
  plane_list->legacy_page_flips.clear();
  return ret;
}

bool HardwareDisplayPlaneManagerLegacy::SetPlaneData(
    HardwareDisplayPlaneList* plane_list,
    HardwareDisplayPlane* hw_plane,
    const OverlayPlane& overlay,
    uint32_t crtc_id,
    const gfx::Rect& src_rect,
    CrtcController* crtc) {
  plane_list->plane_list.push_back(hw_plane);
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
