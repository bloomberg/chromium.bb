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

  static PassRefPtr<CSSValueNonInterpolableValue> create(
      const CSSValue* cssValue) {
    return adoptRef(new CSSValueNonInterpolableValue(cssValue));
  }

  const CSSValue* cssValue() const { return m_cssValue.get(); }

  DECLARE_NON_INTERPOLABLE_VALUE_TYPE();

 private:
  CSSValueNonInterpolableValue(const CSSValue* cssValue)
      : m_cssValue(cssValue) {
    DCHECK(m_cssValue);
  }

  Persistent<const CSSValue> m_cssValue;
};

DEFINE_NON_INTERPOLABLE_VALUE_TYPE(CSSValueNonInterpolableValue);
DEFINE_NON_INTERPOLABLE_VALUE_TYPE_CASTS(CSSValueNonInterpolableValue);

InterpolationValue CSSValueInterpolationType::maybeConvertInitial(
    const StyleResolverState& state,
    ConversionCheckers& conversionCheckers) const {
  return maybeConvertValue(*CSSInitialValue::create(), &state,
                           conversionCheckers);
}

InterpolationValue CSSValueInterpolationType::maybeConvertInherit(
    const StyleResolverState& state,
    ConversionCheckers& conversionCheckers) const {
  return maybeConvertValue(*CSSInheritedValue::create(), &state,
                           conversionCheckers);
}

InterpolationValue CSSValueInterpolationType::maybeConvertValue(
    const CSSValue& value,
    const StyleResolverState*,
    ConversionCheckers& conversionCheckers) const {
  return InterpolationValue(InterpolableList::create(0),
                            CSSValueNonInterpolableValue::create(&value));
}

void CSSValueInterpolationType::applyStandardPropertyValue(
    const InterpolableValue& interpolableValue,
    const NonInterpolableValue* nonInterpolableValue,
    StyleResolverState& state) const {
  StyleBuilder::applyProperty(
      cssProperty(), state,
      *createCSSValue(interpolableValue, nonInterpolableValue, state));
}

const CSSValue* CSSValueInterpolationType::createCSSValue(
    const InterpolableValue&,
    const NonInterpolableValue* nonInterpolableValue,
    const StyleResolverState&) const {
  return toCSSValueNonInterpolableValue(nonInterpolableValue)->cssValue();
}

}  // namespace blink
