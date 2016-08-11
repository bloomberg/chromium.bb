// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_UI_WS_PLATFORM_DISPLAY_INIT_PARAMS_H_
#define SERVICES_UI_WS_PLATFORM_DISPLAY_INIT_PARAMS_H_

#include <stdint.h>

#include "base/memory/ref_counted.h"
#include "ui/gfx/geometry/rect.h"

namespace display {
class PlatformScreen;
}

namespace shell {
class Connector;
}

namespace ui {

class GpuState;
class SurfacesState;

namespace ws {

struct PlatformDisplayInitParams {
  PlatformDisplayInitParams();
  PlatformDisplayInitParams(const PlatformDisplayInitParams& other);
  ~PlatformDisplayInitParams();

  scoped_refptr<SurfacesState> surfaces_state;

  gfx::Rect display_bounds;
  int64_t display_id;
  display::PlatformScreen* platform_screen = nullptr;
};

}  // namespace ws
}  // namespace ui

#endif  // SERVICES_UI_WS_PLATFORM_DISPLAY_INIT_PARAMS_H_
