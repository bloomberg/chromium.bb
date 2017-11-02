// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ZoomAdjustedPixelValue_h
#define ZoomAdjustedPixelValue_h

#include "core/css/CSSPrimitiveValue.h"
#include "core/layout/AdjustForAbsoluteZoom.h"

namespace blink {

class ComputedStyle;

inline CSSPrimitiveValue* ZoomAdjustedPixelValue(double value,
                                                 const ComputedStyle& style) {
  return CSSPrimitiveValue::Create(
      AdjustForAbsoluteZoom::AdjustFloat(value, style),
      CSSPrimitiveValue::UnitType::kPixels);
}

}  // namespace blink

#endif  // ZoomAdjustedPixelValue_h
