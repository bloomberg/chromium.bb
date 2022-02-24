// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_DISPLAY_MODE_H_
#define CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_DISPLAY_MODE_H_

namespace arc {
namespace input_overlay {

// Display mode for display overlay.
enum class DisplayMode {
  kNone,
  // Display overlay can receive events but action labels can't be focused.
  // It shows educational dialog.
  kEducation,
  // Display overlay can't receive any events. It shows input mappings as in
  // view mode and menu anchor.
  kView,
  // Display overlay can receive events and action labels can be focused. It
  // shows input mapping in edit mode.
  kEdit,
  // Display overlay can still receive events. It is a mode after an action
  // label receives a valid key mapping.
  kEdited,
  // Display overlay can receive events but action labels can't be focused.
  // It shows expanded menu and input mapping as in view mode.
  kMenu,
};

}  // namespace input_overlay
}  // namespace arc

#endif  // CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_DISPLAY_MODE_H_
