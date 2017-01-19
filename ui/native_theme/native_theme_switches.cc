// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "ui/native_theme/native_theme_switches.h"

namespace switches {

// Enables or disables overlay scrollbars in Blink (i.e. web content) on Aura
// or Linux.  The status of native UI overlay scrollbars are determined in
// PlatformStyle::CreateScrollBar. Does nothing on Mac.
const char kEnableOverlayScrollbar[] = "enable-overlay-scrollbar";
const char kDisableOverlayScrollbar[] = "disable-overlay-scrollbar";

}  // namespace switches

namespace ui {

bool IsOverlayScrollbarEnabled() {
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();

  if (command_line.HasSwitch(switches::kDisableOverlayScrollbar))
    return false;

  return command_line.HasSwitch(switches::kEnableOverlayScrollbar);
}

}  // namespace ui
