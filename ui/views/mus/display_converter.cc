// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/mus/display_converter.h"

#include <stdint.h>

#include "components/mus/public/cpp/window.h"
#include "mojo/converters/geometry/geometry_type_converters.h"

namespace views {

std::vector<gfx::Display> GetDisplaysFromWindow(mus::Window* window) {
  static int64_t synthesized_display_id = 2000;
  gfx::Display display;
  display.set_id(synthesized_display_id++);
  display.SetScaleAndBounds(
      window->viewport_metrics().device_pixel_ratio,
      gfx::Rect(window->viewport_metrics().size_in_pixels.To<gfx::Size>()));
  std::vector<gfx::Display> displays;
  displays.push_back(display);
  return displays;
}

}  // namespace views
