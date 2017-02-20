// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/animation/CSSVisibilityInterpolationType.h"

#include "core/css/CSSPrimitiveValueMappings.h"
#include "core/css/resolver/StyleResolverState.h"
#include "wtf/PtrUtil.h"
#include <memory>

namespace blink {

class CSSVisibilityNonInterpolableValue : public NonInterpolableValue {
 public:
  ~CSSVisibilityNonInterpolableValue() final {}

  static PassRefPtr<CSSVisibilityNonInterpolableValue> create(EVisibility start,
                                                              EVisibility end) {
    return adoptRef(new CSSVisibilityNonInterpolableValue(start, end));
  }

  EVisibility visibility() const {
    DCHECK(m_isSingle);
    return m_start;
  }

  EVisibility visibility(double fraction) const {
    if (m_isSingle || fraction <= 0)
      return m_start;
    if (fraction >= 1)
      return m_end;
    DCHECK(m_start == EVisibility::kVisible || m_end == EVisibility::kVisible);
    return EVisibility::kVisible;
  }

  DECLARE_NON_INTERPOLABLE_VALUE_TYPE();

 private:
  CSSVisibilityNonInterpolableValue(EVisibility start, EVisibility end)
      : m_start(start), m_end(end), m_isSingle(m_start == m_end) {}

  const EVisibility m_start;
  const EVisibility m_end;
  const bool m_isSingle;
};

DEFINE_NON_INTERPOLABLE_VALUE_TYPE(CSSVisibilityNonInterpolableValue);
DEFINE_NON_INTERPOLABLE_VALUE_TYPE_CASTS(CSSVisibilityNonInterpolableValue);

class UnderlyingVisibilityChecker
    : public InterpolationType::ConversionChecker {
 public:
  ~UnderlyingVisibilityChecker() final {}

  static std::unique_ptr<UnderlyingVisibilityChecker> create(
      EVisibility visibility) {
    return WTF::wrapUnique(new UnderlyingVisibilityChecker(visibility));
  }

 private:
  UnderlyingVisibilityChecker(EVisibility visibility)
      : m_visibility(visibility) {}

  bool isValid(const InterpolationEnvironment&,
               const InterpolationValue& underlying) const final {
    double underlyingFraction =
        toInterpolableNumber(*underlying.interpolableValue).value();
    EVisibility underlyingVisibility =
        toCSSVisibilityNonInterpolableValue(*underlying.nonInterpolableValue)
            .visibility(underlyingFraction);
    return m_visibility == underlyingVisibility;
  }

  const EVisibility m_visibility;
};

class InheritedVisibilityChecker : public InterpolationType::ConversionChecker {
 public:
  static std::unique_ptr<InheritedVisibilityChecker> create(
      EVisibility visibility) {
    return WTF::wrapUnique(new InheritedVisibilityChecker(visibility));
  }

 private:
  InheritedVisibilityChecker(EVisibility visibility)
      : m_visibility(visibility) {}

  bool isValid(const InterpolationEnvironment& environment,
               const InterpolationValue& underlying) const final {
    return m_visibility == environment.state().parentStyle()->visibility();
  }

  const EVisibility m_visibility;
};

InterpolationValue CSSVisibilityInterpolationType::createVisibilityValue(
    EVisibility visibility) const {
  return InterpolationValue(
      InterpolableNumber::create(0),
      CSSVisibilityNonInterpolableValue::create(visibility, visibility));
}

InterpolationValue CSSVisibilityInterpolationType::maybeConvertNeutral(
    const InterpolationValue& underlying,
    ConversionCheckers& conversionCheckers) const {
  double underlyingFraction =
      toInterpolableNumber(*underlying.interpolableValue).value();
  EVisibility underlyingVisibility =
      toCSSVisibilityNonInterpolableValue(*underlying.nonInterpolableValue)
          .visibility(underlyingFraction);
  conversionCheckers.push_back(
      UnderlyingVisibilityChecker::create(underlyingVisibility));
  return createVisibilityValue(underlyingVisibility);
}

InterpolationValue CSSVisibilityInterpolationType::maybeConvertInitial(
    const StyleResolverState&,
    ConversionCheckers&) const {
  return createVisibilityValue(EVisibility::kVisible);
}

InterpolationValue CSSVisibilityInterpolationType::maybeConvertInherit(
    const StyleResolverState& state,
    ConversionCheckers& conversionCheckers) const {
  if (!state.parentStyle())
    return nullptr;
  EVisibility inheritedVisibility = state.parentStyle()->visibility();
  conversionCheckers.push_back(
      InheritedVisibilityChecker::create(inheritedVisibility));
  return createVisibilityValue(inheritedVisibility);
}

InterpolationValue CSSVisibilityInterpolationType::maybeConvertValue(
    const CSSValue& value,
    const StyleResolverState*,
    ConversionCheckers& conversionCheckers) const {
  if (!value.isIdentifierValue())
    return nullptr;

  const CSSIdentifierValue& identifierValue = toCSSIdentifierValue(value);
  CSSValueID keyword = identifierValue.getValueID();

  switch (keyword) {
    case CSSValueHidden:
    case CSSValueVisible:
    case CSSValueCollapse:
      return createVisibilityValue(identifierValue.convertTo<EVisibility>());
    default:
      return nullptr;
  }
}

InterpolationValue
CSSVisibilityInterpolationType::maybeConvertStandardPropertyUnderlyingValue(
    const ComputedStyle& style) const {
  return createVisibilityValue(style.visibility());
}

PairwiseInterpolationValue CSSVisibilityInterpolationType::maybeMergeSingles(
    InterpolationValue&& start,
    InterpolationValue&& end) const {
  EVisibility startVisibility =
      toCSSVisibilityNonInterpolableValue(*start.nonInterpolableValue)
          .visibility();
  EVisibility endVisibility =
      toCSSVisibilityNonInterpolableValue(*end.nonInterpolableValue)
          .visibility();
  // One side must be "visible".
  // Spec: https://drafts.csswg.org/css-transitions/#animtype-visibility
  if (startVisibility != endVisibility &&
      startVisibility != EVisibility::kVisible &&
      endVisibility != EVisibility::kVisible) {
    return nullptr;
  }
  return PairwiseInterpolationValue(InterpolableNumber::create(0),
                                    InterpolableNumber::create(1),
                                    CSSVisibilityNonInterpolableValue::create(
                                        startVisibility, endVisibility));
}

void CSSVisibilityInterpolationType::composite(
    UnderlyingValueOwner& underlyingValueOwner,
    double underlyingFraction,
    const InterpolationValue& value,
    double interpolationFraction) const {
  underlyingValueOwner.set(*this, value);
}

void CSSVisibilityInterpolationType::applyStandardPropertyValue(
    const InterpolableValue& interpolableValue,
    const NonInterpolableValue* nonInterpolableValue,
    StyleResolverState& state) const {
  // Visibility interpolation has been deferred to application time here due to
  // its non-linear behaviour.
  double fraction = toInterpolableNumber(interpolableValue).value();
  EVisibility visibility =
      toCSSVisibilityNonInterpolableValue(nonInterpolableValue)
          ->visibility(fraction);
  state.style()->setVisibility(visibility);
}

}  // namespace blink
