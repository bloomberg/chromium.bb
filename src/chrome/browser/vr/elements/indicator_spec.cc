// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/elements/indicator_spec.h"
#include "chrome/browser/vr/ui_support.h"
#include "chrome/grit/generated_resources.h"

namespace vr {

IndicatorSpec::IndicatorSpec(UiElementName name,
                             UiElementName webvr_name,
                             const gfx::VectorIcon& icon,
                             int resource_string,
                             int background_resource_string,
                             int potential_resource_string,
                             CapturingStateModelMemberPtr signal,
                             bool is_url)
    : name(name),
      webvr_name(webvr_name),
      icon(icon),
      resource_string(resource_string),
      background_resource_string(background_resource_string),
      potential_resource_string(potential_resource_string),
      signal(signal),
      is_url(is_url) {}

IndicatorSpec::IndicatorSpec(const IndicatorSpec& other)
    : name(other.name),
      webvr_name(other.webvr_name),
      icon(other.icon),
      resource_string(other.resource_string),
      background_resource_string(other.background_resource_string),
      potential_resource_string(other.potential_resource_string),
      signal(other.signal),
      is_url(other.is_url) {}

IndicatorSpec::~IndicatorSpec() {}

// clang-format off
std::vector<IndicatorSpec> GetIndicatorSpecs() {

  std::vector<IndicatorSpec> specs = {
      {kLocationAccessIndicator, kWebVrLocationAccessIndicator,
       GetVrIcon(kVrMyLocationIcon),
       IDS_VR_SHELL_SITE_IS_TRACKING_LOCATION,
       // Background tabs cannot track high accuracy location.
       0,
       IDS_VR_SHELL_SITE_CAN_TRACK_LOCATION,
       &CapturingStateModel::location_access_enabled,
       false},

      {kAudioCaptureIndicator, kWebVrAudioCaptureIndicator,
       GetVrIcon(kVrMicIcon),
       IDS_VR_SHELL_SITE_IS_USING_MICROPHONE,
       IDS_VR_SHELL_BG_IS_USING_MICROPHONE,
       IDS_VR_SHELL_SITE_CAN_USE_MICROPHONE,
       &CapturingStateModel::audio_capture_enabled,
       false},

      {kVideoCaptureIndicator, kWebVrVideoCaptureIndicator,
       GetVrIcon(kVrVideocamIcon),
       IDS_VR_SHELL_SITE_IS_USING_CAMERA,
       IDS_VR_SHELL_BG_IS_USING_CAMERA,
       IDS_VR_SHELL_SITE_CAN_USE_CAMERA,
       &CapturingStateModel::video_capture_enabled,
       false},

      {kBluetoothConnectedIndicator, kWebVrBluetoothConnectedIndicator,
       GetVrIcon(kVrBluetoothConnectedIcon),
       IDS_VR_SHELL_SITE_IS_USING_BLUETOOTH,
       IDS_VR_SHELL_BG_IS_USING_BLUETOOTH,
       IDS_VR_SHELL_SITE_CAN_USE_BLUETOOTH,
       &CapturingStateModel::bluetooth_connected,
       false},

      {kScreenCaptureIndicator, kWebVrScreenCaptureIndicator,
       GetVrIcon(kVrScreenShareIcon),
       IDS_VR_SHELL_SITE_IS_SHARING_SCREEN,
       IDS_VR_SHELL_BG_IS_SHARING_SCREEN,
       IDS_VR_SHELL_SITE_CAN_SHARE_SCREEN,
       &CapturingStateModel::screen_capture_enabled,
       false}};

  return specs;
}
// clang-format on

}  // namespace vr
