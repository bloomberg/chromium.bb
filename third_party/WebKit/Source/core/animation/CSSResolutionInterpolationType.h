// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSResolutionInterpolationType_h
#define CSSResolutionInterpolationType_h

#include "core/animation/CSSInterpolationType.h"

namespace blink {

class CSSResolutionInterpolationType : public CSSInterpolationType {
 public:
  CSSResolutionInterpolationType(
      PropertyHandle property,
      const PropertyRegistration* registration = nullptr)
      : CSSInterpolationType(property, registration) {
    DCHECK(property.IsCSSCustomProperty());
  }

  InterpolationValue MaybeConvertNeutral(const InterpolationValue& underlying,
                                         ConversionCheckers&) const final;
  InterpolationValue MaybeConvertValue(const CSSValue&,
                                       const StyleResolverState*,
                                       ConversionCheckers&) const final;

  const CSSValue* CreateCSSValue(const InterpolableValue&,
                                 const NonInterpolableValue*,
                                 const StyleResolverState&) const final;

 private:
  // These methods only apply to CSSInterpolationTypes used by standard CSS
  // properties.
  // CSSResolutionInterpolationType is only accessible via registered custom CSS
  // properties as there are no standard properties that interpolate resolution
  // values.
  InterpolationValue MaybeConvertStandardPropertyUnderlyingValue(
      const ComputedStyle&) const final {
    NOTREACHED();
    return nullptr;
  }
  void ApplyStandardPropertyValue(const InterpolableValue&,
                                  const NonInterpolableValue*,
                                  StyleResolverState&) const final {
    NOTREACHED();
  }
  InterpolationValue MaybeConvertInitial(const StyleResolverState&,
                                         ConversionCheckers&) const final {
    NOTREACHED();
    return nullptr;
  }
  InterpolationValue MaybeConvertInherit(const StyleResolverState&,
                                         ConversionCheckers&) const final {
    NOTREACHED();
    return nullptr;
  }
};

}  // namespace blink

#endif  // CSSResolutionInterpolationType_h
