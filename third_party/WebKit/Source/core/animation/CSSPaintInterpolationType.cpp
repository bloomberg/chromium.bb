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

InterpolationValue CSSPaintInterpolationType::maybeConvertNeutral(
    const InterpolationValue&,
    ConversionCheckers&) const {
  return InterpolationValue(
      CSSColorInterpolationType::createInterpolableColor(Color::transparent));
}

InterpolationValue CSSPaintInterpolationType::maybeConvertInitial(
    const StyleResolverState&,
    ConversionCheckers& conversionCheckers) const {
  StyleColor initialColor;
  if (!PaintPropertyFunctions::getInitialColor(cssProperty(), initialColor))
    return nullptr;
  return InterpolationValue(
      CSSColorInterpolationType::createInterpolableColor(initialColor));
}

class InheritedPaintChecker : public InterpolationType::ConversionChecker {
 public:
  static std::unique_ptr<InheritedPaintChecker> create(
      CSSPropertyID property,
      const StyleColor& color) {
    return WTF::wrapUnique(new InheritedPaintChecker(property, color));
  }
  static std::unique_ptr<InheritedPaintChecker> create(CSSPropertyID property) {
    return WTF::wrapUnique(new InheritedPaintChecker(property));
  }

 private:
  InheritedPaintChecker(CSSPropertyID property)
      : m_property(property), m_validColor(false) {}
  InheritedPaintChecker(CSSPropertyID property, const StyleColor& color)
      : m_property(property), m_validColor(true), m_color(color) {}

  bool isValid(const InterpolationEnvironment& environment,
               const InterpolationValue& underlying) const final {
    StyleColor parentColor;
    if (!PaintPropertyFunctions::getColor(
            m_property, *environment.state().parentStyle(), parentColor))
      return !m_validColor;
    return m_validColor && parentColor == m_color;
  }

  const CSSPropertyID m_property;
  const bool m_validColor;
  const StyleColor m_color;
};

InterpolationValue CSSPaintInterpolationType::maybeConvertInherit(
    const StyleResolverState& state,
    ConversionCheckers& conversionCheckers) const {
  if (!state.parentStyle())
    return nullptr;
  StyleColor parentColor;
  if (!PaintPropertyFunctions::getColor(cssProperty(), *state.parentStyle(),
                                        parentColor)) {
    conversionCheckers.push_back(InheritedPaintChecker::create(cssProperty()));
    return nullptr;
  }
  conversionCheckers.push_back(
      InheritedPaintChecker::create(cssProperty(), parentColor));
  return InterpolationValue(
      CSSColorInterpolationType::createInterpolableColor(parentColor));
}

InterpolationValue CSSPaintInterpolationType::maybeConvertValue(
    const CSSValue& value,
    const StyleResolverState*,
    ConversionCheckers&) const {
  std::unique_ptr<InterpolableValue> interpolableColor =
      CSSColorInterpolationType::maybeCreateInterpolableColor(value);
  if (!interpolableColor)
    return nullptr;
  return InterpolationValue(std::move(interpolableColor));
}

InterpolationValue
CSSPaintInterpolationType::maybeConvertStandardPropertyUnderlyingValue(
    const ComputedStyle& style) const {
  // TODO(alancutter): Support capturing and animating with the visited paint
  // color.
  StyleColor underlyingColor;
  if (!PaintPropertyFunctions::getColor(cssProperty(), style, underlyingColor))
    return nullptr;
  return InterpolationValue(
      CSSColorInterpolationType::createInterpolableColor(underlyingColor));
}

void CSSPaintInterpolationType::applyStandardPropertyValue(
    const InterpolableValue& interpolableColor,
    const NonInterpolableValue*,
    StyleResolverState& state) const {
  PaintPropertyFunctions::setColor(
      cssProperty(), *state.style(),
      CSSColorInterpolationType::resolveInterpolableColor(interpolableColor,
                                                          state));
}

}  // namespace blink
