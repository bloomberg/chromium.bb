// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef zoomAdjustedPixelValue_h
#define zoomAdjustedPixelValue_h

#include "core/css/CSSPrimitiveValue.h"
#include "core/style/ComputedStyle.h"

namespace blink {
class ComputedStyle;

inline CSSPrimitiveValue* ZoomAdjustedPixelValue(double value,
                                                 const ComputedStyle& style) {
  return CSSPrimitiveValue::Create(AdjustFloatForAbsoluteZoom(value, style),
                                   CSSPrimitiveValue::UnitType::kPixels);
}
}
#endif  // zoomAdjustedPixelValue_h
