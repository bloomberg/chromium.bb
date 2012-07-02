// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_SKIA_UTIL_H_
#define UI_GFX_SKIA_UTIL_H_
#pragma once

#include <string>
#include <vector>

#include "base/string16.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkRect.h"
#include "third_party/skia/include/core/SkShader.h"
#include "ui/base/ui_export.h"

class SkBitmap;
class SkDrawLooper;

namespace gfx {

class ImageSkiaRep;
class Rect;
class ShadowValue;

// Convert between Skia and gfx rect types.
UI_EXPORT SkRect RectToSkRect(const gfx::Rect& rect);
UI_EXPORT SkIRect RectToSkIRect(const gfx::Rect& rect);
UI_EXPORT gfx::Rect SkRectToRect(const SkRect& rect);

// Creates a bitmap shader for the image rep with the image rep's scale factor.
// Sets the created shader's local matrix such that it displays the image rep at
// the correct scale factor.
// The shader's local matrix should not be changed after the shader is created.
// TODO(pkotwicz): Allow shader's local matrix to be changed after the shader
// is created.
// Example usage to avoid leaks:
//   SkSafeUnref(paint.setShader(gfx::CreateImageRepShader(image_rep,
//       tile_mode, matrix)));
//
// (The old shader in the paint, if any, needs to be freed, and SkSafeUnref will
// handle the NULL case.)
UI_EXPORT SkShader* CreateImageRepShader(const gfx::ImageSkiaRep& image_rep,
                                         SkShader::TileMode tile_mode,
                                         const SkMatrix& local_matrix);

// Creates a vertical gradient shader. The caller owns the shader.
// Example usage to avoid leaks:
//   SkSafeUnref(paint.setShader(gfx::CreateGradientShader(0, 10, red, blue)));
//
// (The old shader in the paint, if any, needs to be freed, and SkSafeUnref will
// handle the NULL case.)
UI_EXPORT SkShader* CreateGradientShader(int start_point,
                                         int end_point,
                                         SkColor start_color,
                                         SkColor end_color);

// Creates a draw looper to generate |shadows|. The caller owns the draw looper.
// NULL is returned if |shadows| is empty since no draw looper is needed in
// this case.
// Example usage to avoid leaks:
//   SkSafeUnref(paint.setDrawLooper(gfx::CreateShadowDrawLooper(shadows)));
UI_EXPORT SkDrawLooper* CreateShadowDrawLooper(
    const std::vector<ShadowValue>& shadows);

// Returns true if the two bitmaps contain the same pixels.
UI_EXPORT bool BitmapsAreEqual(const SkBitmap& bitmap1,
                               const SkBitmap& bitmap2);

// Strip the accelerator char (typically '&') from a menu string.  A double
// accelerator char ('&&') will be converted to a single char.  The out params
// |accelerated_char_pos| and |accelerated_char_span| will be set to the index
// and span of the last accelerated character, respectively, or -1 and 0 if
// there was none.
UI_EXPORT string16 RemoveAcceleratorChar(const string16& s,
                                         char16 accelerator_char,
                                         int* accelerated_char_pos,
                                         int* accelerated_char_span);

// Converts Skia ARGB format pixels in |skia| to RGBA.
UI_EXPORT void ConvertSkiaToRGBA(const unsigned char* skia,
                                 int pixel_width,
                                 unsigned char* rgba);

}  // namespace gfx

#endif  // UI_GFX_SKIA_UTIL_H_
