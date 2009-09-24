// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SKIA_EXT_SKIA_UTILS_H_
#define SKIA_EXT_SKIA_UTILS_H_

#include "third_party/skia/include/core/SkColor.h"

class SkShader;

namespace skia {

// Creates a vertical gradient shader. The caller owns the shader.
// Example usage to avoid leaks:
//   paint.setShader(gfx::CreateGradientShader(0, 10, red, blue))->safeUnref();
//
// (The old shader in the paint, if any, needs to be freed, and safeUnref will
// handle the NULL case.)
SkShader* CreateGradientShader(int start_point,
                               int end_point,
                               SkColor start_color,
                               SkColor end_color);
}  // namespace skia

#endif  // SKIA_EXT_SKIA_UTILS_H_

