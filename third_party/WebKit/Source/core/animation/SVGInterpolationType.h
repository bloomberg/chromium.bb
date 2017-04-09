// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SVGInterpolationType_h
#define SVGInterpolationType_h

#include "core/animation/InterpolationType.h"

namespace blink {

class SVGPropertyBase;

class SVGInterpolationType : public InterpolationType {
 protected:
  SVGInterpolationType(const QualifiedName& attribute)
      : InterpolationType(PropertyHandle(attribute)) {}

  const QualifiedName& Attribute() const {
    return GetProperty().SvgAttribute();
  }

  virtual InterpolationValue MaybeConvertNeutral(
      const InterpolationValue& underlying,
      ConversionCheckers&) const = 0;
  virtual InterpolationValue MaybeConvertSVGValue(
      const SVGPropertyBase&) const = 0;
  virtual SVGPropertyBase* AppliedSVGValue(
      const InterpolableValue&,
      const NonInterpolableValue*) const = 0;

  InterpolationValue MaybeConvertSingle(const PropertySpecificKeyframe&,
                                        const InterpolationEnvironment&,
                                        const InterpolationValue& underlying,
                                        ConversionCheckers&) const override;
  InterpolationValue MaybeConvertUnderlyingValue(
      const InterpolationEnvironment&) const override;
  void Apply(const InterpolableValue&,
             const NonInterpolableValue*,
             InterpolationEnvironment&) const override;
};

}  // namespace blink

#endif  // SVGInterpolationType_h
