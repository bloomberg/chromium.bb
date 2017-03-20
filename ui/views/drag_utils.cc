// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/drag_utils.h"

#include "ui/base/layout.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/widget/widget.h"

namespace views {

float ScaleFactorForDragFromWidget(const Widget* widget) {
  float device_scale = 1.0f;
  if (widget && widget->GetNativeView()) {
    gfx::NativeView view = widget->GetNativeView();
    device_scale = ui::GetScaleFactorForNativeView(view);
  }
  return device_scale;
}

gfx::Canvas* GetCanvasForDragImage(const Widget* widget,
                                   const gfx::Size& canvas_size) {
  float device_scale = ScaleFactorForDragFromWidget(widget);
  return new gfx::Canvas(canvas_size, device_scale, false);
}

}  // namespace views
