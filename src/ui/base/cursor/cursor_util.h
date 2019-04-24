// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_CURSOR_CURSOR_UTIL_H_
#define UI_BASE_CURSOR_CURSOR_UTIL_H_

#include <vector>

#include "base/compiler_specific.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/ui_base_export.h"
#include "ui/display/display.h"
#include "ui/gfx/geometry/point.h"

namespace ui {

// Scale and rotate the cursor's bitmap and hotpoint.
// |bitmap_in_out| and |hotpoint_in_out| are used as
// both input and output.
UI_BASE_EXPORT void ScaleAndRotateCursorBitmapAndHotpoint(
    float scale,
    display::Display::Rotation rotation,
    SkBitmap* bitmap_in_out,
    gfx::Point* hotpoint_in_out);

// Helpers for CursorLoader.
void GetImageCursorBitmap(int resource_id,
                          float scale,
                          display::Display::Rotation rotation,
                          gfx::Point* hotspot,
                          SkBitmap* bitmap);
void GetAnimatedCursorBitmaps(int resource_id,
                              float scale,
                              display::Display::Rotation rotation,
                              gfx::Point* hotspot,
                              std::vector<SkBitmap>* bitmaps);

}  // namespace ui

#endif  // UI_BASE_CURSOR_CURSOR_UTIL_H_
