// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/drm/gpu/drm_display_snapshot.h"

#include <stdint.h>
#include <stdlib.h>
#include <xf86drmMode.h>

#include "base/format_macros.h"
#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "ui/display/util/edid_parser.h"
#include "ui/ozone/platform/drm/gpu/drm_device.h"
#include "ui/ozone/platform/drm/gpu/drm_display_mode.h"
#include "ui/ozone/platform/drm/gpu/drm_util.h"

#if !defined(DRM_MODE_CONNECTOR_DSI)
#define DRM_MODE_CONNECTOR_DSI 16
#endif

namespace ui {

namespace {

DisplayConnectionType GetDisplayType(drmModeConnector* connector) {
  switch (connector->connector_type) {
    case DRM_MODE_CONNECTOR_VGA:
      return DISPLAY_CONNECTION_TYPE_VGA;
    case DRM_MODE_CONNECTOR_DVII:
    case DRM_MODE_CONNECTOR_DVID:
    case DRM_MODE_CONNECTOR_DVIA:
      return DISPLAY_CONNECTION_TYPE_DVI;
    case DRM_MODE_CONNECTOR_LVDS:
    case DRM_MODE_CONNECTOR_eDP:
    case DRM_MODE_CONNECTOR_DSI:
      return DISPLAY_CONNECTION_TYPE_INTERNAL;
    case DRM_MODE_CONNECTOR_DisplayPort:
      return DISPLAY_CONNECTION_TYPE_DISPLAYPORT;
    case DRM_MODE_CONNECTOR_HDMIA:
    case DRM_MODE_CONNECTOR_HDMIB:
      return DISPLAY_CONNECTION_TYPE_HDMI;
    default:
      return DISPLAY_CONNECTION_TYPE_UNKNOWN;
  }
}

bool IsAspectPreserving(DrmDevice* drm, drmModeConnector* connector) {
  ScopedDrmPropertyPtr property(drm->GetProperty(connector, "scaling mode"));
  if (!property)
    return false;

  for (int props_i = 0; props_i < connector->count_props; ++props_i) {
    if (connector->props[props_i] != property->prop_id)
      continue;

    for (int enums_i = 0; enums_i < property->count_enums; ++enums_i) {
      if (property->enums[enums_i].value == connector->prop_values[props_i] &&
          strcmp(property->enums[enums_i].name, "Full aspect") == 0)
        return true;
    }
  }

  return false;
}

}  // namespace

DrmDisplaySnapshot::DrmDisplaySnapshot(const scoped_refptr<DrmDevice>& drm,
                                       drmModeConnector* connector,
                                       drmModeCrtc* crtc,
                                       uint32_t index)
    : DisplaySnapshot(index,
                      gfx::Point(crtc->x, crtc->y),
                      gfx::Size(connector->mmWidth, connector->mmHeight),
                      GetDisplayType(connector),
                      IsAspectPreserving(drm.get(), connector),
                      false,
                      std::string(),
                      std::vector<const DisplayMode*>(),
                      nullptr,
                      nullptr),
      drm_(drm),
      connector_(connector->connector_id),
      crtc_(crtc->crtc_id),
      dpms_property_(drm->GetProperty(connector, "DPMS")) {
  if (!dpms_property_)
    VLOG(1) << "Failed to find the DPMS property for connector "
            << connector->connector_id;

  ScopedDrmPropertyBlobPtr edid_blob(drm->GetPropertyBlob(connector, "EDID"));

  if (edid_blob) {
    std::vector<uint8_t> edid(
        static_cast<uint8_t*>(edid_blob->data),
        static_cast<uint8_t*>(edid_blob->data) + edid_blob->length);

    if (!GetDisplayIdFromEDID(edid, index, &display_id_))
      display_id_ = index;

    ParseOutputDeviceData(edid, nullptr, &display_name_, nullptr, nullptr);
    ParseOutputOverscanFlag(edid, &overscan_flag_);
  } else {
    VLOG(1) << "Failed to get EDID blob for connector "
            << connector->connector_id;
  }

  for (int i = 0; i < connector->count_modes; ++i) {
    drmModeModeInfo& mode = connector->modes[i];
    modes_.push_back(new DrmDisplayMode(mode));

    if (crtc->mode_valid && SameMode(crtc->mode, mode))
      current_mode_ = modes_.back();

    if (mode.type & DRM_MODE_TYPE_PREFERRED)
      native_mode_ = modes_.back();
  }

  // If no preferred mode is found then use the first one. Using the first one
  // since it should be the best mode.
  if (!native_mode_ && !modes_.empty())
    native_mode_ = modes_.front();
}

DrmDisplaySnapshot::~DrmDisplaySnapshot() {
}

std::string DrmDisplaySnapshot::ToString() const {
  return base::StringPrintf(
      "[type=%d, connector=%" PRIu32 ", crtc=%" PRIu32
      ", origin=%s, mode=%s, dim=%s]",
      type_, connector_, crtc_, origin_.ToString().c_str(),
      current_mode_ ? current_mode_->ToString().c_str() : "NULL",
      physical_size_.ToString().c_str());
}

}  // namespace ui
