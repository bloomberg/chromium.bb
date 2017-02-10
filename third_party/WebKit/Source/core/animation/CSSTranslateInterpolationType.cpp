// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/animation/CSSTranslateInterpolationType.h"

#include "core/animation/LengthInterpolationFunctions.h"
#include "core/css/CSSValueList.h"
#include "core/css/resolver/StyleResolverState.h"
#include "platform/transforms/TranslateTransformOperation.h"
#include "wtf/PtrUtil.h"
#include <memory>

namespace blink {

namespace {

InterpolationValue createNoneValue() {
  return InterpolationValue(InterpolableList::create(0));
}

bool isNoneValue(const InterpolationValue& value) {
  return toInterpolableList(*value.interpolableValue).length() == 0;
}

class InheritedTranslateChecker : public InterpolationType::ConversionChecker {
 public:
  ~InheritedTranslateChecker() {}

  static std::unique_ptr<InheritedTranslateChecker> create(
      PassRefPtr<TranslateTransformOperation> inheritedTranslate) {
    return WTF::wrapUnique(
        new InheritedTranslateChecker(std::move(inheritedTranslate)));
  }

  bool isValid(const InterpolationEnvironment& environment,
               const InterpolationValue& underlying) const final {
    const TransformOperation* inheritedTranslate =
        environment.state().parentStyle()->translate();
    if (m_inheritedTranslate == inheritedTranslate)
      return true;
    if (!m_inheritedTranslate || !inheritedTranslate)
      return false;
    return *m_inheritedTranslate == *inheritedTranslate;
  }

 private:
  InheritedTranslateChecker(
      PassRefPtr<TranslateTransformOperation> inheritedTranslate)
      : m_inheritedTranslate(inheritedTranslate) {}

  RefPtr<TransformOperation> m_inheritedTranslate;
};

enum TranslateComponentIndex : unsigned {
  TranslateX,
  TranslateY,
  TranslateZ,
  TranslateComponentIndexCount,
};

std::unique_ptr<InterpolableValue> createIdentityInterpolableValue() {
  std::unique_ptr<InterpolableList> result =
      InterpolableList::create(TranslateComponentIndexCount);
  result->set(TranslateX,
              LengthInterpolationFunctions::createNeutralInterpolableValue());
  result->set(TranslateY,
              LengthInterpolationFunctions::createNeutralInterpolableValue());
  result->set(TranslateZ,
              LengthInterpolationFunctions::createNeutralInterpolableValue());
  return std::move(result);
}

InterpolationValue convertTranslateOperation(
    const TranslateTransformOperation* translate,
    double zoom) {
  if (!translate)
    return createNoneValue();

  std::unique_ptr<InterpolableList> result =
      InterpolableList::create(TranslateComponentIndexCount);
  result->set(TranslateX, LengthInterpolationFunctions::maybeConvertLength(
                              translate->x(), zoom)
                              .interpolableValue);
  result->set(TranslateY, LengthInterpolationFunctions::maybeConvertLength(
                              translate->y(), zoom)
                              .interpolableValue);
  result->set(TranslateZ, LengthInterpolationFunctions::maybeConvertLength(
                              Length(translate->z(), Fixed), zoom)
                              .interpolableValue);
  return InterpolationValue(std::move(result));
}

}  // namespace

InterpolationValue CSSTranslateInterpolationType::maybeConvertNeutral(
    const InterpolationValue& underlying,
    ConversionCheckers&) const {
  return InterpolationValue(createIdentityInterpolableValue());
}

InterpolationValue CSSTranslateInterpolationType::maybeConvertInitial(
    const StyleResolverState&,
    ConversionCheckers&) const {
  return createNoneValue();
}

InterpolationValue CSSTranslateInterpolationType::maybeConvertInherit(
    const StyleResolverState& state,
    ConversionCheckers& conversionCheckers) const {
  TranslateTransformOperation* inheritedTranslate =
      state.parentStyle()->translate();
  conversionCheckers.push_back(
      InheritedTranslateChecker::create(inheritedTranslate));
  return convertTranslateOperation(inheritedTranslate,
                                   state.parentStyle()->effectiveZoom());
}

InterpolationValue CSSTranslateInterpolationType::maybeConvertValue(
    const CSSValue& value,
    const StyleResolverState*,
    ConversionCheckers&) const {
  if (!value.isBaseValueList()) {
    return createNoneValue();
  }

  const CSSValueList& list = toCSSValueList(value);
  if (list.length() < 1 || list.length() > 3)
    return nullptr;

  std::unique_ptr<InterpolableList> result =
      InterpolableList::create(TranslateComponentIndexCount);
  for (size_t i = 0; i < TranslateComponentIndexCount; i++) {
    InterpolationValue component = nullptr;
    if (i < list.length()) {
      component =
          LengthInterpolationFunctions::maybeConvertCSSValue(list.item(i));
      if (!component)
        return nullptr;
    } else {
      component = InterpolationValue(
          LengthInterpolationFunctions::createNeutralInterpolableValue());
    }
    result->set(i, std::move(component.interpolableValue));
  }
  return InterpolationValue(std::move(result));
}

PairwiseInterpolationValue CSSTranslateInterpolationType::maybeMergeSingles(
    InterpolationValue&& start,
    InterpolationValue&& end) const {
  size_t startListLength =
      toInterpolableList(*start.interpolableValue).length();
  size_t endListLength = toInterpolableList(*end.interpolableValue).length();
  if (startListLength < endListLength)
    start.interpolableValue = createIdentityInterpolableValue();
  else if (endListLength < startListLength)
    end.interpolableValue = createIdentityInterpolableValue();

  return PairwiseInterpolationValue(std::move(start.interpolableValue),
                                    std::move(end.interpolableValue));
}

InterpolationValue
CSSTranslateInterpolationType::maybeConvertStandardPropertyUnderlyingValue(
    const ComputedStyle& style) const {
  return convertTranslateOperation(style.translate(), style.effectiveZoom());
}

void CSSTranslateInterpolationType::composite(
    UnderlyingValueOwner& underlyingValueOwner,
    double underlyingFraction,
    const InterpolationValue& value,
    double interpolationFraction) const {
  if (isNoneValue(value)) {
    return;
  }

  if (isNoneValue(underlyingValueOwner.mutableValue())) {
    underlyingValueOwner.mutableValue().interpolableValue =
        createIdentityInterpolableValue();
  }

  return CSSInterpolationType::composite(
      underlyingValueOwner, underlyingFraction, value, interpolationFraction);
}

void CSSTranslateInterpolationType::applyStandardPropertyValue(
    const InterpolableValue& interpolableValue,
    const NonInterpolableValue*,
    StyleResolverState& state) const {
  const InterpolableList& list = toInterpolableList(interpolableValue);
  if (list.length() == 0) {
    state.style()->setTranslate(nullptr);
    return;
  }
  const CSSToLengthConversionData& conversionData =
      state.cssToLengthConversionData();
  Length x = LengthInterpolationFunctions::createLength(
      *list.get(TranslateX), nullptr, conversionData, ValueRangeAll);
  Length y = LengthInterpolationFunctions::createLength(
      *list.get(TranslateY), nullptr, conversionData, ValueRangeAll);
  float z = LengthInterpolationFunctions::createLength(
                *list.get(TranslateZ), nullptr, conversionData, ValueRangeAll)
                .pixels();

  RefPtr<TranslateTransformOperation> result =
      TranslateTransformOperation::create(x, y, z,
                                          TransformOperation::Translate3D);
  state.style()->setTranslate(std::move(result));
}

}  // namespace blink
