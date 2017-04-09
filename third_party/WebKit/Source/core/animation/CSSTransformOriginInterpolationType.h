// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSTransformOriginInterpolationType_h
#define CSSTransformOriginInterpolationType_h

#include "core/animation/CSSLengthListInterpolationType.h"
#include "core/animation/CSSPositionAxisListInterpolationType.h"
#include "core/animation/LengthInterpolationFunctions.h"
#include "core/animation/ListInterpolationFunctions.h"
#include "core/css/CSSValueList.h"

namespace blink {

class CSSTransformOriginInterpolationType
    : public CSSLengthListInterpolationType {
 public:
  CSSTransformOriginInterpolationType(PropertyHandle property)
      : CSSLengthListInterpolationType(property) {}

 private:
  InterpolationValue MaybeConvertValue(const CSSValue& value,
                                       const StyleResolverState*,
                                       ConversionCheckers&) const final {
    const CSSValueList& list = ToCSSValueList(value);
    DCHECK_EQ(list.length(), 3U);
    return ListInterpolationFunctions::CreateList(
        list.length(), [&list](size_t index) {
          const CSSValue& item = list.Item(index);
          if (index < 2)
            return CSSPositionAxisListInterpolationType::
                ConvertPositionAxisCSSValue(item);
          return LengthInterpolationFunctions::MaybeConvertCSSValue(item);
        });
  }
};

}  // namespace blink

#endif  // CSSTransformOriginInterpolationType_h
