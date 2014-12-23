// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/ui_base_switches_util.h"

#include "base/command_line.h"
#include "ui/base/ui_base_switches.h"

namespace switches {

bool IsLinkDisambiguationPopupEnabled() {
#if defined(OS_ANDROID)
  return true;
#else
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableLinkDisambiguationPopup)) {
    return true;
  }
  return false;
#endif
}

bool IsTextInputFocusManagerEnabled() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnableTextInputFocusManager);
}

bool IsTouchDragDropEnabled() {
#if defined(OS_CHROMEOS)
  return !base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kDisableTouchDragDrop);
#else
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnableTouchDragDrop);
#endif
}

bool IsTouchEditingEnabled() {
#if defined(USE_AURA)
  return !base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kDisableTouchEditing);
#else
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnableTouchEditing);
#endif
}

bool IsTouchFeedbackEnabled() {
  static bool touch_feedback_enabled =
      !base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableTouchFeedback);
  return touch_feedback_enabled;
}

}  // namespace switches
