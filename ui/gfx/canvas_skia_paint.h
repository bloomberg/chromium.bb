// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_CANVAS_SKIA_PAINT_H_
#define UI_GFX_CANVAS_SKIA_PAINT_H_
#pragma once

#include "base/logging.h"
#include "skia/ext/canvas_paint.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/size.h"

// Define a gfx::CanvasSkiaPaint type that wraps our gfx::Canvas like the
// skia::PlatformCanvasPaint wraps PlatformCanvas.

namespace skia {

template<> inline
PlatformCanvas* GetPlatformCanvas(skia::CanvasPaintT<gfx::Canvas>* canvas) {
  PlatformCanvas* platform_canvas = canvas->platform_canvas();
  DCHECK(platform_canvas);
  return platform_canvas;
}

template<> inline
void RecreateBackingCanvas(skia::CanvasPaintT<gfx::Canvas>* canvas,
    int width, int height, float scale, bool opaque) {
  ui::ScaleFactor scale_factor = ui::GetScaleFactorFromScale(scale);
  canvas->RecreateBackingCanvas(gfx::Size(width, height), scale_factor,
      opaque);
}

}  // namespace skia

namespace gfx {

typedef skia::CanvasPaintT<Canvas> CanvasSkiaPaint;

}  // namespace gfx

#endif  // UI_GFX_CANVAS_SKIA_PAINT_H_
