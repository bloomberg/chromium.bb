// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_TRANSFORM_UTIL_H_
#define UI_GFX_TRANSFORM_UTIL_H_

#include "ui/base/ui_export.h"
#include "ui/gfx/transform.h"

namespace gfx {

class Point;

// Returns a scale transform at |anchor| point.
UI_EXPORT Transform GetScaleTransform(const Point& anchor, float scale);

}  // namespace gfx

#endif  // UI_GFX_TRANSFORM_UTIL_H_
