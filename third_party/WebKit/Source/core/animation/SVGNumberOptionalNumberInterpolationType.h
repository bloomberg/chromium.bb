// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SVGNumberOptionalNumberInterpolationType_h
#define SVGNumberOptionalNumberInterpolationType_h

#include "core/animation/SVGInterpolationType.h"

namespace blink {

class SVGNumberOptionalNumberInterpolationType : public SVGInterpolationType {
 public:
  SVGNumberOptionalNumberInterpolationType(const QualifiedName& attribute)
      : SVGInterpolationType(attribute) {}

 private:
  InterpolationValue MaybeConvertNeutral(const InterpolationValue& underlying,
                                         ConversionCheckers&) const final;
  InterpolationValue MaybeConvertSVGValue(
      const SVGPropertyBase& svg_value) const final;
  SVGPropertyBase* AppliedSVGValue(const InterpolableValue&,
                                   const NonInterpolableValue*) const final;
};

}  // namespace blink

#endif  // SVGNumberOptionalNumberInterpolationType_h
