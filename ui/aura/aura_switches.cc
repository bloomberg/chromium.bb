// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/aura_switches.h"

#include "base/command_line.h"

namespace switches {

// Initial dimensions for the host window in the form "1024x768".
const char kAuraHostWindowSize[] = "aura-host-window-size";

// Avoid drawing drop shadows under windows.
const char kAuraNoShadows[] = "aura-no-shadows";

// Use Aura-style translucent window frame.
const char kAuraTranslucentFrames[] = "aura-translucent-frames";

// Use a custom window style, e.g. --aura-window-mode=compact.
// When this flag is not passed we default to "normal" mode.
const char kAuraWindowMode[] = "aura-window-mode";

// Show only a single maximized window, like traditional non-Aura builds.
// Useful for low-resolution screens, such as on laptops.
const char kAuraWindowModeCompact[] = "compact";

// Default window management with multiple draggable windows.
const char kAuraWindowModeNormal[] = "normal";

// Use Aura-style workspace window dragging and sizing.
const char kAuraWorkspaceManager[] = "aura-workspace-manager";

bool IsAuraWindowModeCompact() {
  std::string mode = CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
      switches::kAuraWindowMode);
  return mode == switches::kAuraWindowModeCompact;
}

}  // namespace switches
