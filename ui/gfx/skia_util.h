// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_SKIA_UTIL_H_
#define UI_GFX_SKIA_UTIL_H_
#pragma once

#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkRect.h"

class SkBitmap;
class SkShader;

namespace gfx {

class Rect;

// Convert between Skia and gfx rect types.
SkRect RectToSkRect(const gfx::Rect& rect);
gfx::Rect SkRectToRect(const SkRect& rect);

// Creates a vertical gradient shader. The caller owns the shader.
// Example usage to avoid leaks:
//   SkSafeUnref(paint.setShader(gfx::CreateGradientShader(0, 10, red, blue)));
//
// (The old shader in the paint, if any, needs to be freed, and SkSafeUnref will
// handle the NULL case.)
SkShader* CreateGradientShader(int start_point,
                               int end_point,
                               SkColor start_color,
                               SkColor end_color);

// Returns true if the two bitmaps contain the same pixels.
bool BitmapsAreEqual(const SkBitmap& bitmap1, const SkBitmap& bitmap2);

}  // namespace gfx;

#endif  // UI_GFX_SKIA_UTIL_H_
