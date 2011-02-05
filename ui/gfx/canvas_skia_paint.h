// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_CANVAS_SKIA_PAINT_H_
#define UI_GFX_CANVAS_SKIA_PAINT_H_
#pragma once

#include "skia/ext/canvas_paint.h"
#include "ui/gfx/canvas_skia.h"

// Define a gfx::CanvasSkiaPaint type that wraps our gfx::Canvas like the
// skia::PlatformCanvasPaint wraps PlatformCanvas.

namespace gfx {

typedef skia::CanvasPaintT<CanvasSkia> CanvasSkiaPaint;

}  // namespace gfx

#endif  // UI_GFX_CANVAS_SKIA_PAINT_H_
