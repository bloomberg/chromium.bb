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

PassRefPtr<AnimatableLength> AnimatableLength::create(CSSValue* value)
{
    ASSERT(canCreateFrom(value));
    if (value->isPrimitiveValue()) {
        CSSPrimitiveValue* primitiveValue = WebCore::toCSSPrimitiveValue(value);
        const CSSCalcValue* calcValue = primitiveValue->cssCalcValue();
        if (calcValue)
            return create(calcValue->expressionNode(), primitiveValue);
        NumberUnitType unitType;
        bool isPrimitiveLength = primitiveUnitToNumberType(primitiveValue->primitiveType(), unitType);
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

        NumberUnitType unitType;
        // Only returns true if the type is a primitive length unit.
        return primitiveUnitToNumberType(primitiveValue->primitiveType(), unitType);
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
    if (m_unitType == UnitTypePixels)
        return Length(clampedNumber(range) * conversionData.zoom(), Fixed);
    if (m_unitType == UnitTypePercentage)
        return Length(clampedNumber(range), Percent);

    return toCSSPrimitiveValue(range)->convertToLength<AnyConversion>(conversionData);
}

bool AnimatableLength::usesDefaultInterpolationWith(const AnimatableValue* value) const
{
    const AnimatableLength* length = toAnimatableLength(value);
    NumberUnitType type = commonUnitType(length);
    return type == UnitTypeCalc && (isViewportUnit() || length->isViewportUnit());
}

PassRefPtr<AnimatableValue> AnimatableLength::interpolateTo(const AnimatableValue* value, double fraction) const
{
    const AnimatableLength* length = toAnimatableLength(value);
    NumberUnitType type = commonUnitType(length);
    if (type != UnitTypeCalc)
        return AnimatableLength::create(blend(m_number, length->m_number, fraction), type);

    // FIXME(crbug.com/168840): Support for viewport units in calc needs to be added before we can blend them with other units.
    if (isViewportUnit() || length->isViewportUnit())
        return defaultInterpolateTo(this, value, fraction);

    return AnimatableLength::create(scale(1 - fraction).get(), length->scale(fraction).get());
}

PassRefPtr<AnimatableValue> AnimatableLength::addWith(const AnimatableValue* value) const
{
    // Optimization for adding with 0.
    if (isUnitlessZero())
        return takeConstRef(value);

    const AnimatableLength* length = toAnimatableLength(value);
    if (length->isUnitlessZero())
        return takeConstRef(this);

    NumberUnitType type = commonUnitType(length);
    if (type != UnitTypeCalc)
        return AnimatableLength::create(m_number + length->m_number, type);

    return AnimatableLength::create(this, length);
}

bool AnimatableLength::equalTo(const AnimatableValue* value) const
{
    const AnimatableLength* length = toAnimatableLength(value);
    if (m_unitType != length->m_unitType)
        return false;
    if (isCalc())
        return m_calcExpression == length->m_calcExpression || m_calcExpression->equals(*length->m_calcExpression);
    return m_number == length->m_number;
}

PassRefPtrWillBeRawPtr<CSSCalcExpressionNode> AnimatableLength::toCSSCalcExpressionNode() const
{
    if (isCalc())
        return m_calcExpression;
    return CSSCalcValue::createExpressionNode(toCSSPrimitiveValue(AllValues), m_number == trunc(m_number));
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
            m_cachedCSSPrimitiveValue = CSSPrimitiveValue::create(clampedNumber(range), static_cast<CSSPrimitiveValue::UnitTypes>(numberTypeToPrimitiveUnit(m_unitType)));
    }
    return m_cachedCSSPrimitiveValue;
}

PassRefPtr<AnimatableLength> AnimatableLength::scale(double factor) const
{
    if (isCalc()) {
        return AnimatableLength::create(CSSCalcValue::createExpressionNode(
            m_calcExpression,
            CSSCalcValue::createExpressionNode(CSSPrimitiveValue::create(factor, CSSPrimitiveValue::CSS_NUMBER)),
            CalcMultiply));
    }
    return AnimatableLength::create(m_number * factor, m_unitType);
}

bool AnimatableLength::primitiveUnitToNumberType(unsigned short primitiveUnit, NumberUnitType& numberType)
{
    switch (primitiveUnit) {
    case CSSPrimitiveValue::CSS_PX:
    case CSSPrimitiveValue::CSS_CM:
    case CSSPrimitiveValue::CSS_MM:
    case CSSPrimitiveValue::CSS_IN:
    case CSSPrimitiveValue::CSS_PT:
    case CSSPrimitiveValue::CSS_PC:
        numberType = UnitTypePixels;
        return true;
    case CSSPrimitiveValue::CSS_EMS:
        numberType = UnitTypeFontSize;
        return true;
    case CSSPrimitiveValue::CSS_EXS:
        numberType = UnitTypeFontXSize;
        return true;
    case CSSPrimitiveValue::CSS_REMS:
        numberType = UnitTypeRootFontSize;
        return true;
    case CSSPrimitiveValue::CSS_PERCENTAGE:
        numberType = UnitTypePercentage;
        return true;
    case CSSPrimitiveValue::CSS_VW:
        numberType = UnitTypeViewportWidth;
        return true;
    case CSSPrimitiveValue::CSS_VH:
        numberType = UnitTypeViewportHeight;
        return true;
    case CSSPrimitiveValue::CSS_VMIN:
        numberType = UnitTypeViewportMin;
        return true;
    case CSSPrimitiveValue::CSS_VMAX:
        numberType = UnitTypeViewportMax;
        return true;
    default:
        return false;
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
    case UnitTypeCalc:
        return CSSPrimitiveValue::CSS_UNKNOWN;
    }
    ASSERT_NOT_REACHED();
    return CSSPrimitiveValue::CSS_UNKNOWN;
}

} // namespace WebCore
