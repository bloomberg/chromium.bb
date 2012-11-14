// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/display_util.h"

#include "ui/aura/root_window_host.h"
#include "ui/gfx/display.h"
#include "base/logging.h"
#include "base/string_number_conversions.h"

namespace aura {
namespace {

bool use_fullscreen_host_window = false;

// Default bounds for a display.
const int kDefaultHostWindowX = 200;
const int kDefaultHostWindowY = 200;
const int kDefaultHostWindowWidth = 1280;
const int kDefaultHostWindowHeight = 1024;

}  // namespace

void SetUseFullscreenHostWindow(bool value) {
  use_fullscreen_host_window = value;
}

bool UseFullscreenHostWindow() {
  return use_fullscreen_host_window;
}

gfx::Display CreateDisplayFromSpec(const std::string& spec) {
  static int64 synthesized_display_id = 1000;
  gfx::Rect bounds(kDefaultHostWindowX, kDefaultHostWindowY,
                   kDefaultHostWindowWidth, kDefaultHostWindowHeight);
  int x = 0, y = 0, width, height;
  float scale = 1.0f;
  if (sscanf(spec.c_str(), "%dx%d*%f", &width, &height, &scale) >= 2 ||
      sscanf(spec.c_str(), "%d+%d-%dx%d*%f", &x, &y, &width, &height,
             &scale) >= 4) {
    bounds.SetRect(x, y, width, height);
  } else if (use_fullscreen_host_window) {
    bounds = gfx::Rect(aura::RootWindowHost::GetNativeScreenSize());
  }
  gfx::Display display(synthesized_display_id++);
  display.SetScaleAndBounds(scale, bounds);
  DVLOG(1) << "Display bounds=" << bounds.ToString() << ", scale=" << scale;
  return display;
}

}  // namespace aura
