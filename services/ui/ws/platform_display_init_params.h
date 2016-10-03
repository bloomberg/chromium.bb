// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_UI_WS_PLATFORM_DISPLAY_INIT_PARAMS_H_
#define SERVICES_UI_WS_PLATFORM_DISPLAY_INIT_PARAMS_H_

#include <stdint.h>

#include "base/memory/ref_counted.h"
#include "services/ui/ws/viewport_metrics.h"

namespace ui {

class DisplayCompositor;
class GpuState;

namespace ws {

struct PlatformDisplayInitParams {
  PlatformDisplayInitParams();
  ~PlatformDisplayInitParams();

  scoped_refptr<DisplayCompositor> display_compositor;

  gfx::Rect display_bounds;
  int64_t display_id;
  ViewportMetrics metrics;
};

}  // namespace ws
}  // namespace ui

#endif  // SERVICES_UI_WS_PLATFORM_DISPLAY_INIT_PARAMS_H_
