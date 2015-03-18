// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/drm/gpu/hardware_display_plane_manager_atomic.h"

#include "base/bind.h"
#include "ui/ozone/platform/drm/gpu/drm_device.h"
#include "ui/ozone/platform/drm/gpu/hardware_display_plane_atomic.h"
#include "ui/ozone/platform/drm/gpu/scanout_buffer.h"

namespace ui {

static void AtomicPageFlipCallback(unsigned int /* frame */,
                                   unsigned int /* seconds */,
                                   unsigned int /* useconds */) {
}

HardwareDisplayPlaneManagerAtomic::HardwareDisplayPlaneManagerAtomic() {
}

HardwareDisplayPlaneManagerAtomic::~HardwareDisplayPlaneManagerAtomic() {
}

bool HardwareDisplayPlaneManagerAtomic::Commit(
    HardwareDisplayPlaneList* plane_list,
    bool is_sync) {
  for (HardwareDisplayPlane* plane : plane_list->old_plane_list) {
    bool found =
        std::find(plane_list->plane_list.begin(), plane_list->plane_list.end(),
                  plane) != plane_list->plane_list.end();
    if (!found) {
      // This plane is being released, so we need to zero it.
      plane->set_in_use(false);
      HardwareDisplayPlaneAtomic* atomic_plane =
          static_cast<HardwareDisplayPlaneAtomic*>(plane);
      atomic_plane->SetPlaneData(plane_list->atomic_property_set.get(), 0, 0,
                                 gfx::Rect(), gfx::Rect());
    }
  }

  plane_list->plane_list.swap(plane_list->old_plane_list);
  plane_list->plane_list.clear();
  if (!drm_->CommitProperties(plane_list->atomic_property_set.get(), 0,
                              base::Bind(&AtomicPageFlipCallback))) {
    PLOG(ERROR) << "Failed to commit properties";
    return false;
  }
  return true;
}

bool HardwareDisplayPlaneManagerAtomic::AssignOverlayPlanes(
    HardwareDisplayPlaneList* plane_list,
    const OverlayPlaneList& overlay_list,
    uint32_t crtc_id,
    CrtcController* crtc) {
  // Lazy-initialize the atomic property set.
  if (!plane_list->atomic_property_set)
    plane_list->atomic_property_set.reset(drmModePropertySetAlloc());
  return HardwareDisplayPlaneManager::AssignOverlayPlanes(
      plane_list, overlay_list, crtc_id, crtc);
}

bool HardwareDisplayPlaneManagerAtomic::SetPlaneData(
    HardwareDisplayPlaneList* plane_list,
    HardwareDisplayPlane* hw_plane,
    const OverlayPlane& overlay,
    uint32_t crtc_id,
    const gfx::Rect& src_rect,
    CrtcController* crtc) {
  HardwareDisplayPlaneAtomic* atomic_plane =
      static_cast<HardwareDisplayPlaneAtomic*>(hw_plane);
  if (!atomic_plane->SetPlaneData(plane_list->atomic_property_set.get(),
                                  crtc_id, overlay.buffer->GetFramebufferId(),
                                  overlay.display_bounds, src_rect)) {
    LOG(ERROR) << "Failed to set plane properties";
    return false;
  }
  plane_list->plane_list.push_back(hw_plane);
  return true;
}

scoped_ptr<HardwareDisplayPlane> HardwareDisplayPlaneManagerAtomic::CreatePlane(
    uint32_t plane_id,
    uint32_t possible_crtcs) {
  return scoped_ptr<HardwareDisplayPlane>(
      new HardwareDisplayPlaneAtomic(plane_id, possible_crtcs));
}

}  // namespace ui
