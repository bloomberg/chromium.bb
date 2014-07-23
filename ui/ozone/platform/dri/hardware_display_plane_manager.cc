// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/dri/hardware_display_plane_manager.h"

#include <drm.h>
#include <errno.h>
#include <xf86drm.h>

#include "base/logging.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/ozone/platform/dri/dri_wrapper.h"

namespace ui {

HardwareDisplayPlaneManager::HardwareDisplayPlaneManager(DriWrapper* drm)
    : drm_(drm), property_set_(NULL) {
}

HardwareDisplayPlaneManager::~HardwareDisplayPlaneManager() {
  if (property_set_)
    drmModePropertySetFree(property_set_);
}

bool HardwareDisplayPlaneManager::Initialize() {
  ScopedDrmPlaneResPtr plane_resources(
      drmModeGetPlaneResources(drm_->get_fd()));
  if (!plane_resources) {
    LOG(ERROR) << "Failed to get plane resources.";
    return false;
  }

  property_set_ = drmModePropertySetAlloc();
  if (!property_set_) {
    LOG(ERROR) << "Failed to allocate property set.";
    return false;
  }

  uint32_t num_planes = plane_resources->count_planes;
  for (uint32_t i = 0; i < num_planes; ++i) {
    ScopedDrmPlanePtr drm_plane(
        drmModeGetPlane(drm_->get_fd(), plane_resources->planes[i]));
    if (!drm_plane) {
      LOG(ERROR) << "Failed to get plane " << i << ".";
      return false;
    }
    scoped_ptr<HardwareDisplayPlane> plane(
        new HardwareDisplayPlane(drm_, property_set_, drm_plane.Pass()));
    if (plane->Initialize())
      planes_.push_back(plane.release());
  }

  return true;
}

}  // namespace ui
