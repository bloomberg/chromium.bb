// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/animation/CSSBorderImageLengthBoxInterpolationType.h"

#include <memory>
#include "core/animation/BorderImageLengthBoxPropertyFunctions.h"
#include "core/animation/LengthInterpolationFunctions.h"
#include "core/css/CSSIdentifierValue.h"
#include "core/css/CSSQuadValue.h"
#include "core/css/resolver/StyleResolverState.h"
#include "wtf/PtrUtil.h"

namespace blink {

namespace {

enum SideIndex : unsigned {
  SideTop,
  SideRight,
  SideBottom,
  SideLeft,
  SideIndexCount,
};

enum class SideType {
  Number,
  Auto,
  Length,
};

SideType getSideType(const BorderImageLength& side) {
  if (side.isNumber()) {
    return SideType::Number;
  }
  if (side.length().isAuto()) {
    return SideType::Auto;
  }
  DCHECK(side.length().isSpecified());
  return SideType::Length;
}

SideType getSideType(const CSSValue& side) {
  if (side.isPrimitiveValue() && toCSSPrimitiveValue(side).isNumber()) {
    return SideType::Number;
  }
  if (side.isIdentifierValue() &&
      toCSSIdentifierValue(side).getValueID() == CSSValueAuto) {
    return SideType::Auto;
  }
  return SideType::Length;
}

struct SideTypes {
  explicit SideTypes(const BorderImageLengthBox& box) {
    type[SideTop] = getSideType(box.top());
    type[SideRight] = getSideType(box.right());
    type[SideBottom] = getSideType(box.bottom());
    type[SideLeft] = getSideType(box.left());
  }
  explicit SideTypes(const CSSQuadValue& quad) {
    type[SideTop] = getSideType(*quad.top());
    type[SideRight] = getSideType(*quad.right());
    type[SideBottom] = getSideType(*quad.bottom());
    type[SideLeft] = getSideType(*quad.left());
  }

  bool operator==(const SideTypes& other) const {
    for (size_t i = 0; i < SideIndexCount; i++) {
      if (type[i] != other.type[i])
        return false;
    }
    return true;
  }
  bool operator!=(const SideTypes& other) const { return !(*this == other); }

  SideType type[SideIndexCount];
};

}  // namespace

class CSSBorderImageLengthBoxNonInterpolableValue
    : public NonInterpolableValue {
 public:
  static PassRefPtr<CSSBorderImageLengthBoxNonInterpolableValue> create(
      const SideTypes& sideTypes,
      Vector<RefPtr<NonInterpolableValue>>&& sideNonInterpolableValues) {
    return adoptRef(new CSSBorderImageLengthBoxNonInterpolableValue(
        sideTypes, std::move(sideNonInterpolableValues)));
  }

  PassRefPtr<CSSBorderImageLengthBoxNonInterpolableValue> clone() {
    return adoptRef(new CSSBorderImageLengthBoxNonInterpolableValue(
        m_sideTypes,
        Vector<RefPtr<NonInterpolableValue>>(m_sideNonInterpolableValues)));
  }

  const SideTypes& sideTypes() const { return m_sideTypes; }
  const Vector<RefPtr<NonInterpolableValue>>& sideNonInterpolableValues()
      const {
    return m_sideNonInterpolableValues;
  }
  Vector<RefPtr<NonInterpolableValue>>& sideNonInterpolableValues() {
    return m_sideNonInterpolableValues;
  }

  DECLARE_NON_INTERPOLABLE_VALUE_TYPE();

 private:
  CSSBorderImageLengthBoxNonInterpolableValue(
      const SideTypes& sideTypes,
      Vector<RefPtr<NonInterpolableValue>>&& sideNonInterpolableValues)
      : m_sideTypes(sideTypes),
        m_sideNonInterpolableValues(sideNonInterpolableValues) {
    DCHECK_EQ(m_sideNonInterpolableValues.size(), SideIndexCount);
  }

  const SideTypes m_sideTypes;
  Vector<RefPtr<NonInterpolableValue>> m_sideNonInterpolableValues;
};

DEFINE_NON_INTERPOLABLE_VALUE_TYPE(CSSBorderImageLengthBoxNonInterpolableValue);
DEFINE_NON_INTERPOLABLE_VALUE_TYPE_CASTS(
    CSSBorderImageLengthBoxNonInterpolableValue);

namespace {

class UnderlyingSideTypesChecker : public InterpolationType::ConversionChecker {
 public:
  static std::unique_ptr<UnderlyingSideTypesChecker> create(
      const SideTypes& underlyingSideTypes) {
    return WTF::wrapUnique(new UnderlyingSideTypesChecker(underlyingSideTypes));
  }

