// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SVGNumberInterpolationType_h
#define SVGNumberInterpolationType_h

#include "core/animation/SVGInterpolationType.h"
#include "core/svg_names.h"

namespace blink {

class SVGNumberInterpolationType : public SVGInterpolationType {
 public:
  SVGNumberInterpolationType(const QualifiedName& attribute)
      : SVGInterpolationType(attribute),
        is_non_negative_(attribute == SVGNames::pathLengthAttr) {}

 private:
  InterpolationValue MaybeConvertNeutral(const InterpolationValue& underlying,
                                         ConversionCheckers&) const final;
  InterpolationValue MaybeConvertSVGValue(
      const SVGPropertyBase& svg_value) const final;
  SVGPropertyBase* AppliedSVGValue(const InterpolableValue&,
                                   const NonInterpolableValue*) const final;

  bool is_non_negative_;
};

}  // namespace blink

#endif  // SVGNumberInterpolationType_h
