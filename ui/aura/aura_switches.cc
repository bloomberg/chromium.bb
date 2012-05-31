// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/aura_switches.h"

#include "base/command_line.h"

namespace switches {

// Enable the holding of mouse movements in order to throttle window resizing.
const char kAuraDisableHoldMouseMoves[] = "aura-disable-hold-mouse-moves";

// If set gesture events do not generate mouse events.
const char kAuraDisableMouseEventsFromTouch[] =
    "aura-disable-mouse-events-from-touch";

// Initial dimensions for the host window in the form "1024x768".
const char kAuraHostWindowSize[] = "aura-host-window-size";

// Whether to use the full screen for aura's host window.
const char kAuraHostWindowUseFullscreen[] = "aura-host-window-use-fullscreen";

}  // namespace switches
