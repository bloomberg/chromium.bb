// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/animation/SVGValueInterpolationType.h"

#include "core/animation/InterpolationEnvironment.h"
#include "core/animation/StringKeyframe.h"
#include "core/svg/properties/SVGAnimatedProperty.h"

namespace blink {

class SVGValueNonInterpolableValue : public NonInterpolableValue {
 public:
  virtual ~SVGValueNonInterpolableValue() {}

  static PassRefPtr<SVGValueNonInterpolableValue> Create(
      SVGPropertyBase* svg_value) {
    return AdoptRef(new SVGValueNonInterpolableValue(svg_value));
  }

  SVGPropertyBase* SvgValue() const { return svg_value_; }

  DECLARE_NON_INTERPOLABLE_VALUE_TYPE();

 private:
  SVGValueNonInterpolableValue(SVGPropertyBase* svg_value)
      : svg_value_(svg_value) {}

  Persistent<SVGPropertyBase> svg_value_;
};

DEFINE_NON_INTERPOLABLE_VALUE_TYPE(SVGValueNonInterpolableValue);
DEFINE_NON_INTERPOLABLE_VALUE_TYPE_CASTS(SVGValueNonInterpolableValue);

InterpolationValue SVGValueInterpolationType::MaybeConvertSVGValue(
    const SVGPropertyBase& value) const {
  SVGPropertyBase* referenced_value =
      const_cast<SVGPropertyBase*>(&value);  // Take ref.
  return InterpolationValue(
      InterpolableList::Create(0),
      SVGValueNonInterpolableValue::Create(referenced_value));
}

SVGPropertyBase* SVGValueInterpolationType::AppliedSVGValue(
    const InterpolableValue&,
    const NonInterpolableValue* non_interpolable_value) const {
  return ToSVGValueNonInterpolableValue(*non_interpolable_value).SvgValue();
}

}  // namespace blink
