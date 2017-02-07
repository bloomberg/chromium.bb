// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/animation/CSSScaleInterpolationType.h"

#include "core/css/CSSPrimitiveValue.h"
#include "core/css/CSSValueList.h"
#include "core/css/resolver/StyleResolverState.h"
#include "wtf/PtrUtil.h"
#include <memory>

namespace blink {

namespace {

struct Scale {
  Scale(double x, double y, double z) { init(x, y, z, false); }
  explicit Scale() { init(1, 1, 1, true); }
  explicit Scale(const ScaleTransformOperation* scale) {
    if (scale)
      init(scale->x(), scale->y(), scale->z(), false);
    else
      init(1, 1, 1, true);
  }
  explicit Scale(const InterpolableValue& value) {
    const InterpolableList& list = toInterpolableList(value);
    if (list.length() == 0) {
      init(1, 1, 1, true);
      return;
    }
    init(toInterpolableNumber(*list.get(0)).value(),
         toInterpolableNumber(*list.get(1)).value(),
         toInterpolableNumber(*list.get(2)).value(), false);
  }

  void init(double x, double y, double z, bool isValueNone) {
    array[0] = x;
    array[1] = y;
    array[2] = z;
    isNone = isValueNone;
  }

  InterpolationValue createInterpolationValue() const;

  bool operator==(const Scale& other) const {
    for (size_t i = 0; i < 3; i++) {
      if (array[i] != other.array[i])
        return false;
    }
    return isNone == other.isNone;
  }

  double array[3];
  bool isNone;
};

std::unique_ptr<InterpolableValue> createIdentityInterpolableValue() {
  std::unique_ptr<InterpolableList> list = InterpolableList::create(3);
  for (size_t i = 0; i < 3; i++)
    list->set(i, InterpolableNumber::create(1));
  return std::move(list);
}

class InheritedScaleChecker : public InterpolationType::ConversionChecker {
 public:
  static std::unique_ptr<InheritedScaleChecker> create(const Scale& scale) {
    return WTF::wrapUnique(new InheritedScaleChecker(scale));
  }

 private:
  InheritedScaleChecker(const Scale& scale) : m_scale(scale) {}

  bool isValid(const InterpolationEnvironment& environment,
               const InterpolationValue&) const final {
    return m_scale == Scale(environment.state().parentStyle()->scale());
  }

  const Scale m_scale;
};

}  // namespace

class CSSScaleNonInterpolableValue : public NonInterpolableValue {
 public:
  ~CSSScaleNonInterpolableValue() final {}

  static PassRefPtr<CSSScaleNonInterpolableValue> create(const Scale& scale) {
    return adoptRef(
        new CSSScaleNonInterpolableValue(scale, scale, false, false));
  }

  static PassRefPtr<CSSScaleNonInterpolableValue> merge(
      const CSSScaleNonInterpolableValue& start,
      const CSSScaleNonInterpolableValue& end) {
    return adoptRef(new CSSScaleNonInterpolableValue(start.start(), end.end(),
                                                     start.isStartAdditive(),
                                                     end.isEndAdditive()));
  }

  const Scale& start() const { return m_start; }
  const Scale& end() const { return m_end; }
  bool isStartAdditive() const { return m_isStartAdditive; }
  bool isEndAdditive() const { return m_isEndAdditive; }

  void setIsAdditive() {
    m_isStartAdditive = true;
    m_isEndAdditive = true;
  }

  DECLARE_NON_INTERPOLABLE_VALUE_TYPE();

 private:
  CSSScaleNonInterpolableValue(const Scale& start,
                               const Scale& end,
                               bool isStartAdditive,
                               bool isEndAdditive)
      : m_start(start),
        m_end(end),
        m_isStartAdditive(isStartAdditive),
        m_isEndAdditive(isEndAdditive) {}

