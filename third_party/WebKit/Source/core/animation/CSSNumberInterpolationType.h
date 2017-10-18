// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSNumberInterpolationType_h
#define CSSNumberInterpolationType_h

#include "core/CoreExport.h"
#include "core/animation/CSSInterpolationType.h"

namespace blink {

class CORE_EXPORT CSSNumberInterpolationType : public CSSInterpolationType {
 public:
  CSSNumberInterpolationType(PropertyHandle property,
                             const PropertyRegistration* registration = nullptr,
                             bool round_to_integer = false)
      : CSSInterpolationType(property, registration),
        round_to_integer_(round_to_integer) {
    // This integer flag only applies to registered custom properties.
    DCHECK(!round_to_integer_ || property.IsCSSCustomProperty());
  }

  InterpolationValue MaybeConvertStandardPropertyUnderlyingValue(
      const ComputedStyle&) const final;
  void ApplyStandardPropertyValue(const InterpolableValue&,
                                  const NonInterpolableValue*,
                                  StyleResolverState&) const final;

  const CSSValue* CreateCSSValue(const InterpolableValue&,
                                 const NonInterpolableValue*,
                                 const StyleResolverState&) const final;

 private:
  InterpolationValue CreateNumberValue(double number) const;
  InterpolationValue MaybeConvertNeutral(const InterpolationValue& underlying,
                                         ConversionCheckers&) const final;
  InterpolationValue MaybeConvertInitial(const StyleResolverState&,
                                         ConversionCheckers&) const final;
  InterpolationValue MaybeConvertInherit(const StyleResolverState&,
                                         ConversionCheckers&) const final;
  InterpolationValue MaybeConvertValue(const CSSValue&,
                                       const StyleResolverState*,
                                       ConversionCheckers&) const final;

  const bool round_to_integer_;
};

}  // namespace blink

#endif  // CSSNumberInterpolationType_h
