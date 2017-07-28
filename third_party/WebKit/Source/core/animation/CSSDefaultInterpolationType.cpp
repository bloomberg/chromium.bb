// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/animation/CSSDefaultInterpolationType.h"

#include "core/animation/CSSInterpolationEnvironment.h"
#include "core/animation/StringKeyframe.h"
#include "core/css/resolver/StyleBuilder.h"

namespace blink {

class CSSDefaultNonInterpolableValue : public NonInterpolableValue {
 public:
  ~CSSDefaultNonInterpolableValue() final {}

  static PassRefPtr<CSSDefaultNonInterpolableValue> Create(
      const CSSValue* css_value) {
    return AdoptRef(new CSSDefaultNonInterpolableValue(css_value));
  }

  const CSSValue* CssValue() const { return css_value_.Get(); }

  DECLARE_NON_INTERPOLABLE_VALUE_TYPE();

 private:
  CSSDefaultNonInterpolableValue(const CSSValue* css_value)
      : css_value_(css_value) {
    DCHECK(css_value_);
  }

  Persistent<const CSSValue> css_value_;
};

DEFINE_NON_INTERPOLABLE_VALUE_TYPE(CSSDefaultNonInterpolableValue);
DEFINE_NON_INTERPOLABLE_VALUE_TYPE_CASTS(CSSDefaultNonInterpolableValue);

InterpolationValue CSSDefaultInterpolationType::MaybeConvertSingle(
    const PropertySpecificKeyframe& keyframe,
    const InterpolationEnvironment&,
    const InterpolationValue&,
    ConversionCheckers&) const {
  return InterpolationValue(
      InterpolableList::Create(0),
      CSSDefaultNonInterpolableValue::Create(
          ToCSSPropertySpecificKeyframe(keyframe).Value()));
}

void CSSDefaultInterpolationType::Apply(
    const InterpolableValue&,
    const NonInterpolableValue* non_interpolable_value,
    InterpolationEnvironment& environment) const {
  StyleBuilder::ApplyProperty(
      GetProperty().CssProperty(),
      ToCSSInterpolationEnvironment(environment).GetState(),
      *ToCSSDefaultNonInterpolableValue(non_interpolable_value)->CssValue());
}

}  // namespace blink
