/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "core/animation/AnimatableLength.h"

#include "core/css/CSSPrimitiveValueMappings.h"
#include "platform/CalculationValue.h"
#include "platform/animation/AnimationUtilities.h"

namespace WebCore {

PassRefPtrWillBeRawPtr<AnimatableLength> AnimatableLength::create(CSSValue* value)
{
    ASSERT(canCreateFrom(value));
    if (value->isPrimitiveValue()) {
        CSSPrimitiveValue* primitiveValue = WebCore::toCSSPrimitiveValue(value);
        const CSSCalcValue* calcValue = primitiveValue->cssCalcValue();
        if (calcValue)
            return create(calcValue->expressionNode(), primitiveValue);
        LengthUnitType unitType;
        bool isPrimitiveLength = primitiveUnitToLengthType(primitiveValue->primitiveType(), unitType);
        ASSERT_UNUSED(isPrimitiveLength, isPrimitiveLength);
        const double scale = CSSPrimitiveValue::conversionToCanonicalUnitsScaleFactor(primitiveValue->primitiveType());
        return create(primitiveValue->getDoubleValue() * scale, unitType, primitiveValue);
    }

    if (value->isCalcValue())
        return create(toCSSCalcValue(value)->expressionNode());

    ASSERT_NOT_REACHED();
    return nullptr;
}

bool AnimatableLength::canCreateFrom(const CSSValue* value)
{
    ASSERT(value);
    if (value->isPrimitiveValue()) {
        const CSSPrimitiveValue* primitiveValue = WebCore::toCSSPrimitiveValue(value);
        if (primitiveValue->cssCalcValue())
            return true;

        LengthUnitType unitType;
        // Only returns true if the type is a primitive length unit.
        return primitiveUnitToLengthType(primitiveValue->primitiveType(), unitType);
    }
    return value->isCalcValue();
}

PassRefPtrWillBeRawPtr<CSSValue> AnimatableLength::toCSSValue(NumberRange range) const
{
    return toCSSPrimitiveValue(range);
}

Length AnimatableLength::toLength(const CSSToLengthConversionData& conversionData, NumberRange range) const
{
    // Avoid creating a CSSValue in the common cases
    if (m_lengthUnitType == UnitTypePixels)
        return Length(clampedNumber(range) * conversionData.zoom(), Fixed);
    if (m_lengthUnitType == UnitTypePercentage)
        return Length(clampedNumber(range), Percent);

    return toCSSPrimitiveValue(range)->convertToLength<AnyConversion>(conversionData);
}

bool AnimatableLength::usesDefaultInterpolationWith(const AnimatableValue* value) const
{
    const AnimatableLength* length = toAnimatableLength(value);
    LengthUnitType type = commonUnitType(length);
    return type == UnitTypeCalc && (isViewportUnit() || length->isViewportUnit());
}

PassRefPtrWillBeRawPtr<AnimatableValue> AnimatableLength::interpolateTo(const AnimatableValue* value, double fraction) const
{
    const AnimatableLength* length = toAnimatableLength(value);
    LengthUnitType type = commonUnitType(length);
    if (type != UnitTypeCalc)
        return AnimatableLength::create(blend(m_lengthValue, length->m_lengthValue, fraction), type);

    // FIXME(crbug.com/168840): Support for viewport units in calc needs to be added before we can blend them with other units.
    if (isViewportUnit() || length->isViewportUnit())
        return defaultInterpolateTo(this, value, fraction);

    return AnimatableLength::create(scale(1 - fraction).get(), length->scale(fraction).get());
}

PassRefPtrWillBeRawPtr<AnimatableValue> AnimatableLength::addWith(const AnimatableValue* value) const
{
    // Optimization for adding with 0.
    if (isUnitlessZero())
        return takeConstRef(value);

    const AnimatableLength* length = toAnimatableLength(value);
    if (length->isUnitlessZero())
        return takeConstRef(this);

    LengthUnitType type = commonUnitType(length);
    if (type != UnitTypeCalc)
        return AnimatableLength::create(m_lengthValue + length->m_lengthValue, type);

    return AnimatableLength::create(this, length);
}

bool AnimatableLength::equalTo(const AnimatableValue* value) const
{
    const AnimatableLength* length = toAnimatableLength(value);
    if (m_lengthUnitType != length->m_lengthUnitType)
        return false;
    if (isCalc())
        return m_calcExpression == length->m_calcExpression || m_calcExpression->equals(*length->m_calcExpression);
    return m_lengthValue == length->m_lengthValue;
}

PassRefPtrWillBeRawPtr<CSSCalcExpressionNode> AnimatableLength::toCSSCalcExpressionNode() const
{
    if (isCalc())
        return m_calcExpression;
    return CSSCalcValue::createExpressionNode(toCSSPrimitiveValue(AllValues), m_lengthValue == trunc(m_lengthValue));
}

static bool isCompatibleWithRange(const CSSPrimitiveValue* primitiveValue, NumberRange range)
{
    ASSERT(primitiveValue);
    if (range == AllValues)
        return true;
    if (primitiveValue->isCalculated())
        return primitiveValue->cssCalcValue()->permittedValueRange() == ValueRangeNonNegative;
    return primitiveValue->getDoubleValue() >= 0;
}

PassRefPtrWillBeRawPtr<CSSPrimitiveValue> AnimatableLength::toCSSPrimitiveValue(NumberRange range) const
{
    if (!m_cachedCSSPrimitiveValue || !isCompatibleWithRange(m_cachedCSSPrimitiveValue.get(), range)) {
        if (isCalc())
            m_cachedCSSPrimitiveValue = CSSPrimitiveValue::create(CSSCalcValue::create(m_calcExpression, range == AllValues ? ValueRangeAll : ValueRangeNonNegative));
        else
            m_cachedCSSPrimitiveValue = CSSPrimitiveValue::create(clampedNumber(range), static_cast<CSSPrimitiveValue::UnitTypes>(lengthTypeToPrimitiveUnit(m_lengthUnitType)));
    }
    return m_cachedCSSPrimitiveValue;
}

PassRefPtrWillBeRawPtr<AnimatableLength> AnimatableLength::scale(double factor) const
{
    if (isCalc()) {
        return AnimatableLength::create(CSSCalcValue::createExpressionNode(
            m_calcExpression,
            CSSCalcValue::createExpressionNode(CSSPrimitiveValue::create(factor, CSSPrimitiveValue::CSS_NUMBER)),
            CalcMultiply));
    }
    return AnimatableLength::create(m_lengthValue * factor, m_lengthUnitType);
}

bool AnimatableLength::primitiveUnitToLengthType(unsigned short primitiveUnit, LengthUnitType& type)
{
    switch (primitiveUnit) {
    case CSSPrimitiveValue::CSS_PX:
    case CSSPrimitiveValue::CSS_CM:
    case CSSPrimitiveValue::CSS_MM:
    case CSSPrimitiveValue::CSS_IN:
    case CSSPrimitiveValue::CSS_PT:
    case CSSPrimitiveValue::CSS_PC:
        type = UnitTypePixels;
        return true;
    case CSSPrimitiveValue::CSS_EMS:
        type = UnitTypeFontSize;
        return true;
    case CSSPrimitiveValue::CSS_EXS:
        type = UnitTypeFontXSize;
        return true;
    case CSSPrimitiveValue::CSS_REMS:
        type = UnitTypeRootFontSize;
        return true;
    case CSSPrimitiveValue::CSS_PERCENTAGE:
        type = UnitTypePercentage;
        return true;
    case CSSPrimitiveValue::CSS_VW:
        type = UnitTypeViewportWidth;
        return true;
    case CSSPrimitiveValue::CSS_VH:
        type = UnitTypeViewportHeight;
        return true;
    case CSSPrimitiveValue::CSS_VMIN:
        type = UnitTypeViewportMin;
        return true;
    case CSSPrimitiveValue::CSS_VMAX:
        type = UnitTypeViewportMax;
        return true;
    default:
        return false;
    }
}

unsigned short AnimatableLength::lengthTypeToPrimitiveUnit(LengthUnitType type)
{
    switch (type) {
    case UnitTypePixels:
        return CSSPrimitiveValue::CSS_PX;
    case UnitTypeFontSize:
        return CSSPrimitiveValue::CSS_EMS;
    case UnitTypeFontXSize:
        return CSSPrimitiveValue::CSS_EXS;
    case UnitTypeRootFontSize:
        return CSSPrimitiveValue::CSS_REMS;
    case UnitTypePercentage:
        return CSSPrimitiveValue::CSS_PERCENTAGE;
    case UnitTypeViewportWidth:
        return CSSPrimitiveValue::CSS_VW;
    case UnitTypeViewportHeight:
        return CSSPrimitiveValue::CSS_VH;
    case UnitTypeViewportMin:
        return CSSPrimitiveValue::CSS_VMIN;
    case UnitTypeViewportMax:
        return CSSPrimitiveValue::CSS_VMAX;
    case UnitTypeCalc:
        return CSSPrimitiveValue::CSS_UNKNOWN;
    }
    ASSERT_NOT_REACHED();
    return CSSPrimitiveValue::CSS_UNKNOWN;
}

void AnimatableLength::trace(Visitor* visitor)
{
    visitor->trace(m_calcExpression);
    visitor->trace(m_cachedCSSPrimitiveValue);
}

} // namespace WebCore