  static SideTypes getUnderlyingSideTypes(
      const InterpolationValue& underlying) {
    return toCSSBorderImageLengthBoxNonInterpolableValue(
               *underlying.nonInterpolableValue)
        .sideTypes();
  }

 private:
  UnderlyingSideTypesChecker(const SideTypes& underlyingSideTypes)
      : m_underlyingSideTypes(underlyingSideTypes) {}

  bool isValid(const InterpolationEnvironment&,
               const InterpolationValue& underlying) const final {
    return m_underlyingSideTypes == getUnderlyingSideTypes(underlying);
  }

  const SideTypes m_underlyingSideTypes;
};

class InheritedSideTypesChecker : public InterpolationType::ConversionChecker {
 public:
  static std::unique_ptr<InheritedSideTypesChecker> create(
      CSSPropertyID property,
      const SideTypes& inheritedSideTypes) {
    return WTF::wrapUnique(
        new InheritedSideTypesChecker(property, inheritedSideTypes));
  }

 private:
  InheritedSideTypesChecker(CSSPropertyID property,
                            const SideTypes& inheritedSideTypes)
      : m_property(property), m_inheritedSideTypes(inheritedSideTypes) {}

  bool isValid(const InterpolationEnvironment& environment,
               const InterpolationValue& underlying) const final {
    return m_inheritedSideTypes ==
           SideTypes(
               BorderImageLengthBoxPropertyFunctions::getBorderImageLengthBox(
                   m_property, *environment.state().parentStyle()));
  }

