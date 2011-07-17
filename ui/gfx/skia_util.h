// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_SKIA_UTIL_H_
#define UI_GFX_SKIA_UTIL_H_
#pragma once

#include <string>

#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkRect.h"
#include "ui/ui_api.h"

class SkBitmap;
class SkShader;

namespace gfx {

class Rect;

// Convert between Skia and gfx rect types.
UI_API SkRect RectToSkRect(const gfx::Rect& rect);
UI_API gfx::Rect SkRectToRect(const SkRect& rect);

// Creates a vertical gradient shader. The caller owns the shader.
// Example usage to avoid leaks:
//   SkSafeUnref(paint.setShader(gfx::CreateGradientShader(0, 10, red, blue)));
//
// (The old shader in the paint, if any, needs to be freed, and SkSafeUnref will
// handle the NULL case.)
UI_API SkShader* CreateGradientShader(int start_point,
                                      int end_point,
                                      SkColor start_color,
                                      SkColor end_color);

// Returns true if the two bitmaps contain the same pixels.
UI_API bool BitmapsAreEqual(const SkBitmap& bitmap1, const SkBitmap& bitmap2);

// Strip the accelerator char (typically '&') from a menu string.  A
// double accelerator char ('&&') will be converted to a single char.
UI_API std::string RemoveAcceleratorChar(const std::string& s,
                                         char accelerator_char);

}  // namespace gfx;

#endif  // UI_GFX_SKIA_UTIL_H_
