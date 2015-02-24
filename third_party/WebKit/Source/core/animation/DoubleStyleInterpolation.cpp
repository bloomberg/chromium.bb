// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/animation/DoubleStyleInterpolation.h"

#include "core/css/CSSCalculationValue.h"
#include "core/css/resolver/StyleBuilder.h"

namespace blink {

bool DoubleStyleInterpolation::canCreateFrom(const CSSValue& value)
{
    ASSERT(value.isPrimitiveValue());
    return value.isPrimitiveValue() && (toCSSPrimitiveValue(value).isNumber() || toCSSPrimitiveValue(value).isAngle());
}

PassOwnPtrWillBeRawPtr<InterpolableValue> DoubleStyleInterpolation::doubleToInterpolableValue(const CSSValue& value)
{
    ASSERT(canCreateFrom(value));
    const CSSPrimitiveValue& primitive = toCSSPrimitiveValue(value);
    if (primitive.isNumber())
        return InterpolableNumber::create(primitive.getDoubleValue());
    if (primitive.isAngle())
        return InterpolableNumber::create(primitive.computeDegrees());
    ASSERT_NOT_REACHED();
    return nullptr;
}

PassRefPtrWillBeRawPtr<CSSValue> DoubleStyleInterpolation::interpolableValueToDouble(InterpolableValue* value, bool isNumber, InterpolationRange clamp)
{
    ASSERT(value->isNumber());
    double doubleValue = toInterpolableNumber(value)->value();

    switch (clamp) {
    case RangeAll:
        // Do nothing
        break;
    case RangeZeroToOne:
        doubleValue = clampTo<float>(doubleValue, 0, 1);
        break;
    case RangeOpacityFIXME:
        doubleValue = clampTo<float>(doubleValue, 0, nextafter(1, 0));
        break;
    case RangeFloor:
        doubleValue = floor(doubleValue);
        break;
    case RangeRound:
        doubleValue = round(doubleValue);
        break;
    case RangeRoundGreaterThanOrEqualToOne:
        doubleValue = clampTo<float>(round(doubleValue), 1);
        break;
    case RangeGreaterThanOrEqualToOne:
        doubleValue = clampTo<float>(doubleValue, 1);
        break;
    case RangeNonNegative:
        doubleValue = clampTo<float>(doubleValue, 0);
        break;
    default:
        ASSERT_NOT_REACHED();
    }
    if (isNumber)
        return CSSPrimitiveValue::create(doubleValue, CSSPrimitiveValue::CSS_NUMBER);
    return CSSPrimitiveValue::create(doubleValue, CSSPrimitiveValue::CSS_DEG);
}

void DoubleStyleInterpolation::apply(StyleResolverState& state) const
{
    if (m_id != CSSPropertyMotionRotation) {
        StyleBuilder::applyProperty(m_id, state, interpolableValueToDouble(m_cachedValue.get(), m_isNumber, m_clamp).get());
        return;
    }

    StyleBuilder::applyProperty(m_id, state, interpolableValueToMotionRotation(m_cachedValue.get(), m_flag).get());
}

DEFINE_TRACE(DoubleStyleInterpolation)
{
    StyleInterpolation::trace(visitor);
}

namespace {

bool extractMotionRotation(const CSSValue& value, float* rotation, MotionRotationType* rotationType)
{
    *rotation = 0;
    *rotationType = MotionRotationFixed;

    if (!value.isValueList())
        return false;
    const CSSValueList& list = toCSSValueList(value);

    int len = list.length();
    for (int i = 0; i < len; i++) {
        const CSSValue* item = list.item(i);
        if (!item->isPrimitiveValue())
            return false;
        const CSSPrimitiveValue* primitiveValue = toCSSPrimitiveValue(item);
        if (primitiveValue->getValueID() == CSSValueAuto) {
            *rotationType = MotionRotationAuto;
        } else if (primitiveValue->getValueID() == CSSValueReverse) {
            *rotationType = MotionRotationAuto;
            *rotation += 180;
        } else if (primitiveValue->isAngle() || primitiveValue->primitiveType() == CSSPrimitiveValue::CSS_DEG) {
            *rotation += primitiveValue->computeDegrees();
        } else {
            return false;
        }
    }
    return true;
}

} // namespace

PassRefPtrWillBeRawPtr<CSSValue> DoubleStyleInterpolation::interpolableValueToMotionRotation(InterpolableValue* value, bool flag)
{
    RefPtrWillBeRawPtr<CSSValueList> list = CSSValueList::createSpaceSeparated();
    if (flag)
        list->append(CSSPrimitiveValue::createIdentifier(CSSValueAuto));
    ASSERT(value->isNumber());
    list->append(CSSPrimitiveValue::create(toInterpolableNumber(value)->value(), CSSPrimitiveValue::CSS_DEG));
    return list.release();
}

PassOwnPtrWillBeRawPtr<InterpolableValue> DoubleStyleInterpolation::motionRotationToInterpolableValue(const CSSValue& value)
{
    float rotation;
    MotionRotationType rotationType;
    extractMotionRotation(value, &rotation, &rotationType);

    return InterpolableNumber::create(rotation);
}

PassRefPtrWillBeRawPtr<DoubleStyleInterpolation> DoubleStyleInterpolation::maybeCreateFromMotionRotation(const CSSValue& start, const CSSValue& end, CSSPropertyID id)
{
    float startRotation, endRotation;
    MotionRotationType startRotationType, endRotationType;

    if (!extractMotionRotation(start, &startRotation, &startRotationType)
        || !extractMotionRotation(end, &endRotation, &endRotationType)
        || startRotationType != endRotationType)
        return nullptr;

    return adoptRefWillBeNoop(new DoubleStyleInterpolation(
        motionRotationToInterpolableValue(start),
        motionRotationToInterpolableValue(end),
        id, true, InterpolationRange::RangeAll, startRotationType == MotionRotationAuto));
}

}
