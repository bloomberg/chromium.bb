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

#ifndef AnimatableNumber_h
#define AnimatableNumber_h

#include "core/animation/AnimatableValue.h"
#include "core/css/CSSCalculationValue.h"
#include "core/css/CSSPrimitiveValue.h"
#include "core/platform/CalculationValue.h"

namespace WebCore {

// Handles animation of CSSPrimitiveValues that can be represented by doubles including CSSCalcValue.
// See primitiveUnitToNumberType() for the list of supported units (with the exception of calc).
// If created from a CSSPrimitiveValue this class will cache it to be returned in toCSSValue().
class AnimatableNumber : public AnimatableValue {
public:
    enum NumberUnitType {
        UnitTypeNumber,
        UnitTypeLength,
        UnitTypeFontSize,
        UnitTypeFontXSize,
        UnitTypeRootFontSize,
        UnitTypePercentage,
        UnitTypeViewportWidth,
        UnitTypeViewportHeight,
        UnitTypeViewportMin,
        UnitTypeViewportMax,
        UnitTypeTime,
        UnitTypeAngle,
        UnitTypeFrequency,
        UnitTypeResolution,
        UnitTypeInvalid,
    };

    virtual ~AnimatableNumber() { }
    static bool canCreateFrom(const CSSValue*);
    static PassRefPtr<AnimatableNumber> create(CSSValue*);
    static PassRefPtr<AnimatableNumber> create(double number, NumberUnitType unitType, CSSPrimitiveValue* cssPrimitiveValue = 0)
    {
        return adoptRef(new AnimatableNumber(number, unitType, cssPrimitiveValue));
    }
    PassRefPtr<CSSValue> toCSSValue(CalculationPermittedValueRange = CalculationRangeAll) const;
    double toDouble() const;
    Length toLength(const RenderStyle* currStyle, const RenderStyle* rootStyle, double zoom, CalculationPermittedValueRange = CalculationRangeAll) const;

protected:
    virtual PassRefPtr<AnimatableValue> interpolateTo(const AnimatableValue*, double fraction) const OVERRIDE;
    virtual PassRefPtr<AnimatableValue> addWith(const AnimatableValue*) const OVERRIDE;

private:
    AnimatableNumber(double number, NumberUnitType unitType, CSSPrimitiveValue* cssPrimitiveValue)
        : AnimatableValue(TypeNumber)
        , m_number(number)
        , m_unitType(unitType)
        , m_isCalc(false)
        , m_cachedCSSPrimitiveValue(cssPrimitiveValue)
    {
        ASSERT(m_unitType != UnitTypeInvalid);
    }
    AnimatableNumber(PassRefPtr<CSSCalcExpressionNode> calcExpression, CSSPrimitiveValue* cssPrimitiveValue)
        : AnimatableValue(TypeNumber)
        , m_isCalc(true)
        , m_calcExpression(calcExpression)
        , m_cachedCSSPrimitiveValue(cssPrimitiveValue)
    {
        ASSERT(m_calcExpression);
    }

    static PassRefPtr<AnimatableNumber> create(PassRefPtr<CSSCalcExpressionNode> calcExpression, CSSPrimitiveValue* cssPrimitiveValue = 0)
    {
        return adoptRef(new AnimatableNumber(calcExpression, cssPrimitiveValue));
    }
    static PassRefPtr<AnimatableNumber> create(const AnimatableNumber* leftAddend, const AnimatableNumber* rightAddend);

    PassRefPtr<CSSPrimitiveValue> toCSSPrimitiveValue(CalculationPermittedValueRange) const;
    PassRefPtr<CSSCalcExpressionNode> toCSSCalcExpressionNode() const;

    PassRefPtr<AnimatableNumber> scale(double) const;
    static bool isCompatibleWithCalcRange(const CSSPrimitiveValue*, CalculationPermittedValueRange&);
    static NumberUnitType primitiveUnitToNumberType(unsigned short primitiveUnit);
    static unsigned short numberTypeToPrimitiveUnit(NumberUnitType numberType);

    double m_number;
    NumberUnitType m_unitType;

    bool m_isCalc;
    RefPtr<CSSCalcExpressionNode> m_calcExpression;

    mutable RefPtr<CSSPrimitiveValue> m_cachedCSSPrimitiveValue;
};

inline const AnimatableNumber* toAnimatableNumber(const AnimatableValue* value)
{
    ASSERT_WITH_SECURITY_IMPLICATION(value && value->isNumber());
    return static_cast<const AnimatableNumber*>(value);
}

} // namespace WebCore

#endif // AnimatableNumber_h
