// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "ui/native_theme/native_theme_switches.h"

namespace switches {

// Enables overlay scrollbars on Aura or Linux. Does nothing on Mac.
const char kEnableOverlayScrollbar[] = "enable-overlay-scrollbar";

// Disables overlay scrollbars on Aura or Linux. Does nothing on Mac.
const char kDisableOverlayScrollbar[] = "disable-overlay-scrollbar";

// Hides scrollbars on Aura, Linux, Android, ChromeOS, MacOS (MacOS
// support is experimental).
const char kHideScrollbars[] = "hide-scrollbars";

}  // namespace switches

namespace ui {

bool IsOverlayScrollbarEnabled() {
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();

  // Hidden scrollbars are realized through never-showing overlay scrollbars.
  if (ShouldHideScrollbars())
    return true;

  if (command_line.HasSwitch(switches::kDisableOverlayScrollbar))
    return false;
  else if (command_line.HasSwitch(switches::kEnableOverlayScrollbar))
    return true;

  return false;
}

bool ShouldHideScrollbars() {
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();
  return command_line.HasSwitch(switches::kHideScrollbars);
}

}  // namespace ui
