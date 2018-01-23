// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/drm/gpu/hardware_display_plane_manager.h"

#include <drm_fourcc.h>

#include <algorithm>
#include <set>
#include <utility>

#include "base/logging.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/ozone/platform/drm/gpu/drm_device.h"
#include "ui/ozone/platform/drm/gpu/scanout_buffer.h"

namespace ui {
namespace {

const float kFixedPointScaleValue = 65536.0f;

}  // namespace

HardwareDisplayPlaneList::HardwareDisplayPlaneList() {
  atomic_property_set.reset(drmModeAtomicAlloc());
}

HardwareDisplayPlaneList::~HardwareDisplayPlaneList() {
}

HardwareDisplayPlaneList::PageFlipInfo::PageFlipInfo(uint32_t crtc_id,
                                                     uint32_t framebuffer,
                                                     CrtcController* crtc)
    : crtc_id(crtc_id), framebuffer(framebuffer), crtc(crtc) {
}

HardwareDisplayPlaneList::PageFlipInfo::PageFlipInfo(
    const PageFlipInfo& other) = default;

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

  // Try to get all of the planes if possible, so we don't have to try to
  // discover hidden primary planes.
  bool has_universal_planes = false;
#if defined(DRM_CLIENT_CAP_UNIVERSAL_PLANES)
  has_universal_planes = drm->SetCapability(DRM_CLIENT_CAP_UNIVERSAL_PLANES, 1);
#endif  // defined(DRM_CLIENT_CAP_UNIVERSAL_PLANES)

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
    // TODO(hoegsberg) crbug.com/763760: We've rolled back the
    // downstream, incompatible drmModeGetPlane2 ioctl for now while
    // we update libdrm to the upstream per-plane IN_FORMATS property
    // API. This drops support for compressed and tiled framebuffers
    // in the interim, but once the buildroots and SDKs have pulled in
    // the new libdrm we'll add it back by reading the property.
    ScopedDrmPlanePtr drm_plane(
        drmModeGetPlane(drm->get_fd(), plane_resources->planes[i]));
    if (!drm_plane) {
      PLOG(ERROR) << "Failed to get plane " << i;
      return false;
    }

    ScopedDrmObjectPropertyPtr drm_plane_properties(drmModeObjectGetProperties(
        drm->get_fd(), plane_resources->planes[i], DRM_MODE_OBJECT_PLANE));

    std::vector<uint32_t> supported_formats;
    std::vector<drm_format_modifier> supported_format_modifiers;

    if (drm_plane_properties) {
      for (uint32_t j = 0; j < drm_plane_properties->count_props; j++) {
        ScopedDrmPropertyPtr property(
            drmModeGetProperty(drm->get_fd(), drm_plane_properties->props[j]));
        if (strcmp(property->name, "IN_FORMATS") == 0) {
          ScopedDrmPropertyBlobPtr blob(drmModeGetPropertyBlob(
              drm->get_fd(), drm_plane_properties->prop_values[j]));

          auto* data = static_cast<const uint8_t*>(blob->data);
          auto* header = reinterpret_cast<const drm_format_modifier_blob*>(data);
          auto* formats =
              reinterpret_cast<const uint32_t*>(data + header->formats_offset);
          auto* modifiers = reinterpret_cast<const drm_format_modifier*>(
              data + header->modifiers_offset);

          for (uint32_t k = 0; k < header->count_formats; k++)
            supported_formats.push_back(formats[k]);
          for (uint32_t k = 0; k < header->count_modifiers; k++)
            supported_format_modifiers.push_back(modifiers[k]);
        }
      }
    }

    if (supported_formats.empty()) {
      uint32_t formats_size = drm_plane->count_formats;
      for (uint32_t j = 0; j < formats_size; j++)
        supported_formats.push_back(drm_plane->formats[j]);
    }

    plane_ids.insert(drm_plane->plane_id);
    std::unique_ptr<HardwareDisplayPlane> plane(
        CreatePlane(drm_plane->plane_id, drm_plane->possible_crtcs));

    if (plane->Initialize(drm, supported_formats, supported_format_modifiers,
                          false, false)) {
      // CRTC controllers always assume they have a cursor plane and the cursor
      // plane is updated via cursor specific DRM API. Hence, we dont keep
      // track of Cursor plane here to avoid re-using it for any other purpose.
      if (plane->type() != HardwareDisplayPlane::kCursor)
        planes_.push_back(std::move(plane));
    }
  }

  // crbug.com/464085: if driver reports no primary planes for a crtc, create a
  // dummy plane for which we can assign exactly one overlay.
  // TODO(dnicoara): refactor this to simplify AssignOverlayPlanes and move
  // this workaround into HardwareDisplayPlaneLegacy.
  if (!has_universal_planes) {
    for (int i = 0; i < resources->count_crtcs; ++i) {
      if (plane_ids.find(resources->crtcs[i] - 1) == plane_ids.end()) {
        std::unique_ptr<HardwareDisplayPlane> dummy_plane(
            CreatePlane(resources->crtcs[i] - 1, (1 << i)));
        if (dummy_plane->Initialize(drm, std::vector<uint32_t>(),
                                    std::vector<drm_format_modifier>(), true,
                                    false)) {
          planes_.push_back(std::move(dummy_plane));
        }
      }
    }
  }

  std::sort(planes_.begin(), planes_.end(),
            [](const std::unique_ptr<HardwareDisplayPlane>& l,
               const std::unique_ptr<HardwareDisplayPlane>& r) {
              return l->plane_id() < r->plane_id();
            });

  PopulateSupportedFormats();
  return true;
}

