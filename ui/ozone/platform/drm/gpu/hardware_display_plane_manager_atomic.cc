// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/drm/gpu/hardware_display_plane_manager_atomic.h"

#include "base/bind.h"
#include "base/stl_util.h"
#include "ui/ozone/platform/drm/gpu/crtc_controller.h"
#include "ui/ozone/platform/drm/gpu/drm_device.h"
#include "ui/ozone/platform/drm/gpu/hardware_display_plane_atomic.h"
#include "ui/ozone/platform/drm/gpu/scanout_buffer.h"

namespace ui {

namespace {

void AtomicPageFlipCallback(std::vector<base::WeakPtr<CrtcController>> crtcs,
                            unsigned int frame,
                            base::TimeTicks timestamp) {
  for (auto& crtc : crtcs) {
    auto* crtc_ptr = crtc.get();
    if (crtc_ptr)
      crtc_ptr->OnPageFlipEvent(frame, timestamp);
  }
}

}  // namespace

HardwareDisplayPlaneManagerAtomic::HardwareDisplayPlaneManagerAtomic() {
}

HardwareDisplayPlaneManagerAtomic::~HardwareDisplayPlaneManagerAtomic() {
}

bool HardwareDisplayPlaneManagerAtomic::Commit(
    HardwareDisplayPlaneList* plane_list,
    bool test_only) {
  for (HardwareDisplayPlane* plane : plane_list->old_plane_list) {
    if (!base::ContainsValue(plane_list->plane_list, plane)) {
      // This plane is being released, so we need to zero it.
      plane->set_in_use(false);
      HardwareDisplayPlaneAtomic* atomic_plane =
          static_cast<HardwareDisplayPlaneAtomic*>(plane);
      atomic_plane->SetPlaneData(plane_list->atomic_property_set.get(), 0, 0,
                                 gfx::Rect(), gfx::Rect(),
                                 gfx::OVERLAY_TRANSFORM_NONE);
    }
  }

  std::vector<base::WeakPtr<CrtcController>> crtcs;
  for (HardwareDisplayPlane* plane : plane_list->plane_list) {
    HardwareDisplayPlaneAtomic* atomic_plane =
        static_cast<HardwareDisplayPlaneAtomic*>(plane);
    if (crtcs.empty() || crtcs.back().get() != atomic_plane->crtc())
      crtcs.push_back(atomic_plane->crtc()->AsWeakPtr());
  }

  if (test_only) {
    for (HardwareDisplayPlane* plane : plane_list->plane_list) {
      plane->set_in_use(false);
    }
  } else {
    plane_list->plane_list.swap(plane_list->old_plane_list);
  }

  uint32_t flags = 0;
  if (test_only) {
    flags = DRM_MODE_ATOMIC_TEST_ONLY;
  } else {
    flags = DRM_MODE_PAGE_FLIP_EVENT | DRM_MODE_ATOMIC_NONBLOCK;
  }

  if (!drm_->CommitProperties(plane_list->atomic_property_set.get(), flags,
                              crtcs.size(),
                              base::Bind(&AtomicPageFlipCallback, crtcs))) {
    if (!test_only) {
      PLOG(ERROR) << "Failed to commit properties for page flip.";
    } else {
      VPLOG(2) << "Failed to commit properties for MODE_ATOMIC_TEST_ONLY.";
    }

    ResetCurrentPlaneList(plane_list);
    return false;
  }

  plane_list->plane_list.clear();
  plane_list->atomic_property_set.reset(drmModeAtomicAlloc());
  return true;
}

bool HardwareDisplayPlaneManagerAtomic::DisableOverlayPlanes(
    HardwareDisplayPlaneList* plane_list) {
  for (HardwareDisplayPlane* plane : plane_list->old_plane_list) {
    if (plane->type() != HardwareDisplayPlane::kOverlay)
      continue;
    plane->set_in_use(false);
    plane->set_owning_crtc(0);

    HardwareDisplayPlaneAtomic* atomic_plane =
        static_cast<HardwareDisplayPlaneAtomic*>(plane);
    atomic_plane->SetPlaneData(plane_list->atomic_property_set.get(), 0, 0,
                               gfx::Rect(), gfx::Rect(),
                               gfx::OVERLAY_TRANSFORM_NONE);
  }
  // The list of crtcs is only useful if flags contains DRM_MODE_PAGE_FLIP_EVENT
  // to get the pageflip callback. In this case we don't need to be notified
  // at the next page flip, so the list of crtcs can be empty.
  std::vector<base::WeakPtr<CrtcController>> crtcs;
  bool ret = drm_->CommitProperties(plane_list->atomic_property_set.get(),
                                    DRM_MODE_ATOMIC_NONBLOCK, crtcs.size(),
                                    base::Bind(&AtomicPageFlipCallback, crtcs));
  PLOG_IF(ERROR, !ret) << "Failed to commit properties for page flip.";

  plane_list->atomic_property_set.reset(drmModeAtomicAlloc());
  return ret;
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
  uint32_t framebuffer_id = overlay.z_order
                                ? overlay.buffer->GetFramebufferId()
                                : overlay.buffer->GetOpaqueFramebufferId();
  if (!atomic_plane->SetPlaneData(
          plane_list->atomic_property_set.get(), crtc_id, framebuffer_id,
          overlay.display_bounds, src_rect, overlay.plane_transform)) {
    LOG(ERROR) << "Failed to set plane properties";
    return false;
  }
  atomic_plane->set_crtc(crtc);
  return true;
}

std::unique_ptr<HardwareDisplayPlane>
HardwareDisplayPlaneManagerAtomic::CreatePlane(uint32_t plane_id,
                                               uint32_t possible_crtcs) {
  return std::unique_ptr<HardwareDisplayPlane>(
      new HardwareDisplayPlaneAtomic(plane_id, possible_crtcs));
}

}  // namespace ui
