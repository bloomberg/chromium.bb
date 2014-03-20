// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_DISPLAY_DISPLAY_CONSTANTS_H_
#define UI_DISPLAY_DISPLAY_CONSTANTS_H_

namespace ui {

// Used to describe the state of a multi-display configuration.
enum OutputState {
  OUTPUT_STATE_INVALID,
  OUTPUT_STATE_HEADLESS,
  OUTPUT_STATE_SINGLE,
  OUTPUT_STATE_DUAL_MIRROR,
  OUTPUT_STATE_DUAL_EXTENDED,
};

// Video output types.
enum OutputType {
  OUTPUT_TYPE_NONE = 0,
  OUTPUT_TYPE_UNKNOWN = 1 << 0,
  OUTPUT_TYPE_INTERNAL = 1 << 1,
  OUTPUT_TYPE_VGA = 1 << 2,
  OUTPUT_TYPE_HDMI = 1 << 3,
  OUTPUT_TYPE_DVI = 1 << 4,
  OUTPUT_TYPE_DISPLAYPORT = 1 << 5,
  OUTPUT_TYPE_NETWORK = 1 << 6,
};

// Content protection methods applied on video output.
enum OutputProtectionMethod {
  OUTPUT_PROTECTION_METHOD_NONE = 0,
  OUTPUT_PROTECTION_METHOD_HDCP = 1 << 0,
};

// HDCP protection state.
enum HDCPState { HDCP_STATE_UNDESIRED, HDCP_STATE_DESIRED, HDCP_STATE_ENABLED };

// Color calibration profiles.
enum ColorCalibrationProfile {
  COLOR_PROFILE_STANDARD,
  COLOR_PROFILE_DYNAMIC,
  COLOR_PROFILE_MOVIE,
  COLOR_PROFILE_READING,
};

}  // namespace ui

#endif  // UI_DISPLAY_DISPLAY_CONSTANTS_H_
