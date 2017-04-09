// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/animation/CSSPaintInterpolationType.h"

#include "core/animation/CSSColorInterpolationType.h"
#include "core/animation/PaintPropertyFunctions.h"
#include "core/css/resolver/StyleResolverState.h"
#include "wtf/PtrUtil.h"
#include <memory>

namespace blink {

InterpolationValue CSSPaintInterpolationType::MaybeConvertNeutral(
    const InterpolationValue&,
    ConversionCheckers&) const {
  return InterpolationValue(
      CSSColorInterpolationType::CreateInterpolableColor(Color::kTransparent));
}

InterpolationValue CSSPaintInterpolationType::MaybeConvertInitial(
    const StyleResolverState&,
    ConversionCheckers& conversion_checkers) const {
  StyleColor initial_color;
  if (!PaintPropertyFunctions::GetInitialColor(CssProperty(), initial_color))
    return nullptr;
  return InterpolationValue(
      CSSColorInterpolationType::CreateInterpolableColor(initial_color));
}

class InheritedPaintChecker : public InterpolationType::ConversionChecker {
 public:
  static std::unique_ptr<InheritedPaintChecker> Create(
      CSSPropertyID property,
      const StyleColor& color) {
    return WTF::WrapUnique(new InheritedPaintChecker(property, color));
  }
  static std::unique_ptr<InheritedPaintChecker> Create(CSSPropertyID property) {
    return WTF::WrapUnique(new InheritedPaintChecker(property));
  }

 private:
  InheritedPaintChecker(CSSPropertyID property)
      : property_(property), valid_color_(false) {}
  InheritedPaintChecker(CSSPropertyID property, const StyleColor& color)
      : property_(property), valid_color_(true), color_(color) {}

  bool IsValid(const InterpolationEnvironment& environment,
               const InterpolationValue& underlying) const final {
    StyleColor parent_color;
    if (!PaintPropertyFunctions::GetColor(
            property_, *environment.GetState().ParentStyle(), parent_color))
      return !valid_color_;
    return valid_color_ && parent_color == color_;
  }

  const CSSPropertyID property_;
  const bool valid_color_;
  const StyleColor color_;
};

InterpolationValue CSSPaintInterpolationType::MaybeConvertInherit(
    const StyleResolverState& state,
    ConversionCheckers& conversion_checkers) const {
  if (!state.ParentStyle())
    return nullptr;
  StyleColor parent_color;
  if (!PaintPropertyFunctions::GetColor(CssProperty(), *state.ParentStyle(),
                                        parent_color)) {
    conversion_checkers.push_back(InheritedPaintChecker::Create(CssProperty()));
    return nullptr;
  }
  conversion_checkers.push_back(
      InheritedPaintChecker::Create(CssProperty(), parent_color));
  return InterpolationValue(
      CSSColorInterpolationType::CreateInterpolableColor(parent_color));
}

InterpolationValue CSSPaintInterpolationType::MaybeConvertValue(
    const CSSValue& value,
    const StyleResolverState*,
    ConversionCheckers&) const {
  std::unique_ptr<InterpolableValue> interpolable_color =
      CSSColorInterpolationType::MaybeCreateInterpolableColor(value);
  if (!interpolable_color)
    return nullptr;
  return InterpolationValue(std::move(interpolable_color));
}

InterpolationValue
CSSPaintInterpolationType::MaybeConvertStandardPropertyUnderlyingValue(
    const ComputedStyle& style) const {
  // TODO(alancutter): Support capturing and animating with the visited paint
  // color.
  StyleColor underlying_color;
  if (!PaintPropertyFunctions::GetColor(CssProperty(), style, underlying_color))
    return nullptr;
  return InterpolationValue(
      CSSColorInterpolationType::CreateInterpolableColor(underlying_color));
}

void CSSPaintInterpolationType::ApplyStandardPropertyValue(
    const InterpolableValue& interpolable_color,
    const NonInterpolableValue*,
    StyleResolverState& state) const {
  PaintPropertyFunctions::SetColor(
      CssProperty(), *state.Style(),
      CSSColorInterpolationType::ResolveInterpolableColor(interpolable_color,
                                                          state));
}

}  // namespace blink
