// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSAngleInterpolationType_h
#define CSSAngleInterpolationType_h

#include "core/animation/CSSInterpolationType.h"

namespace blink {

class CSSAngleInterpolationType : public CSSInterpolationType {
 public:
  CSSAngleInterpolationType(PropertyHandle property)
      : CSSInterpolationType(property) {
    DCHECK(property.isCSSCustomProperty());
  }

  InterpolationValue maybeConvertNeutral(const InterpolationValue& underlying,
                                         ConversionCheckers&) const final;
  InterpolationValue maybeConvertValue(const CSSValue&,
                                       const StyleResolverState*,
                                       ConversionCheckers&) const final;

  const CSSValue* createCSSValue(const InterpolableValue&,
                                 const NonInterpolableValue*,
                                 const StyleResolverState&) const final;

 private:
  // These methods only apply to CSSInterpolationTypes used by standard CSS
  // properties.
  // CSSAngleInterpolationType is only accessible via registered custom CSS
  // properties.
  InterpolationValue maybeConvertStandardPropertyUnderlyingValue(
      const ComputedStyle&) const final {
    NOTREACHED();
    return nullptr;
  }
  void applyStandardPropertyValue(const InterpolableValue&,
                                  const NonInterpolableValue*,
                                  StyleResolverState&) const final {
    NOTREACHED();
  }
  InterpolationValue maybeConvertInitial(const StyleResolverState&,
                                         ConversionCheckers&) const final {
    NOTREACHED();
    return nullptr;
  }
  InterpolationValue maybeConvertInherit(const StyleResolverState&,
                                         ConversionCheckers&) const final {
    NOTREACHED();
    return nullptr;
  }
};

}  // namespace blink

#endif  // CSSAngleInterpolationType_h
