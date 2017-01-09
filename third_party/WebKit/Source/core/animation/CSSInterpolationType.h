// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSInterpolationType_h
#define CSSInterpolationType_h

#include "core/animation/InterpolationEnvironment.h"
#include "core/animation/InterpolationType.h"

namespace blink {

class CSSInterpolationType : public InterpolationType {
 protected:
  CSSInterpolationType(PropertyHandle);

  CSSPropertyID cssProperty() const { return getProperty().cssProperty(); }

  InterpolationValue maybeConvertSingle(const PropertySpecificKeyframe&,
                                        const InterpolationEnvironment&,
                                        const InterpolationValue& underlying,
                                        ConversionCheckers&) const final;
  virtual InterpolationValue maybeConvertNeutral(
      const InterpolationValue& underlying,
      ConversionCheckers&) const = 0;
  virtual InterpolationValue maybeConvertInitial(const StyleResolverState&,
                                                 ConversionCheckers&) const = 0;
  virtual InterpolationValue maybeConvertInherit(const StyleResolverState&,
                                                 ConversionCheckers&) const = 0;
  virtual InterpolationValue maybeConvertValue(const CSSValue&,
                                               const StyleResolverState&,
                                               ConversionCheckers&) const = 0;
  virtual void additiveKeyframeHook(InterpolationValue&) const {}

  InterpolationValue maybeConvertUnderlyingValue(
      const InterpolationEnvironment&) const final;
  virtual InterpolationValue maybeConvertStandardPropertyUnderlyingValue(
      const StyleResolverState&) const = 0;

  void apply(const InterpolableValue&,
             const NonInterpolableValue*,
             InterpolationEnvironment&) const final;
  virtual void applyStandardPropertyValue(const InterpolableValue&,
                                          const NonInterpolableValue*,
                                          StyleResolverState&) const = 0;

 private:
  InterpolationValue maybeConvertSingleInternal(
      const PropertySpecificKeyframe&,
      const InterpolationEnvironment&,
      const InterpolationValue& underlying,
      ConversionCheckers&) const;
};

}  // namespace blink

#endif  // CSSInterpolationType_h
