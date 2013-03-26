// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/native_theme/native_theme.h"

#include "base/command_line.h"
#include "base/metrics/field_trial.h"
#include "ui/base/ui_base_switches.h"

// Constants for the new menu style field trial.
const char kNewMenuStyleFieldTrialName[] = "NewMenuStyle";
const char kNewMenuStyleFieldTrialGroupName[] = "OldStyle";

namespace ui {

void NativeTheme::SetScrollbarColors(unsigned inactive_color,
                                     unsigned active_color,
                                     unsigned track_color) {
  thumb_inactive_color_ = inactive_color;
  thumb_active_color_ = active_color;
  track_color_ = track_color;
}

// NativeTheme::instance() is implemented in the platform specific source files,
// such as native_theme_win.cc or native_theme_linux.cc

// static
bool NativeTheme::IsNewMenuStyleEnabled() {
#if defined(USE_AURA)
  return true;
#else
  static bool enable_new_menu_style =
      !CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableNewMenuStyle);
  // Run experiment only if there is no kDisableNewMenuStyle flag.
  if (enable_new_menu_style) {
    enable_new_menu_style =
        base::FieldTrialList::FindFullName(kNewMenuStyleFieldTrialName) !=
        kNewMenuStyleFieldTrialGroupName;
  }
  return enable_new_menu_style;
#endif
}

NativeTheme::NativeTheme()
    : thumb_inactive_color_(0xeaeaea),
      thumb_active_color_(0xf4f4f4),
      track_color_(0xd3d3d3) {
}

NativeTheme::~NativeTheme() {}

}  // namespace ui
