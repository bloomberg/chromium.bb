// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/dri/hardware_display_plane.h"

#include <drm.h>
#include <errno.h>
#include <xf86drm.h>

#include "base/logging.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/ozone/platform/dri/dri_wrapper.h"

namespace ui {

namespace {
const char* kCrtcPropName = "CRTC_ID";
const char* kFbPropName = "FB_ID";
const char* kCrtcXPropName = "CRTC_X";
const char* kCrtcYPropName = "CRTC_Y";
const char* kCrtcWPropName = "CRTC_W";
const char* kCrtcHPropName = "CRTC_H";
const char* kSrcXPropName = "SRC_X";
const char* kSrcYPropName = "SRC_Y";
const char* kSrcWPropName = "SRC_W";
const char* kSrcHPropName = "SRC_H";
}

HardwareDisplayPlane::Property::Property() : id_(0) {
}

bool HardwareDisplayPlane::Property::Initialize(
    DriWrapper* drm,
    const char* name,
    const ScopedDrmObjectPropertyPtr& plane_props) {
  for (uint32_t i = 0; i < plane_props->count_props; i++) {
    ScopedDrmPropertyPtr property(
        drmModeGetProperty(drm->get_fd(), plane_props->props[i]));
    if (!strcmp(property->name, name)) {
      id_ = property->prop_id;
      break;
    }
  }
  if (!id_) {
    LOG(ERROR) << "Could not find property " << name;
    return false;
  }
  return true;
}

HardwareDisplayPlane::HardwareDisplayPlane(
    DriWrapper* drm,
    drmModePropertySetPtr atomic_property_set,
    ScopedDrmPlanePtr plane)
    : drm_(drm),
      property_set_(atomic_property_set),
      plane_(plane.Pass()),
      plane_id_(plane_->plane_id) {
}

HardwareDisplayPlane::~HardwareDisplayPlane() {
}

bool HardwareDisplayPlane::SetPlaneData(uint32_t crtc_id,
                                        uint32_t framebuffer,
                                        const gfx::Rect& crtc_rect,
                                        const gfx::Rect& src_rect) {
  int plane_set_error =
      drmModePropertySetAdd(
          property_set_, plane_id_, crtc_prop_.id_, crtc_id) ||
      drmModePropertySetAdd(
          property_set_, plane_id_, fb_prop_.id_, framebuffer) ||
      drmModePropertySetAdd(
          property_set_, plane_id_, crtc_x_prop_.id_, crtc_rect.x()) ||
      drmModePropertySetAdd(
          property_set_, plane_id_, crtc_y_prop_.id_, crtc_rect.y()) ||
      drmModePropertySetAdd(
          property_set_, plane_id_, crtc_w_prop_.id_, crtc_rect.width()) ||
      drmModePropertySetAdd(
          property_set_, plane_id_, crtc_h_prop_.id_, crtc_rect.height()) ||
      drmModePropertySetAdd(
          property_set_, plane_id_, src_x_prop_.id_, src_rect.x()) ||
      drmModePropertySetAdd(
          property_set_, plane_id_, src_y_prop_.id_, src_rect.x()) ||
      drmModePropertySetAdd(
          property_set_, plane_id_, src_w_prop_.id_, src_rect.width()) ||
      drmModePropertySetAdd(
          property_set_, plane_id_, src_h_prop_.id_, src_rect.height());

  if (plane_set_error) {
    LOG(ERROR) << "Failed to set plane data";
    return false;
  }
  return true;
}

bool HardwareDisplayPlane::Initialize() {
  ScopedDrmObjectPropertyPtr plane_props(drmModeObjectGetProperties(
      drm_->get_fd(), plane_id_, DRM_MODE_OBJECT_PLANE));

  if (!plane_props) {
    LOG(ERROR) << "Unable to get plane properties.";
    return false;
  }

  bool props_init =
      crtc_prop_.Initialize(drm_, kCrtcPropName, plane_props) &&
      fb_prop_.Initialize(drm_, kFbPropName, plane_props) &&
      crtc_x_prop_.Initialize(drm_, kCrtcXPropName, plane_props) &&
      crtc_y_prop_.Initialize(drm_, kCrtcYPropName, plane_props) &&
      crtc_w_prop_.Initialize(drm_, kCrtcWPropName, plane_props) &&
      crtc_h_prop_.Initialize(drm_, kCrtcHPropName, plane_props) &&
      src_x_prop_.Initialize(drm_, kSrcXPropName, plane_props) &&
      src_y_prop_.Initialize(drm_, kSrcYPropName, plane_props) &&
      src_w_prop_.Initialize(drm_, kSrcWPropName, plane_props) &&
      src_h_prop_.Initialize(drm_, kSrcHPropName, plane_props);

  if (!props_init) {
    LOG(ERROR) << "Unable to get plane properties.";
    return false;
  }
  return true;
}

}  // namespace ui
