// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSPositionInterpolationType_h
#define CSSPositionInterpolationType_h

#include "core/animation/CSSLengthListInterpolationType.h"

#include "core/animation/CSSPositionAxisListInterpolationType.h"
#include "core/animation/ListInterpolationFunctions.h"
#include "core/css/CSSPrimitiveValue.h"
#include "core/css/CSSValuePair.h"

namespace blink {

class CSSPositionInterpolationType : public CSSLengthListInterpolationType {
 public:
  CSSPositionInterpolationType(PropertyHandle property)
      : CSSLengthListInterpolationType(property) {}

 private:
  InterpolationValue MaybeConvertValue(const CSSValue& value,
                                       const StyleResolverState*,
                                       ConversionCheckers&) const final {
    if (!value.IsValuePair()) {
      return nullptr;
    }
    const CSSValuePair& pair = ToCSSValuePair(value);
    return ListInterpolationFunctions::CreateList(2, [&pair](size_t index) {
      return CSSPositionAxisListInterpolationType::ConvertPositionAxisCSSValue(
          index == 0 ? pair.First() : pair.Second());
    });
  }
};

}  // namespace blink

#endif  // CSSPositionInterpolationType_h
