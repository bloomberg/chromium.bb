// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_UI_WS_VIEWPORT_METRICS_H_
#define SERVICES_UI_WS_VIEWPORT_METRICS_H_

#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

namespace ui {
namespace ws {

struct ViewportMetrics {
  gfx::Rect bounds;  // DIP.
  gfx::Size pixel_size;
  float device_scale_factor = 0.0f;
};

}  // namespace ws
}  // namespace ui

#endif  // SERVICES_UI_WS_VIEWPORT_METRICS_H_
