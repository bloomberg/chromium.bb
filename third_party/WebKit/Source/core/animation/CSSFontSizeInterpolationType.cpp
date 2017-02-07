// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/animation/CSSFontSizeInterpolationType.h"

#include "core/animation/LengthInterpolationFunctions.h"
#include "core/css/CSSIdentifierValue.h"
#include "core/css/resolver/StyleResolverState.h"
#include "platform/LengthFunctions.h"
#include "platform/fonts/FontDescription.h"
#include "wtf/PtrUtil.h"
#include <memory>

namespace blink {

namespace {

class IsMonospaceChecker : public InterpolationType::ConversionChecker {
 public:
  static std::unique_ptr<IsMonospaceChecker> create(bool isMonospace) {
    return WTF::wrapUnique(new IsMonospaceChecker(isMonospace));
  }

 private:
  IsMonospaceChecker(bool isMonospace) : m_isMonospace(isMonospace) {}

  bool isValid(const InterpolationEnvironment& environment,
               const InterpolationValue&) const final {
    return m_isMonospace ==
           environment.state().style()->getFontDescription().isMonospace();
  }

  const bool m_isMonospace;
};

class InheritedFontSizeChecker : public InterpolationType::ConversionChecker {
 public:
  static std::unique_ptr<InheritedFontSizeChecker> create(
      const FontDescription::Size& inheritedFontSize) {
    return WTF::wrapUnique(new InheritedFontSizeChecker(inheritedFontSize));
  }

 private:
  InheritedFontSizeChecker(const FontDescription::Size& inheritedFontSize)
      : m_inheritedFontSize(inheritedFontSize.value) {}

  bool isValid(const InterpolationEnvironment& environment,
               const InterpolationValue&) const final {
    return m_inheritedFontSize ==
           environment.state().parentFontDescription().getSize().value;
  }

  const float m_inheritedFontSize;
};

InterpolationValue convertFontSize(float size) {
  return InterpolationValue(
      LengthInterpolationFunctions::createInterpolablePixels(size));
}

InterpolationValue maybeConvertKeyword(
    CSSValueID valueID,
    const StyleResolverState& state,
    InterpolationType::ConversionCheckers& conversionCheckers) {
  if (FontSize::isValidValueID(valueID)) {
    bool isMonospace = state.style()->getFontDescription().isMonospace();
    conversionCheckers.push_back(IsMonospaceChecker::create(isMonospace));
    return convertFontSize(state.fontBuilder().fontSizeForKeyword(
        FontSize::keywordSize(valueID), isMonospace));
  }

  if (valueID != CSSValueSmaller && valueID != CSSValueLarger)
    return nullptr;

  const FontDescription::Size& inheritedFontSize =
      state.parentFontDescription().getSize();
  conversionCheckers.push_back(
      InheritedFontSizeChecker::create(inheritedFontSize));
  if (valueID == CSSValueSmaller)
    return convertFontSize(
        FontDescription::smallerSize(inheritedFontSize).value);
  return convertFontSize(FontDescription::largerSize(inheritedFontSize).value);
}

}  // namespace

InterpolationValue CSSFontSizeInterpolationType::maybeConvertNeutral(
    const InterpolationValue&,
    ConversionCheckers&) const {
  return InterpolationValue(
      LengthInterpolationFunctions::createNeutralInterpolableValue());
}

InterpolationValue CSSFontSizeInterpolationType::maybeConvertInitial(
    const StyleResolverState& state,
    ConversionCheckers& conversionCheckers) const {
  return maybeConvertKeyword(FontSize::initialValueID(), state,
                             conversionCheckers);
}

InterpolationValue CSSFontSizeInterpolationType::maybeConvertInherit(
    const StyleResolverState& state,
    ConversionCheckers& conversionCheckers) const {
  const FontDescription::Size& inheritedFontSize =
      state.parentFontDescription().getSize();
  conversionCheckers.push_back(
      InheritedFontSizeChecker::create(inheritedFontSize));
  return convertFontSize(inheritedFontSize.value);
}

InterpolationValue CSSFontSizeInterpolationType::maybeConvertValue(
    const CSSValue& value,
    const StyleResolverState* state,
    ConversionCheckers& conversionCheckers) const {
  std::unique_ptr<InterpolableValue> result =
      LengthInterpolationFunctions::maybeConvertCSSValue(value)
          .interpolableValue;
  if (result)
    return InterpolationValue(std::move(result));

  if (!value.isIdentifierValue())
    return nullptr;

  DCHECK(state);
  return maybeConvertKeyword(toCSSIdentifierValue(value).getValueID(), *state,
                             conversionCheckers);
}

InterpolationValue
CSSFontSizeInterpolationType::maybeConvertStandardPropertyUnderlyingValue(
    const ComputedStyle& style) const {
  return convertFontSize(style.specifiedFontSize());
}

void CSSFontSizeInterpolationType::applyStandardPropertyValue(
    const InterpolableValue& interpolableValue,
    const NonInterpolableValue*,
    StyleResolverState& state) const {
  const FontDescription& parentFont = state.parentFontDescription();
  Length fontSizeLength = LengthInterpolationFunctions::createLength(
      interpolableValue, nullptr, state.fontSizeConversionData(),
      ValueRangeNonNegative);
  float fontSize =
      floatValueForLength(fontSizeLength, parentFont.getSize().value);
  state.fontBuilder().setSize(FontDescription::Size(
      0, fontSize,
      !fontSizeLength.isPercentOrCalc() || parentFont.isAbsoluteSize()));
}

}  // namespace blink
