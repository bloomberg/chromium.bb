// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/drm/gpu/hardware_display_plane_manager.h"

#include <drm.h>
#include <xf86drm.h>

#include <set>

#include "base/logging.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/ozone/platform/drm/gpu/crtc_controller.h"
#include "ui/ozone/platform/drm/gpu/drm_device.h"
#include "ui/ozone/platform/drm/gpu/hardware_display_controller.h"
#include "ui/ozone/platform/drm/gpu/scanout_buffer.h"
#include "ui/ozone/public/ozone_switches.h"

namespace ui {
namespace {

const float kFixedPointScaleValue = 65536.0f;

}  // namespace

HardwareDisplayPlaneList::HardwareDisplayPlaneList() : committed(false) {
}
HardwareDisplayPlaneList::~HardwareDisplayPlaneList() {
  for (auto* plane : plane_list) {
    plane->set_in_use(false);
    plane->set_owning_crtc(0);
  }
  for (auto* plane : old_plane_list) {
    plane->set_in_use(false);
    plane->set_owning_crtc(0);
  }
}

HardwareDisplayPlaneList::PageFlipInfo::PageFlipInfo(uint32_t crtc_id,
                                                     uint32_t framebuffer,
                                                     CrtcController* crtc)
    : crtc_id(crtc_id), framebuffer(framebuffer), crtc(crtc) {
}

HardwareDisplayPlaneList::PageFlipInfo::~PageFlipInfo() {
}

HardwareDisplayPlaneList::PageFlipInfo::Plane::Plane(int plane,
                                                     int framebuffer,
                                                     const gfx::Rect& bounds,
                                                     const gfx::Rect& src_rect)
    : plane(plane),
      framebuffer(framebuffer),
      bounds(bounds),
      src_rect(src_rect) {
}

HardwareDisplayPlaneList::PageFlipInfo::Plane::~Plane() {
}

HardwareDisplayPlaneManager::HardwareDisplayPlaneManager() : drm_(nullptr) {
}

HardwareDisplayPlaneManager::~HardwareDisplayPlaneManager() {
}

bool HardwareDisplayPlaneManager::Initialize(DrmDevice* drm) {
  drm_ = drm;
  ScopedDrmResourcesPtr resources(drmModeGetResources(drm->get_fd()));
  if (!resources) {
    PLOG(ERROR) << "Failed to get resources";
    return false;
  }

  ScopedDrmPlaneResPtr plane_resources(drmModeGetPlaneResources(drm->get_fd()));
  if (!plane_resources) {
    PLOG(ERROR) << "Failed to get plane resources";
    return false;
  }

  crtcs_.clear();
  for (int i = 0; i < resources->count_crtcs; ++i) {
    crtcs_.push_back(resources->crtcs[i]);
  }

  uint32_t num_planes = plane_resources->count_planes;
  std::set<uint32_t> plane_ids;
  for (uint32_t i = 0; i < num_planes; ++i) {
    ScopedDrmPlanePtr drm_plane(
        drmModeGetPlane(drm->get_fd(), plane_resources->planes[i]));
    if (!drm_plane) {
      PLOG(ERROR) << "Failed to get plane " << i;
      return false;
    }
    plane_ids.insert(drm_plane->plane_id);
    scoped_ptr<HardwareDisplayPlane> plane(
        new HardwareDisplayPlane(drm_plane.Pass()));
    if (plane->Initialize(drm))
      planes_.push_back(plane.release());
  }

  // crbug.com/464085: if driver reports no primary planes for a crtc, create a
  // dummy plane for which we can assign exactly one overlay.
  // TODO(dnicoara): refactor this to simplify AssignOverlayPlanes and move
  // this workaround into HardwareDisplayPlaneLegacy.
  for (int i = 0; i < resources->count_crtcs; ++i) {
    if (plane_ids.find(resources->crtcs[i] - 1) == plane_ids.end()) {
      scoped_ptr<HardwareDisplayPlane> dummy_plane(
          new HardwareDisplayPlane(0, (1 << i)));
      dummy_plane->set_is_dummy(true);
      if (dummy_plane->Initialize(drm))
        planes_.push_back(dummy_plane.release());
    }
  }

  std::sort(planes_.begin(), planes_.end(),
            [](HardwareDisplayPlane* l, HardwareDisplayPlane* r) {
              return l->plane_id() < r->plane_id();
            });
  return true;
}

HardwareDisplayPlane* HardwareDisplayPlaneManager::FindNextUnusedPlane(
    size_t* index,
    uint32_t crtc_index) {
  for (size_t i = *index; i < planes_.size(); ++i) {
    auto plane = planes_[i];
    if (!plane->in_use() && plane->CanUseForCrtc(crtc_index)) {
      *index = i + 1;
      return plane;
    }
  }
  return nullptr;
}

int HardwareDisplayPlaneManager::LookupCrtcIndex(uint32_t crtc_id) {
  for (size_t i = 0; i < crtcs_.size(); ++i)
    if (crtcs_[i] == crtc_id)
      return i;
  return -1;
}

bool HardwareDisplayPlaneManager::AssignOverlayPlanes(
    HardwareDisplayPlaneList* plane_list,
    const OverlayPlaneList& overlay_list,
    uint32_t crtc_id,
    CrtcController* crtc) {
  // If we had previously committed this set, mark all owned planes as free.
  if (plane_list->committed) {
    plane_list->committed = false;
    for (auto* plane : plane_list->old_plane_list) {
      plane->set_in_use(false);
    }
  }

  int crtc_index = LookupCrtcIndex(crtc_id);
  if (crtc_index < 0) {
    LOG(ERROR) << "Cannot find crtc " << crtc_id;
    return false;
  }

  size_t plane_idx = 0;
  for (const auto& plane : overlay_list) {
    HardwareDisplayPlane* hw_plane =
        FindNextUnusedPlane(&plane_idx, crtc_index);
    if (!hw_plane) {
      LOG(ERROR) << "Failed to find a free plane for crtc " << crtc_id;
      return false;
    }

    gfx::Rect fixed_point_rect;
    if (!hw_plane->is_dummy()) {
      const gfx::Size& size = plane.buffer->GetSize();
      gfx::RectF crop_rect = plane.crop_rect;
      crop_rect.Scale(size.width(), size.height());

      // This returns a number in 16.16 fixed point, required by the DRM overlay
      // APIs.
      auto to_fixed_point =
          [](double v) -> uint32_t { return v * kFixedPointScaleValue; };
      fixed_point_rect = gfx::Rect(to_fixed_point(crop_rect.x()),
                                   to_fixed_point(crop_rect.y()),
                                   to_fixed_point(crop_rect.width()),
                                   to_fixed_point(crop_rect.height()));
    }

    plane_list->plane_list.push_back(hw_plane);
    hw_plane->set_owning_crtc(crtc_id);
    if (SetPlaneData(plane_list, hw_plane, plane, crtc_id, fixed_point_rect,
                     crtc)) {
      hw_plane->set_in_use(true);
    } else {
      return false;
    }
  }
  return true;
}

void HardwareDisplayPlaneManager::ResetPlanes(
    HardwareDisplayPlaneList* plane_list,
    uint32_t crtc_id) {
  std::vector<HardwareDisplayPlane*> planes;
  planes.swap(plane_list->old_plane_list);
  for (auto* plane : planes) {
    if (plane->owning_crtc() == crtc_id) {
      plane->set_owning_crtc(0);
      plane->set_in_use(false);
    } else {
      plane_list->old_plane_list.push_back(plane);
    }
  }
}

}  // namespace ui
