// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_RECT_CONVERSIONS_H_
#define UI_GFX_RECT_CONVERSIONS_H_

#include "ui/gfx/rect.h"
#include "ui/gfx/rect_f.h"

namespace gfx {

// Returns the smallest Rect that encloses the given RectF.
UI_EXPORT Rect ToEnclosingRect(const RectF& rect);

// Returns the largest Rect that is enclosed by the given Rect.
UI_EXPORT Rect ToEnclosedRect(const RectF& rect);

}  // namespace gfx

#endif  // UI_GFX_RECT_CONVERSIONS_H_