  const Scale m_start;
  const Scale m_end;
  bool m_isStartAdditive;
  bool m_isEndAdditive;
};

DEFINE_NON_INTERPOLABLE_VALUE_TYPE(CSSScaleNonInterpolableValue);
DEFINE_NON_INTERPOLABLE_VALUE_TYPE_CASTS(CSSScaleNonInterpolableValue);

InterpolationValue Scale::createInterpolationValue() const {
  if (isNone) {
    return InterpolationValue(InterpolableList::create(0),
                              CSSScaleNonInterpolableValue::create(*this));
  }

  std::unique_ptr<InterpolableList> list = InterpolableList::create(3);
  for (size_t i = 0; i < 3; i++)
    list->set(i, InterpolableNumber::create(array[i]));
  return InterpolationValue(std::move(list),
                            CSSScaleNonInterpolableValue::create(*this));
}

InterpolationValue CSSScaleInterpolationType::maybeConvertNeutral(
    const InterpolationValue&,
    ConversionCheckers&) const {
  return Scale(1, 1, 1).createInterpolationValue();
}

InterpolationValue CSSScaleInterpolationType::maybeConvertInitial(
    const StyleResolverState&,
    ConversionCheckers&) const {
  return Scale().createInterpolationValue();
}

InterpolationValue CSSScaleInterpolationType::maybeConvertInherit(
    const StyleResolverState& state,
    ConversionCheckers& conversionCheckers) const {
  Scale inheritedScale(state.parentStyle()->scale());
  conversionCheckers.push_back(InheritedScaleChecker::create(inheritedScale));
  return inheritedScale.createInterpolationValue();
}

InterpolationValue CSSScaleInterpolationType::maybeConvertValue(
    const CSSValue& value,
    const StyleResolverState*,
    ConversionCheckers&) const {
  if (!value.isBaseValueList())
    return Scale().createInterpolationValue();

  const CSSValueList& list = toCSSValueList(value);
  DCHECK(list.length() >= 1 && list.length() <= 3);

  Scale scale(1, 1, 1);
  for (size_t i = 0; i < list.length(); i++) {
    const CSSValue& item = list.item(i);
    scale.array[i] = toCSSPrimitiveValue(item).getDoubleValue();
  }

  return scale.createInterpolationValue();
}

void CSSScaleInterpolationType::additiveKeyframeHook(
    InterpolationValue& value) const {
  toCSSScaleNonInterpolableValue(*value.nonInterpolableValue).setIsAdditive();
}

PairwiseInterpolationValue CSSScaleInterpolationType::maybeMergeSingles(
    InterpolationValue&& start,
    InterpolationValue&& end) const {
  size_t startListLength =
      toInterpolableList(*start.interpolableValue).length();
  size_t endListLength = toInterpolableList(*end.interpolableValue).length();
  if (startListLength < endListLength)
    start.interpolableValue = createIdentityInterpolableValue();
  else if (endListLength < startListLength)
    end.interpolableValue = createIdentityInterpolableValue();

  return PairwiseInterpolationValue(
      std::move(start.interpolableValue), std::move(end.interpolableValue),
      CSSScaleNonInterpolableValue::merge(
          toCSSScaleNonInterpolableValue(*start.nonInterpolableValue),
          toCSSScaleNonInterpolableValue(*end.nonInterpolableValue)));
}

InterpolationValue
CSSScaleInterpolationType::maybeConvertStandardPropertyUnderlyingValue(
    const ComputedStyle& style) const {
  return Scale(style.scale()).createInterpolationValue();
}

void CSSScaleInterpolationType::composite(
    UnderlyingValueOwner& underlyingValueOwner,
    double underlyingFraction,
    const InterpolationValue& value,
    double interpolationFraction) const {
  if (toInterpolableList(*underlyingValueOwner.mutableValue().interpolableValue)
          .length() == 0) {
    underlyingValueOwner.mutableValue().interpolableValue =
        createIdentityInterpolableValue();
  }

  const CSSScaleNonInterpolableValue& metadata =
      toCSSScaleNonInterpolableValue(*value.nonInterpolableValue);
  DCHECK(metadata.isStartAdditive() || metadata.isEndAdditive());

  InterpolableList& underlyingList = toInterpolableList(
      *underlyingValueOwner.mutableValue().interpolableValue);
  for (size_t i = 0; i < 3; i++) {
    InterpolableNumber& underlying =
        toInterpolableNumber(*underlyingList.getMutable(i));

    double start = metadata.start().array[i] *
                   (metadata.isStartAdditive() ? underlying.value() : 1);
    double end = metadata.end().array[i] *
                 (metadata.isEndAdditive() ? underlying.value() : 1);
    underlying.set(blend(start, end, interpolationFraction));
  }
}

void CSSScaleInterpolationType::applyStandardPropertyValue(
    const InterpolableValue& interpolableValue,
    const NonInterpolableValue*,
    StyleResolverState& state) const {
  Scale scale(interpolableValue);
  if (scale.isNone) {
    state.style()->setScale(nullptr);
    return;
  }
  state.style()->setScale(ScaleTransformOperation::create(
      scale.array[0], scale.array[1], scale.array[2],
      TransformOperation::Scale3D));
}

}  // namespace blink
