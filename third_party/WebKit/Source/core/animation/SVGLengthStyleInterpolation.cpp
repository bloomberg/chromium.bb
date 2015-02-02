// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/animation/SVGLengthStyleInterpolation.h"

#include "core/css/resolver/StyleBuilder.h"

namespace blink {

namespace {

bool isBaseline(const CSSValue& value)
{
    if (!value.isPrimitiveValue())
        return false;
    const CSSPrimitiveValue& primitiveValue = toCSSPrimitiveValue(value);
    return primitiveValue.isValueID() && primitiveValue.getValueID() == CSSValueBaseline;
}

} // namespace

bool SVGLengthStyleInterpolation::canCreateFrom(const CSSValue& value)
{
    if (!value.isPrimitiveValue())
        return false;

    switch (toCSSPrimitiveValue(value).primitiveType()) {
    case CSSPrimitiveValue::CSS_NUMBER:
    case CSSPrimitiveValue::CSS_PERCENTAGE:
    case CSSPrimitiveValue::CSS_EMS:
    case CSSPrimitiveValue::CSS_EXS:
    case CSSPrimitiveValue::CSS_PX:
    case CSSPrimitiveValue::CSS_CM:
    case CSSPrimitiveValue::CSS_MM:
    case CSSPrimitiveValue::CSS_IN:
    case CSSPrimitiveValue::CSS_PT:
    case CSSPrimitiveValue::CSS_PC:
        return true;

    default:
        return isBaseline(value);
    }
}

PassOwnPtrWillBeRawPtr<InterpolableValue> SVGLengthStyleInterpolation::lengthToInterpolableValue(const CSSPrimitiveValue& length)
{
    ASSERT(canCreateFrom(length));
    return InterpolableNumber::create(length.getDoubleValue());
}

PassRefPtrWillBeRawPtr<CSSPrimitiveValue> SVGLengthStyleInterpolation::interpolableValueToLength(const InterpolableValue& interpolableValue, CSSPrimitiveValue::UnitType type, InterpolationRange range)
{
    double value = toInterpolableNumber(interpolableValue).value();
    if (range == RangeNonNegative && value < 0)
        value = 0;

    return CSSPrimitiveValue::create(value, type);
}

PassRefPtrWillBeRawPtr<SVGLengthStyleInterpolation> SVGLengthStyleInterpolation::maybeCreate(const CSSValue& start, const CSSValue& end, CSSPropertyID id, InterpolationRange range)
{
    if (!canCreateFrom(start) || !canCreateFrom(end))
        return nullptr;

    RefPtrWillBeRawPtr<CSSPrimitiveValue> zero = CSSPrimitiveValue::create(0, CSSPrimitiveValue::CSS_PX);
    const CSSPrimitiveValue& primitiveStart =
        isBaseline(start) ? *zero : toCSSPrimitiveValue(start);
    const CSSPrimitiveValue& primitiveEnd =
        isBaseline(end) ? *zero : toCSSPrimitiveValue(end);

    CSSPrimitiveValue::UnitType type = primitiveStart.primitiveType();
    if (primitiveStart.getDoubleValue() == 0)
        type = primitiveEnd.primitiveType();
    else if (primitiveEnd.getDoubleValue() != 0 && primitiveEnd.primitiveType() != type)
        return nullptr;

    return adoptRefWillBeNoop(new SVGLengthStyleInterpolation(primitiveStart, primitiveEnd, id, type, range));
}

SVGLengthStyleInterpolation::SVGLengthStyleInterpolation(const CSSPrimitiveValue& start, const CSSPrimitiveValue& end, CSSPropertyID id, CSSPrimitiveValue::UnitType type, InterpolationRange range)
    : StyleInterpolation(lengthToInterpolableValue(start), lengthToInterpolableValue(end), id)
    , m_type(type)
    , m_range(range)
{
}

void SVGLengthStyleInterpolation::apply(StyleResolverState& state) const
{
    StyleBuilder::applyProperty(m_id, state, interpolableValueToLength(*m_cachedValue, m_type, m_range).get());
}

}
