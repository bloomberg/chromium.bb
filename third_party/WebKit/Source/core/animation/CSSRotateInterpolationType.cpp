// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/animation/CSSRotateInterpolationType.h"

#include "core/css/resolver/StyleBuilderConverter.h"
#include "platform/transforms/RotateTransformOperation.h"
#include "platform/transforms/Rotation.h"
#include "wtf/PtrUtil.h"
#include <memory>

namespace blink {

class OptionalRotation {
 public:
  OptionalRotation() : m_isNone(true) {}

  explicit OptionalRotation(Rotation rotation)
      : m_rotation(rotation), m_isNone(false) {}

  bool isNone() const { return m_isNone; }
  const Rotation& rotation() const {
    DCHECK(!m_isNone);
    return m_rotation;
  }

  static OptionalRotation add(const OptionalRotation& a,
                              const OptionalRotation& b) {
    if (a.isNone())
      return b;
    if (b.isNone())
      return a;
    return OptionalRotation(Rotation::add(a.rotation(), b.rotation()));
  }
  static OptionalRotation slerp(const OptionalRotation& from,
                                const OptionalRotation& to,
                                double progress) {
    if (from.isNone() && to.isNone())
      return OptionalRotation();

    return OptionalRotation(
        Rotation::slerp(from.isNone() ? Rotation() : from.rotation(),
                        to.isNone() ? Rotation() : to.rotation(), progress));
  }

 private:
  Rotation m_rotation;
  bool m_isNone;
};

class CSSRotateNonInterpolableValue : public NonInterpolableValue {
 public:
  static PassRefPtr<CSSRotateNonInterpolableValue> create(
      const OptionalRotation& rotation) {
    return adoptRef(new CSSRotateNonInterpolableValue(
        true, rotation, OptionalRotation(), false, false));
  }

  static PassRefPtr<CSSRotateNonInterpolableValue> create(
      const CSSRotateNonInterpolableValue& start,
      const CSSRotateNonInterpolableValue& end) {
    return adoptRef(new CSSRotateNonInterpolableValue(
        false, start.optionalRotation(), end.optionalRotation(),
        start.isAdditive(), end.isAdditive()));
  }

  PassRefPtr<CSSRotateNonInterpolableValue> composite(
      const CSSRotateNonInterpolableValue& other,
      double otherProgress) {
    DCHECK(m_isSingle && !m_isStartAdditive);
    if (other.m_isSingle) {
      DCHECK_EQ(otherProgress, 0);
      DCHECK(other.isAdditive());
      return create(
          OptionalRotation::add(optionalRotation(), other.optionalRotation()));
    }

    DCHECK(other.m_isStartAdditive || other.m_isEndAdditive);
    OptionalRotation start =
        other.m_isStartAdditive
            ? OptionalRotation::add(optionalRotation(), other.m_start)
            : other.m_start;
    OptionalRotation end =
        other.m_isEndAdditive
            ? OptionalRotation::add(optionalRotation(), other.m_end)
            : other.m_end;
    return create(OptionalRotation::slerp(start, end, otherProgress));
  }

  void setSingleAdditive() {
    DCHECK(m_isSingle);
    m_isStartAdditive = true;
  }

  OptionalRotation slerpedRotation(double progress) const {
    DCHECK(!m_isStartAdditive && !m_isEndAdditive);
    DCHECK(!m_isSingle || progress == 0);
    if (progress == 0)
      return m_start;
    if (progress == 1)
      return m_end;
    return OptionalRotation::slerp(m_start, m_end, progress);
  }

  DECLARE_NON_INTERPOLABLE_VALUE_TYPE();

 private:
  CSSRotateNonInterpolableValue(bool isSingle,
                                const OptionalRotation& start,
                                const OptionalRotation& end,
                                bool isStartAdditive,
                                bool isEndAdditive)
      : m_isSingle(isSingle),
        m_start(start),
        m_end(end),
        m_isStartAdditive(isStartAdditive),
        m_isEndAdditive(isEndAdditive) {}

  const OptionalRotation& optionalRotation() const {
    DCHECK(m_isSingle);
    return m_start;
  }
  bool isAdditive() const {
    DCHECK(m_isSingle);
    return m_isStartAdditive;
  }

  bool m_isSingle;
  OptionalRotation m_start;
  OptionalRotation m_end;
  bool m_isStartAdditive;
  bool m_isEndAdditive;
};

DEFINE_NON_INTERPOLABLE_VALUE_TYPE(CSSRotateNonInterpolableValue);
DEFINE_NON_INTERPOLABLE_VALUE_TYPE_CASTS(CSSRotateNonInterpolableValue);

namespace {

OptionalRotation getRotation(const ComputedStyle& style) {
  if (!style.rotate())
    return OptionalRotation();
  return OptionalRotation(
      Rotation(style.rotate()->axis(), style.rotate()->angle()));
}

InterpolationValue convertRotation(const OptionalRotation& rotation) {
  return InterpolationValue(InterpolableNumber::create(0),
                            CSSRotateNonInterpolableValue::create(rotation));
}

class InheritedRotationChecker : public InterpolationType::ConversionChecker {
 public:
  static std::unique_ptr<InheritedRotationChecker> create(
      const OptionalRotation& inheritedRotation) {
    return WTF::wrapUnique(new InheritedRotationChecker(inheritedRotation));
  }

