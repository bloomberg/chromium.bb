// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/animation/CSSValueInterpolationType.h"

#include "core/animation/InterpolationEnvironment.h"
#include "core/animation/StringKeyframe.h"
#include "core/css/CSSInheritedValue.h"
#include "core/css/CSSInitialValue.h"
#include "core/css/resolver/StyleBuilder.h"

namespace blink {

class CSSValueNonInterpolableValue : public NonInterpolableValue {
 public:
  ~CSSValueNonInterpolableValue() final {}

  static PassRefPtr<CSSValueNonInterpolableValue> Create(
      const CSSValue* css_value) {
    return AdoptRef(new CSSValueNonInterpolableValue(css_value));
  }

  const CSSValue* CssValue() const { return css_value_.Get(); }

  DECLARE_NON_INTERPOLABLE_VALUE_TYPE();

 private:
  CSSValueNonInterpolableValue(const CSSValue* css_value)
      : css_value_(css_value) {
    DCHECK(css_value_);
  }

  Persistent<const CSSValue> css_value_;
};

DEFINE_NON_INTERPOLABLE_VALUE_TYPE(CSSValueNonInterpolableValue);
DEFINE_NON_INTERPOLABLE_VALUE_TYPE_CASTS(CSSValueNonInterpolableValue);

InterpolationValue CSSValueInterpolationType::MaybeConvertInitial(
    const StyleResolverState& state,
    ConversionCheckers& conversion_checkers) const {
  return MaybeConvertValue(*CSSInitialValue::Create(), &state,
                           conversion_checkers);
}

InterpolationValue CSSValueInterpolationType::MaybeConvertInherit(
    const StyleResolverState& state,
    ConversionCheckers& conversion_checkers) const {
  return MaybeConvertValue(*CSSInheritedValue::Create(), &state,
                           conversion_checkers);
}

InterpolationValue CSSValueInterpolationType::MaybeConvertValue(
    const CSSValue& value,
    const StyleResolverState*,
    ConversionCheckers& conversion_checkers) const {
  return InterpolationValue(InterpolableList::Create(0),
                            CSSValueNonInterpolableValue::Create(&value));
}

void CSSValueInterpolationType::ApplyStandardPropertyValue(
    const InterpolableValue& interpolable_value,
    const NonInterpolableValue* non_interpolable_value,
    StyleResolverState& state) const {
  StyleBuilder::ApplyProperty(
      CssProperty(), state,
      *CreateCSSValue(interpolable_value, non_interpolable_value, state));
}

const CSSValue* CSSValueInterpolationType::CreateCSSValue(
    const InterpolableValue&,
    const NonInterpolableValue* non_interpolable_value,
    const StyleResolverState&) const {
  return ToCSSValueNonInterpolableValue(non_interpolable_value)->CssValue();
}

}  // namespace blink
