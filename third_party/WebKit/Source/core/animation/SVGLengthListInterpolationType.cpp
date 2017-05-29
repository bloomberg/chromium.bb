// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/animation/SVGLengthListInterpolationType.h"

#include <memory>
#include "core/animation/SVGInterpolationEnvironment.h"
#include "core/animation/SVGLengthInterpolationType.h"
#include "core/animation/UnderlyingLengthChecker.h"
#include "core/svg/SVGLengthList.h"

namespace blink {

InterpolationValue SVGLengthListInterpolationType::MaybeConvertNeutral(
    const InterpolationValue& underlying,
    ConversionCheckers& conversion_checkers) const {
  size_t underlying_length =
      UnderlyingLengthChecker::GetUnderlyingLength(underlying);
  conversion_checkers.push_back(
      UnderlyingLengthChecker::Create(underlying_length));

  if (underlying_length == 0)
    return nullptr;

  std::unique_ptr<InterpolableList> result =
      InterpolableList::Create(underlying_length);
  for (size_t i = 0; i < underlying_length; i++)
    result->Set(i, SVGLengthInterpolationType::NeutralInterpolableValue());
  return InterpolationValue(std::move(result));
}

InterpolationValue SVGLengthListInterpolationType::MaybeConvertSVGValue(
    const SVGPropertyBase& svg_value) const {
  if (svg_value.GetType() != kAnimatedLengthList)
    return nullptr;

  const SVGLengthList& length_list = ToSVGLengthList(svg_value);
  std::unique_ptr<InterpolableList> result =
      InterpolableList::Create(length_list.length());
  for (size_t i = 0; i < length_list.length(); i++) {
    InterpolationValue component =
        SVGLengthInterpolationType::ConvertSVGLength(*length_list.at(i));
    result->Set(i, std::move(component.interpolable_value));
  }
  return InterpolationValue(std::move(result));
}

PairwiseInterpolationValue SVGLengthListInterpolationType::MaybeMergeSingles(
    InterpolationValue&& start,
    InterpolationValue&& end) const {
  size_t start_length = ToInterpolableList(*start.interpolable_value).length();
  size_t end_length = ToInterpolableList(*end.interpolable_value).length();
  if (start_length != end_length)
    return nullptr;
  return InterpolationType::MaybeMergeSingles(std::move(start), std::move(end));
}

void SVGLengthListInterpolationType::Composite(
    UnderlyingValueOwner& underlying_value_owner,
    double underlying_fraction,
    const InterpolationValue& value,
    double interpolation_fraction) const {
  size_t start_length =
      ToInterpolableList(*underlying_value_owner.Value().interpolable_value)
          .length();
  size_t end_length = ToInterpolableList(*value.interpolable_value).length();

  if (start_length == end_length)
    InterpolationType::Composite(underlying_value_owner, underlying_fraction,
                                 value, interpolation_fraction);
  else
    underlying_value_owner.Set(*this, value);
}

SVGPropertyBase* SVGLengthListInterpolationType::AppliedSVGValue(
    const InterpolableValue& interpolable_value,
    const NonInterpolableValue*) const {
  NOTREACHED();
  // This function is no longer called, because apply has been overridden.
  return nullptr;
}

void SVGLengthListInterpolationType::Apply(
    const InterpolableValue& interpolable_value,
    const NonInterpolableValue* non_interpolable_value,
    InterpolationEnvironment& environment) const {
  SVGElement& element = ToSVGInterpolationEnvironment(environment).SvgElement();
  SVGLengthContext length_context(&element);

  SVGLengthList* result = SVGLengthList::Create(unit_mode_);
  const InterpolableList& list = ToInterpolableList(interpolable_value);
  for (size_t i = 0; i < list.length(); i++) {
    result->Append(SVGLengthInterpolationType::ResolveInterpolableSVGLength(
        *list.Get(i), length_context, unit_mode_, negative_values_forbidden_));
  }

  element.SetWebAnimatedAttribute(Attribute(), result);
}

}  // namespace blink
