// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/animation/CSSPaintInterpolationType.h"

#include <memory>
#include "core/CSSPropertyNames.h"
#include "core/animation/CSSColorInterpolationType.h"
#include "core/css/StyleColor.h"
#include "core/css/resolver/StyleResolverState.h"
#include "core/style/ComputedStyle.h"
#include "platform/wtf/PtrUtil.h"

namespace blink {

namespace {
static bool GetColorFromPaint(const SVGPaintType type,
                              const Color color,
                              StyleColor& result) {
  switch (type) {
    case SVG_PAINTTYPE_RGBCOLOR:
      result = color;
      return true;
    case SVG_PAINTTYPE_CURRENTCOLOR:
      result = StyleColor::CurrentColor();
      return true;
    default:
      return false;
  }
}

bool GetColor(CSSPropertyID property,
              const ComputedStyle& style,
              StyleColor& result) {
  switch (property) {
    case CSSPropertyFill:
      return GetColorFromPaint(style.SvgStyle().FillPaintType(),
                               style.SvgStyle().FillPaintColor(), result);
    case CSSPropertyStroke:
      return GetColorFromPaint(style.SvgStyle().StrokePaintType(),
                               style.SvgStyle().StrokePaintColor(), result);
    default:
      NOTREACHED();
      return false;
  }
}
}  // namespace

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
  if (!GetColor(CssProperty(), ComputedStyle::InitialStyle(), initial_color))
    return nullptr;
  return InterpolationValue(
      CSSColorInterpolationType::CreateInterpolableColor(initial_color));
}

class InheritedPaintChecker
    : public CSSInterpolationType::CSSConversionChecker {
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

  bool IsValid(const StyleResolverState& state,
               const InterpolationValue& underlying) const final {
    StyleColor parent_color;
    if (!GetColor(property_, *state.ParentStyle(), parent_color))
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
  if (!GetColor(CssProperty(), *state.ParentStyle(), parent_color)) {
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
  if (!GetColor(CssProperty(), style, underlying_color))
    return nullptr;
  return InterpolationValue(
      CSSColorInterpolationType::CreateInterpolableColor(underlying_color));
}

void CSSPaintInterpolationType::ApplyStandardPropertyValue(
    const InterpolableValue& interpolable_color,
    const NonInterpolableValue*,
    StyleResolverState& state) const {
  Color color = CSSColorInterpolationType::ResolveInterpolableColor(
      interpolable_color, state);
  switch (CssProperty()) {
    case CSSPropertyFill:
      state.Style()->AccessSVGStyle().SetFillPaint(SVG_PAINTTYPE_RGBCOLOR,
                                                   color, String(), true, true);
      break;
    case CSSPropertyStroke:
      state.Style()->AccessSVGStyle().SetStrokePaint(
          SVG_PAINTTYPE_RGBCOLOR, color, String(), true, true);
      break;
    default:
      NOTREACHED();
  }
}

}  // namespace blink
