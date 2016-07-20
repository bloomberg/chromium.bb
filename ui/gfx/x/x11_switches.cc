// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"
#include "ui/gfx/x/x11_switches.h"

namespace switches {

#if !defined(OS_CHROMEOS)
// Color bit depth of the main window created in the browser process and matches
// XWindowAttributes.depth.
const char kWindowDepth[] = "window-depth";

// Which X11 display to connect to. Emulates the GTK+ "--display=" command line
// argument.
const char kX11Display[] = "display";

// A VisualID that supports the requested window depth.
const char kX11VisualID[] = "x11-visual-id";
#endif

}  // namespace switches