  const CSSPropertyID m_property;
  const SideTypes m_inheritedSideTypes;
};

InterpolationValue convertBorderImageLengthBox(const BorderImageLengthBox& box,
                                               double zoom) {
  std::unique_ptr<InterpolableList> list =
      InterpolableList::create(SideIndexCount);
  Vector<RefPtr<NonInterpolableValue>> nonInterpolableValues(SideIndexCount);
  const BorderImageLength* sides[SideIndexCount] = {};
  sides[SideTop] = &box.top();
  sides[SideRight] = &box.right();
  sides[SideBottom] = &box.bottom();
  sides[SideLeft] = &box.left();

  for (size_t i = 0; i < SideIndexCount; i++) {
    const BorderImageLength& side = *sides[i];
    if (side.isNumber()) {
      list->set(i, InterpolableNumber::create(side.number()));
    } else if (side.length().isAuto()) {
      list->set(i, InterpolableList::create(0));
    } else {
      InterpolationValue convertedSide =
          LengthInterpolationFunctions::maybeConvertLength(side.length(), zoom);
      if (!convertedSide)
        return nullptr;
      list->set(i, std::move(convertedSide.interpolableValue));
      nonInterpolableValues[i] = std::move(convertedSide.nonInterpolableValue);
    }
  }

  return InterpolationValue(
      std::move(list), CSSBorderImageLengthBoxNonInterpolableValue::create(
                           SideTypes(box), std::move(nonInterpolableValues)));
}

}  // namespace

InterpolationValue
CSSBorderImageLengthBoxInterpolationType::maybeConvertNeutral(
    const InterpolationValue& underlying,
    ConversionCheckers& conversionCheckers) const {
  SideTypes underlyingSideTypes =
      UnderlyingSideTypesChecker::getUnderlyingSideTypes(underlying);
  conversionCheckers.push_back(
      UnderlyingSideTypesChecker::create(underlyingSideTypes));
  return InterpolationValue(underlying.interpolableValue->cloneAndZero(),
                            toCSSBorderImageLengthBoxNonInterpolableValue(
                                *underlying.nonInterpolableValue)
                                .clone());
}

InterpolationValue
CSSBorderImageLengthBoxInterpolationType::maybeConvertInitial(
    const StyleResolverState&,
    ConversionCheckers&) const {
  return convertBorderImageLengthBox(
      BorderImageLengthBoxPropertyFunctions::getInitialBorderImageLengthBox(
          cssProperty()),
      1);
}

InterpolationValue
CSSBorderImageLengthBoxInterpolationType::maybeConvertInherit(
    const StyleResolverState& state,
    ConversionCheckers& conversionCheckers) const {
  const BorderImageLengthBox& inherited =
      BorderImageLengthBoxPropertyFunctions::getBorderImageLengthBox(
          cssProperty(), *state.parentStyle());
  conversionCheckers.push_back(
      InheritedSideTypesChecker::create(cssProperty(), SideTypes(inherited)));
  return convertBorderImageLengthBox(inherited,
                                     state.parentStyle()->effectiveZoom());
}

InterpolationValue CSSBorderImageLengthBoxInterpolationType::maybeConvertValue(
    const CSSValue& value,
    const StyleResolverState*,
    ConversionCheckers&) const {
  if (!value.isQuadValue())
    return nullptr;

  const CSSQuadValue& quad = toCSSQuadValue(value);
  std::unique_ptr<InterpolableList> list =
      InterpolableList::create(SideIndexCount);
  Vector<RefPtr<NonInterpolableValue>> nonInterpolableValues(SideIndexCount);
  const CSSValue* sides[SideIndexCount] = {};
  sides[SideTop] = quad.top();
  sides[SideRight] = quad.right();
  sides[SideBottom] = quad.bottom();
  sides[SideLeft] = quad.left();

  for (size_t i = 0; i < SideIndexCount; i++) {
    const CSSValue& side = *sides[i];
    if (side.isPrimitiveValue() && toCSSPrimitiveValue(side).isNumber()) {
      list->set(i, InterpolableNumber::create(
                       toCSSPrimitiveValue(side).getDoubleValue()));
    } else if (side.isIdentifierValue() &&
               toCSSIdentifierValue(side).getValueID() == CSSValueAuto) {
      list->set(i, InterpolableList::create(0));
    } else {
      InterpolationValue convertedSide =
          LengthInterpolationFunctions::maybeConvertCSSValue(side);
      if (!convertedSide)
        return nullptr;
      list->set(i, std::move(convertedSide.interpolableValue));
      nonInterpolableValues[i] = std::move(convertedSide.nonInterpolableValue);
    }
  }

  return InterpolationValue(
      std::move(list), CSSBorderImageLengthBoxNonInterpolableValue::create(
                           SideTypes(quad), std::move(nonInterpolableValues)));
}

InterpolationValue CSSBorderImageLengthBoxInterpolationType::
    maybeConvertStandardPropertyUnderlyingValue(
        const ComputedStyle& style) const {
  return convertBorderImageLengthBox(
      BorderImageLengthBoxPropertyFunctions::getBorderImageLengthBox(
          cssProperty(), style),
      style.effectiveZoom());
}

PairwiseInterpolationValue
CSSBorderImageLengthBoxInterpolationType::maybeMergeSingles(
    InterpolationValue&& start,
    InterpolationValue&& end) const {
  const SideTypes& startSideTypes =
      toCSSBorderImageLengthBoxNonInterpolableValue(*start.nonInterpolableValue)
          .sideTypes();
  const SideTypes& endSideTypes =
      toCSSBorderImageLengthBoxNonInterpolableValue(*end.nonInterpolableValue)
          .sideTypes();
  if (startSideTypes != endSideTypes)
    return nullptr;

  return PairwiseInterpolationValue(std::move(start.interpolableValue),
                                    std::move(end.interpolableValue),
                                    std::move(start.nonInterpolableValue));
}

void CSSBorderImageLengthBoxInterpolationType::composite(
    UnderlyingValueOwner& underlyingValueOwner,
    double underlyingFraction,
    const InterpolationValue& value,
    double interpolationFraction) const {
  const SideTypes& underlyingSideTypes =
      toCSSBorderImageLengthBoxNonInterpolableValue(
          *underlyingValueOwner.value().nonInterpolableValue)
          .sideTypes();
  const auto& nonInterpolableValue =
      toCSSBorderImageLengthBoxNonInterpolableValue(
          *value.nonInterpolableValue);
  const SideTypes& sideTypes = nonInterpolableValue.sideTypes();

  if (underlyingSideTypes != sideTypes) {
    underlyingValueOwner.set(*this, value);
    return;
  }

  InterpolationValue& underlyingValue = underlyingValueOwner.mutableValue();
  InterpolableList& underlyingList =
      toInterpolableList(*underlyingValue.interpolableValue);
  Vector<RefPtr<NonInterpolableValue>>& underlyingSideNonInterpolableValues =
      toCSSBorderImageLengthBoxNonInterpolableValue(
          *underlyingValue.nonInterpolableValue)
          .sideNonInterpolableValues();
  const InterpolableList& list = toInterpolableList(*value.interpolableValue);
  const Vector<RefPtr<NonInterpolableValue>>& sideNonInterpolableValues =
      nonInterpolableValue.sideNonInterpolableValues();

  for (size_t i = 0; i < SideIndexCount; i++) {
    switch (sideTypes.type[i]) {
      case SideType::Number:
        underlyingList.getMutable(i)->scaleAndAdd(underlyingFraction,
                                                  *list.get(i));
        break;
      case SideType::Length:
        LengthInterpolationFunctions::composite(
            underlyingList.getMutable(i),
            underlyingSideNonInterpolableValues[i], underlyingFraction,
            *list.get(i), sideNonInterpolableValues[i].get());
        break;
      case SideType::Auto:
        break;
      default:
        NOTREACHED();
        break;
    }
  }
}

void CSSBorderImageLengthBoxInterpolationType::applyStandardPropertyValue(
    const InterpolableValue& interpolableValue,
    const NonInterpolableValue* nonInterpolableValue,
    StyleResolverState& state) const {
  const SideTypes& sideTypes =
      toCSSBorderImageLengthBoxNonInterpolableValue(nonInterpolableValue)
          ->sideTypes();
  const Vector<RefPtr<NonInterpolableValue>>& nonInterpolableValues =
      toCSSBorderImageLengthBoxNonInterpolableValue(nonInterpolableValue)
          ->sideNonInterpolableValues();
  const InterpolableList& list = toInterpolableList(interpolableValue);
  const auto& convertSide = [&sideTypes, &list, &state, &nonInterpolableValues](
      size_t index) -> BorderImageLength {
    switch (sideTypes.type[index]) {
      case SideType::Number:
        return clampTo<double>(toInterpolableNumber(list.get(index))->value(),
                               0);
      case SideType::Auto:
        return Length(Auto);
      case SideType::Length:
        return LengthInterpolationFunctions::createLength(
            *list.get(index), nonInterpolableValues[index].get(),
            state.cssToLengthConversionData(), ValueRangeNonNegative);
      default:
        NOTREACHED();
        return Length(Auto);
    }
  };
  BorderImageLengthBox box(convertSide(SideTop), convertSide(SideRight),
                           convertSide(SideBottom), convertSide(SideLeft));
  BorderImageLengthBoxPropertyFunctions::setBorderImageLengthBox(
      cssProperty(), *state.style(), box);
}

}  // namespace blink
