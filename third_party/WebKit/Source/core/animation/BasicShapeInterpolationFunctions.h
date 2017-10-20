// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BasicShapeInterpolationFunctions_h
#define BasicShapeInterpolationFunctions_h

#include "core/animation/InterpolationValue.h"
#include <memory>

namespace blink {

class BasicShape;
class CSSValue;
class CSSToLengthConversionData;

namespace BasicShapeInterpolationFunctions {

InterpolationValue MaybeConvertCSSValue(const CSSValue&);
InterpolationValue MaybeConvertBasicShape(const BasicShape*, double zoom);
std::unique_ptr<InterpolableValue> CreateNeutralValue(
    const NonInterpolableValue&);
bool ShapesAreCompatible(const NonInterpolableValue&,
                         const NonInterpolableValue&);
scoped_refptr<BasicShape> CreateBasicShape(const InterpolableValue&,
                                           const NonInterpolableValue&,
                                           const CSSToLengthConversionData&);

}  // namespace BasicShapeInterpolationFunctions

}  // namespace blink

#endif  // BasicShapeInterpolationFunctions_h
