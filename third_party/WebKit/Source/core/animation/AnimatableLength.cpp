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
#include "core/platform/CalculationValue.h"

namespace WebCore {

PassRefPtr<AnimatableLength> AnimatableLength::create(CSSValue* value)
{
    ASSERT(canCreateFrom(value));
    if (value->isPrimitiveValue()) {
        CSSPrimitiveValue* primitiveValue = WebCore::toCSSPrimitiveValue(value);
        const CSSCalcValue* calcValue = primitiveValue->cssCalcValue();
        if (calcValue)
            return create(calcValue->expressionNode(), primitiveValue);
        NumberUnitType unitType = primitiveUnitToNumberType(primitiveValue->primitiveType());
        ASSERT(unitType != UnitTypeInvalid);
        const double scale = CSSPrimitiveValue::conversionToCanonicalUnitsScaleFactor(primitiveValue->primitiveType());
        return create(primitiveValue->getDoubleValue() * scale, unitType, primitiveValue);
    }

    if (value->isCalcValue())
        return create(toCSSCalcValue(value)->expressionNode());

    ASSERT_NOT_REACHED();
    return 0;
}

PassRefPtr<AnimatableLength> AnimatableLength::create(const AnimatableLength* leftAddend, const AnimatableLength* rightAddend)
{
    ASSERT(leftAddend);
    ASSERT(rightAddend);

    if (!leftAddend->m_isCalc && !rightAddend->m_isCalc && leftAddend->m_unitType == rightAddend->m_unitType)
        return create(leftAddend->m_number + rightAddend->m_number, leftAddend->m_unitType);
    return create(CSSCalcValue::createExpressionNode(leftAddend->toCSSCalcExpressionNode(), rightAddend->toCSSCalcExpressionNode(), CalcAdd));
}

bool AnimatableLength::canCreateFrom(const CSSValue* value)
{
    ASSERT(value);
    if (value->isPrimitiveValue()) {
        const CSSPrimitiveValue* primitiveValue = WebCore::toCSSPrimitiveValue(value);
        if (primitiveValue->cssCalcValue())
            return true;
        return primitiveUnitToNumberType(primitiveValue->primitiveType()) != UnitTypeInvalid;
    }
    return value->isCalcValue();
}

PassRefPtr<CSSValue> AnimatableLength::toCSSValue(NumberRange range) const
{
    return toCSSPrimitiveValue(range);
}

Length AnimatableLength::toLength(const RenderStyle* style, const RenderStyle* rootStyle, double zoom, NumberRange range) const
{
    if (!m_isCalc) {
        // Avoid creating a CSSValue in the common cases
        if (m_unitType == UnitTypePixels)
            return Length(clampedNumber(range) * zoom, Fixed);
        if (m_unitType == UnitTypePercentage)
            return Length(clampedNumber(range), Percent);
    }
    return toCSSPrimitiveValue(range)->convertToLength<AnyConversion>(style, rootStyle, zoom);
}

PassRefPtr<AnimatableValue> AnimatableLength::interpolateTo(const AnimatableValue* value, double fraction) const
{
    const AnimatableLength* number = toAnimatableLength(value);
    return AnimatableLength::create(scale(1 - fraction).get(), number->scale(fraction).get());
}

PassRefPtr<AnimatableValue> AnimatableLength::addWith(const AnimatableValue* value) const
{
    // Optimization for adding with 0.
    if (!m_isCalc && !m_number)
        return takeConstRef(value);
    const AnimatableLength* number = toAnimatableLength(value);
    if (!number->m_isCalc && !number->m_number)
        return takeConstRef(this);

    return AnimatableLength::create(this, number);
}

PassRefPtr<CSSCalcExpressionNode> AnimatableLength::toCSSCalcExpressionNode() const
{
    if (m_isCalc)
        return m_calcExpression;
    return CSSCalcValue::createExpressionNode(toCSSPrimitiveValue(AllValues), m_number == trunc(m_number));
}

static bool isCompatibleWithRange(const CSSPrimitiveValue* primitiveValue, NumberRange range)
{
    ASSERT(primitiveValue);
    if (range == AllValues)
        return true;
    if (primitiveValue->isCalculated())
        return primitiveValue->cssCalcValue()->permittedValueRange() == CalculationRangeNonNegative;
    return primitiveValue->getDoubleValue() >= 0;
}

PassRefPtr<CSSPrimitiveValue> AnimatableLength::toCSSPrimitiveValue(NumberRange range) const
{
    ASSERT(m_unitType != UnitTypeInvalid);
    if (!m_cachedCSSPrimitiveValue || !isCompatibleWithRange(m_cachedCSSPrimitiveValue.get(), range)) {
        if (m_isCalc)
            m_cachedCSSPrimitiveValue = CSSPrimitiveValue::create(CSSCalcValue::create(m_calcExpression, range == AllValues ? CalculationRangeAll : CalculationRangeNonNegative));
        else
            m_cachedCSSPrimitiveValue = CSSPrimitiveValue::create(clampedNumber(range), static_cast<CSSPrimitiveValue::UnitTypes>(numberTypeToPrimitiveUnit(m_unitType)));
    }
    return m_cachedCSSPrimitiveValue;
}

PassRefPtr<AnimatableLength> AnimatableLength::scale(double factor) const
{
    if (m_isCalc) {
        return AnimatableLength::create(CSSCalcValue::createExpressionNode(
            m_calcExpression,
            CSSCalcValue::createExpressionNode(CSSPrimitiveValue::create(factor, CSSPrimitiveValue::CSS_NUMBER)),
            CalcMultiply));
    }
    return AnimatableLength::create(m_number * factor, m_unitType);
}

AnimatableLength::NumberUnitType AnimatableLength::primitiveUnitToNumberType(unsigned short primitiveUnit)
{
    switch (primitiveUnit) {
    case CSSPrimitiveValue::CSS_PX:
    case CSSPrimitiveValue::CSS_CM:
    case CSSPrimitiveValue::CSS_MM:
    case CSSPrimitiveValue::CSS_IN:
    case CSSPrimitiveValue::CSS_PT:
    case CSSPrimitiveValue::CSS_PC:
        return UnitTypePixels;
    case CSSPrimitiveValue::CSS_EMS:
        return UnitTypeFontSize;
    case CSSPrimitiveValue::CSS_EXS:
        return UnitTypeFontXSize;
    case CSSPrimitiveValue::CSS_REMS:
        return UnitTypeRootFontSize;
    case CSSPrimitiveValue::CSS_PERCENTAGE:
        return UnitTypePercentage;
    case CSSPrimitiveValue::CSS_VW:
        return UnitTypeViewportWidth;
    case CSSPrimitiveValue::CSS_VH:
        return UnitTypeViewportHeight;
    case CSSPrimitiveValue::CSS_VMIN:
        return UnitTypeViewportMin;
    case CSSPrimitiveValue::CSS_VMAX:
        return UnitTypeViewportMax;
    default:
        return UnitTypeInvalid;
    }
}

unsigned short AnimatableLength::numberTypeToPrimitiveUnit(NumberUnitType numberType)
{
    switch (numberType) {
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
    case UnitTypeInvalid:
        return CSSPrimitiveValue::CSS_UNKNOWN;
    }
    ASSERT_NOT_REACHED();
    return CSSPrimitiveValue::CSS_UNKNOWN;
}

} // namespace WebCore