  bool isValid(const InterpolationEnvironment& environment,
               const InterpolationValue& underlying) const final {
    OptionalRotation inheritedRotation =
        getRotation(*environment.state().parentStyle());
    if (m_inheritedRotation.isNone() || inheritedRotation.isNone())
      return inheritedRotation.isNone() == inheritedRotation.isNone();
    return m_inheritedRotation.rotation().axis ==
               inheritedRotation.rotation().axis &&
           m_inheritedRotation.rotation().angle ==
               inheritedRotation.rotation().angle;
  }

 private:
  InheritedRotationChecker(const OptionalRotation& inheritedRotation)
      : m_inheritedRotation(inheritedRotation) {}

  const OptionalRotation m_inheritedRotation;
};

}  // namespace

InterpolationValue CSSRotateInterpolationType::maybeConvertNeutral(
    const InterpolationValue& underlying,
    ConversionCheckers&) const {
  return convertRotation(OptionalRotation(Rotation()));
}

InterpolationValue CSSRotateInterpolationType::maybeConvertInitial(
    const StyleResolverState&,
    ConversionCheckers&) const {
  return convertRotation(OptionalRotation());
}

InterpolationValue CSSRotateInterpolationType::maybeConvertInherit(
    const StyleResolverState& state,
    ConversionCheckers& conversionCheckers) const {
  OptionalRotation inheritedRotation = getRotation(*state.parentStyle());
  conversionCheckers.push_back(
      InheritedRotationChecker::create(inheritedRotation));
  return convertRotation(inheritedRotation);
}

InterpolationValue CSSRotateInterpolationType::maybeConvertValue(
    const CSSValue& value,
    const StyleResolverState*,
    ConversionCheckers&) const {
  if (!value.isBaseValueList()) {
    return convertRotation(OptionalRotation());
  }

  return convertRotation(
      OptionalRotation(StyleBuilderConverter::convertRotation(value)));
}

void CSSRotateInterpolationType::additiveKeyframeHook(
    InterpolationValue& value) const {
  toCSSRotateNonInterpolableValue(*value.nonInterpolableValue)
      .setSingleAdditive();
}

PairwiseInterpolationValue CSSRotateInterpolationType::maybeMergeSingles(
    InterpolationValue&& start,
    InterpolationValue&& end) const {
  return PairwiseInterpolationValue(
      InterpolableNumber::create(0), InterpolableNumber::create(1),
      CSSRotateNonInterpolableValue::create(
          toCSSRotateNonInterpolableValue(*start.nonInterpolableValue),
          toCSSRotateNonInterpolableValue(*end.nonInterpolableValue)));
}

InterpolationValue
CSSRotateInterpolationType::maybeConvertStandardPropertyUnderlyingValue(
    const ComputedStyle& style) const {
  return convertRotation(getRotation(style));
}

void CSSRotateInterpolationType::composite(
    UnderlyingValueOwner& underlyingValueOwner,
    double underlyingFraction,
    const InterpolationValue& value,
    double interpolationFraction) const {
  CSSRotateNonInterpolableValue& underlyingNonInterpolableValue =
      toCSSRotateNonInterpolableValue(
          *underlyingValueOwner.value().nonInterpolableValue);
  const CSSRotateNonInterpolableValue& nonInterpolableValue =
      toCSSRotateNonInterpolableValue(*value.nonInterpolableValue);
  double progress = toInterpolableNumber(*value.interpolableValue).value();
  underlyingValueOwner.mutableValue().nonInterpolableValue =
      underlyingNonInterpolableValue.composite(nonInterpolableValue, progress);
}

void CSSRotateInterpolationType::applyStandardPropertyValue(
    const InterpolableValue& interpolableValue,
    const NonInterpolableValue* untypedNonInterpolableValue,
    StyleResolverState& state) const {
  double progress = toInterpolableNumber(interpolableValue).value();
  const CSSRotateNonInterpolableValue& nonInterpolableValue =
      toCSSRotateNonInterpolableValue(*untypedNonInterpolableValue);
  OptionalRotation rotation = nonInterpolableValue.slerpedRotation(progress);
  if (rotation.isNone()) {
    state.style()->setRotate(nullptr);
    return;
  }
  state.style()->setRotate(RotateTransformOperation::create(
      rotation.rotation(), TransformOperation::Rotate3D));
}

}  // namespace blink