std::unique_ptr<HardwareDisplayPlane> HardwareDisplayPlaneManager::CreatePlane(
    uint32_t plane_id,
    uint32_t possible_crtcs) {
  return std::unique_ptr<HardwareDisplayPlane>(
      new HardwareDisplayPlane(plane_id, possible_crtcs));
}

HardwareDisplayPlane* HardwareDisplayPlaneManager::FindNextUnusedPlane(
    size_t* index,
    uint32_t crtc_index,
    const OverlayPlane& overlay) const {
  for (size_t i = *index; i < planes_.size(); ++i) {
    auto* plane = planes_[i].get();
    if (!plane->in_use() && IsCompatible(plane, overlay, crtc_index)) {
      *index = i + 1;
      return plane;
    }
  }
  return nullptr;
}

int HardwareDisplayPlaneManager::LookupCrtcIndex(uint32_t crtc_id) const {
  for (size_t i = 0; i < crtcs_.size(); ++i)
    if (crtcs_[i] == crtc_id)
      return i;
  return -1;
}

bool HardwareDisplayPlaneManager::IsCompatible(HardwareDisplayPlane* plane,
                                               const OverlayPlane& overlay,
                                               uint32_t crtc_index) const {
  if (!plane->CanUseForCrtc(crtc_index))
    return false;

  if (!plane->IsSupportedFormat(overlay.buffer->GetFramebufferPixelFormat()))
    return false;

  // TODO(kalyank): We should check for z-order and any needed transformation
  // support. Driver doesn't expose any property to check for z-order, can we
  // rely on the sorting we do based on plane ids ?

  return true;
}

void HardwareDisplayPlaneManager::PopulateSupportedFormats() {
  std::set<uint32_t> supported_formats;

  for (const auto& plane : planes_) {
    const std::vector<uint32_t>& formats = plane->supported_formats();
    supported_formats.insert(formats.begin(), formats.end());
  }

  supported_formats_.reserve(supported_formats.size());
  supported_formats_.assign(supported_formats.begin(), supported_formats.end());
}

void HardwareDisplayPlaneManager::ResetCurrentPlaneList(
    HardwareDisplayPlaneList* plane_list) const {
  for (auto* hardware_plane : plane_list->plane_list) {
    hardware_plane->set_in_use(false);
    hardware_plane->set_owning_crtc(0);
  }

  plane_list->plane_list.clear();
  plane_list->legacy_page_flips.clear();
  plane_list->atomic_property_set.reset(drmModeAtomicAlloc());
}

void HardwareDisplayPlaneManager::BeginFrame(
    HardwareDisplayPlaneList* plane_list) {
  for (auto* plane : plane_list->old_plane_list) {
    plane->set_in_use(false);
  }
}

bool HardwareDisplayPlaneManager::AssignOverlayPlanes(
    HardwareDisplayPlaneList* plane_list,
    const OverlayPlaneList& overlay_list,
    uint32_t crtc_id,
    CrtcController* crtc) {
  int crtc_index = LookupCrtcIndex(crtc_id);
  if (crtc_index < 0) {
    LOG(ERROR) << "Cannot find crtc " << crtc_id;
    return false;
  }

  size_t plane_idx = 0;
  for (const auto& plane : overlay_list) {
    HardwareDisplayPlane* hw_plane =
        FindNextUnusedPlane(&plane_idx, crtc_index, plane);
    if (!hw_plane) {
      LOG(ERROR) << "Failed to find a free plane for crtc " << crtc_id;
      ResetCurrentPlaneList(plane_list);
      return false;
    }

    gfx::Rect fixed_point_rect;
    if (hw_plane->type() != HardwareDisplayPlane::kDummy) {
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

    if (!SetPlaneData(plane_list, hw_plane, plane, crtc_id, fixed_point_rect,
                      crtc)) {
      ResetCurrentPlaneList(plane_list);
      return false;
    }

    plane_list->plane_list.push_back(hw_plane);
    hw_plane->set_owning_crtc(crtc_id);
    hw_plane->set_in_use(true);
  }
  return true;
}

const std::vector<uint32_t>& HardwareDisplayPlaneManager::GetSupportedFormats()
    const {
  return supported_formats_;
}

std::vector<uint64_t> HardwareDisplayPlaneManager::GetFormatModifiers(
    uint32_t crtc_id,
    uint32_t format) {
  int crtc_index = LookupCrtcIndex(crtc_id);

  for (const auto& plane : planes_) {
    if (plane->CanUseForCrtc(crtc_index) &&
        plane->type() == HardwareDisplayPlane::kPrimary) {
      return plane->ModifiersForFormat(format);
    }
  }

  return std::vector<uint64_t>();
}

}  // namespace ui
