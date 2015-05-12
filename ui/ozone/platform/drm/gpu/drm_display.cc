// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/drm/gpu/drm_display.h"

#include <xf86drmMode.h>

#include "ui/display/types/gamma_ramp_rgb_entry.h"
#include "ui/ozone/platform/drm/common/drm_util.h"
#include "ui/ozone/platform/drm/gpu/drm_device.h"
#include "ui/ozone/platform/drm/gpu/screen_manager.h"

namespace ui {

namespace {

const char kContentProtection[] = "Content Protection";

struct ContentProtectionMapping {
  const char* name;
  HDCPState state;
};

const ContentProtectionMapping kContentProtectionStates[] = {
    {"Undesired", HDCP_STATE_UNDESIRED},
    {"Desired", HDCP_STATE_DESIRED},
    {"Enabled", HDCP_STATE_ENABLED}};

// Converts |state| to the DRM value associated with the it.
uint32_t GetContentProtectionValue(drmModePropertyRes* property,
                                   HDCPState state) {
  std::string name;
  for (size_t i = 0; i < arraysize(kContentProtectionStates); ++i) {
    if (kContentProtectionStates[i].state == state) {
      name = kContentProtectionStates[i].name;
      break;
    }
  }

  for (int i = 0; i < property->count_enums; ++i)
    if (name == property->enums[i].name)
      return i;

  NOTREACHED();
  return 0;
}

std::string GetEnumNameForProperty(drmModeConnector* connector,
                                   drmModePropertyRes* property) {
  for (int prop_idx = 0; prop_idx < connector->count_props; ++prop_idx) {
    if (connector->props[prop_idx] != property->prop_id)
      continue;

    for (int enum_idx = 0; enum_idx < property->count_enums; ++enum_idx) {
      const drm_mode_property_enum& property_enum = property->enums[enum_idx];
      if (property_enum.value == connector->prop_values[prop_idx])
        return property_enum.name;
    }
  }

  NOTREACHED();
  return std::string();
}

gfx::Size GetDrmModeSize(const drmModeModeInfo& mode) {
  return gfx::Size(mode.hdisplay, mode.vdisplay);
}

}  // namespace

DrmDisplay::DrmDisplay(ScreenManager* screen_manager,
                       int64_t display_id,
                       const scoped_refptr<DrmDevice>& drm,
                       uint32_t crtc,
                       uint32_t connector,
                       const std::vector<drmModeModeInfo>& modes)
    : screen_manager_(screen_manager),
      display_id_(display_id),
      drm_(drm),
      crtc_(crtc),
      connector_(connector),
      modes_(modes) {
}

DrmDisplay::~DrmDisplay() {
}

bool DrmDisplay::Configure(const drmModeModeInfo* mode,
                           const gfx::Point& origin) {
  VLOG(1) << "DRM configuring: device=" << drm_->device_path().value()
          << " crtc=" << crtc_ << " connector=" << connector_
          << " origin=" << origin.ToString()
          << " size=" << (mode ? GetDrmModeSize(*mode).ToString() : "0x0");

  if (mode) {
    if (!screen_manager_->ConfigureDisplayController(drm_, crtc_, connector_,
                                                     origin, *mode)) {
      VLOG(1) << "Failed to configure: device=" << drm_->device_path().value()
              << " crtc=" << crtc_ << " connector=" << connector_;
      return false;
    }
  } else {
    if (!screen_manager_->DisableDisplayController(drm_, crtc_)) {
      VLOG(1) << "Failed to disable device=" << drm_->device_path().value()
              << " crtc=" << crtc_;
      return false;
    }
  }

  return true;
}

bool DrmDisplay::GetHDCPState(HDCPState* state) {
  ScopedDrmConnectorPtr connector(drm_->GetConnector(connector_));
  if (!connector) {
    PLOG(ERROR) << "Failed to get connector " << connector_;
    return false;
  }

  ScopedDrmPropertyPtr hdcp_property(
      drm_->GetProperty(connector.get(), kContentProtection));
  if (!hdcp_property) {
    PLOG(ERROR) << "'" << kContentProtection << "' property doesn't exist.";
    return false;
  }

  std::string name =
      GetEnumNameForProperty(connector.get(), hdcp_property.get());
  for (size_t i = 0; i < arraysize(kContentProtectionStates); ++i) {
    if (name == kContentProtectionStates[i].name) {
      *state = kContentProtectionStates[i].state;
      VLOG(3) << "HDCP state: " << *state << " (" << name << ")";
      return true;
    }
  }

  LOG(ERROR) << "Unknown content protection value '" << name << "'";
  return false;
}

bool DrmDisplay::SetHDCPState(HDCPState state) {
  ScopedDrmConnectorPtr connector(drm_->GetConnector(connector_));
  if (!connector) {
    PLOG(ERROR) << "Failed to get connector " << connector_;
    return false;
  }

  ScopedDrmPropertyPtr hdcp_property(
      drm_->GetProperty(connector.get(), kContentProtection));
  if (!hdcp_property) {
    LOG(ERROR) << "'" << kContentProtection << "' property doesn't exist.";
    return false;
  }

  return drm_->SetProperty(
      connector_, hdcp_property->prop_id,
      GetContentProtectionValue(hdcp_property.get(), state));
}

void DrmDisplay::SetGammaRamp(const std::vector<GammaRampRGBEntry>& lut) {
  if (!drm_->SetGammaRamp(crtc_, lut)) {
    LOG(ERROR) << "Failed to set gamma ramp for display: crtc_id = " << crtc_
               << " size = " << lut.size();
  }
}

}  // namespace ui
