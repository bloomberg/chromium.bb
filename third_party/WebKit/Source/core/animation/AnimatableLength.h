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

#ifndef AnimatableLength_h
#define AnimatableLength_h

#include "core/animation/AnimatableValue.h"
#include "core/css/CSSCalculationValue.h"
#include "core/css/CSSPrimitiveValue.h"
#include "platform/Length.h"

namespace WebCore {

enum NumberRange {
    AllValues,
    NonNegativeValues,
};

// Handles animation of CSS length and percentage values including CSS calc.
// See primitiveUnitToLengthType() for the list of supported units.
// If created from a CSSPrimitiveValue this class will cache it to be returned in toCSSValue().
class AnimatableLength FINAL : public AnimatableValue {
public:
    enum LengthUnitType {
        UnitTypePixels,
        UnitTypePercentage,
        UnitTypeFontSize,
        UnitTypeFontXSize,
        UnitTypeRootFontSize,
        UnitTypeViewportWidth,
        UnitTypeViewportHeight,
        UnitTypeViewportMin,
        UnitTypeViewportMax,
        UnitTypeCalc,
    };

    virtual ~AnimatableLength() { }
    static bool canCreateFrom(const CSSValue*);
    static PassRefPtrWillBeRawPtr<AnimatableLength> create(CSSValue*);
    static PassRefPtrWillBeRawPtr<AnimatableLength> create(double number, LengthUnitType unitType, CSSPrimitiveValue* cssPrimitiveValue = 0)
    {
        return adoptRefWillBeNoop(new AnimatableLength(number, unitType, cssPrimitiveValue));
    }
    static PassRefPtrWillBeRawPtr<AnimatableLength> create(PassRefPtrWillBeRawPtr<CSSCalcExpressionNode> calcExpression, CSSPrimitiveValue* cssPrimitiveValue = 0)
    {
        return adoptRefWillBeNoop(new AnimatableLength(calcExpression, cssPrimitiveValue));
    }
    PassRefPtrWillBeRawPtr<CSSValue> toCSSValue(NumberRange = AllValues) const;
    Length toLength(const CSSToLengthConversionData&, NumberRange = AllValues) const;

    virtual void trace(Visitor*) OVERRIDE;

protected:
    virtual PassRefPtrWillBeRawPtr<AnimatableValue> interpolateTo(const AnimatableValue*, double fraction) const OVERRIDE;
    virtual PassRefPtrWillBeRawPtr<AnimatableValue> addWith(const AnimatableValue*) const OVERRIDE;
    virtual bool usesDefaultInterpolationWith(const AnimatableValue*) const OVERRIDE;

private:
    AnimatableLength(double number, LengthUnitType unitType, CSSPrimitiveValue* cssPrimitiveValue)
        : m_lengthValue(number)
        , m_lengthUnitType(unitType)
        , m_cachedCSSPrimitiveValue(cssPrimitiveValue)
    {
        ASSERT(m_lengthUnitType != UnitTypeCalc);
    }
    AnimatableLength(PassRefPtrWillBeRawPtr<CSSCalcExpressionNode> calcExpression, CSSPrimitiveValue* cssPrimitiveValue)
        : m_lengthUnitType(UnitTypeCalc)
        , m_calcExpression(calcExpression)
        , m_cachedCSSPrimitiveValue(cssPrimitiveValue)
    {
        ASSERT(m_calcExpression);
    }
    virtual AnimatableType type() const OVERRIDE { return TypeLength; }
    virtual bool equalTo(const AnimatableValue*) const OVERRIDE;

    bool isCalc() const
    {
        return m_lengthUnitType == UnitTypeCalc;
    }

    bool isViewportUnit() const
    {
        return m_lengthUnitType == UnitTypeViewportWidth || m_lengthUnitType == UnitTypeViewportHeight || m_lengthUnitType == UnitTypeViewportMin || m_lengthUnitType == UnitTypeViewportMax;
    }

    static PassRefPtrWillBeRawPtr<AnimatableLength> create(const AnimatableLength* leftAddend, const AnimatableLength* rightAddend)
    {
        ASSERT(leftAddend && rightAddend);
        return create(CSSCalcValue::createExpressionNode(leftAddend->toCSSCalcExpressionNode(), rightAddend->toCSSCalcExpressionNode(), CalcAdd));
    }

    PassRefPtrWillBeRawPtr<CSSPrimitiveValue> toCSSPrimitiveValue(NumberRange) const;
    PassRefPtrWillBeRawPtr<CSSCalcExpressionNode> toCSSCalcExpressionNode() const;

    PassRefPtrWillBeRawPtr<AnimatableLength> scale(double) const;
    double clampedNumber(NumberRange range) const
    {
        ASSERT(!isCalc());
        return (range == NonNegativeValues && m_lengthValue <= 0) ? 0 : m_lengthValue;
    }

    // Returns true and populates lengthType, if primitiveUnit is a length unit. Otherwise, returns false.
    static bool primitiveUnitToLengthType(unsigned short primitiveUnit, LengthUnitType&);

    static unsigned short lengthTypeToPrimitiveUnit(LengthUnitType);

    // Zero is effectively unitless, except in the case of percentage.
    // http://www.w3.org/TR/css3-values/#calc-computed-value
    // e.g. calc(100% - 100% + 1em) resolves to calc(0% + 1em), not to calc(1em)
    bool isUnitlessZero() const
    {
        return !isCalc() && !m_lengthValue && m_lengthUnitType != UnitTypePercentage;
    }

    LengthUnitType commonUnitType(const AnimatableLength* length) const
    {
        if (m_lengthUnitType == length->m_lengthUnitType)
            return m_lengthUnitType;

        if (isUnitlessZero())
            return length->m_lengthUnitType;
        if (length->isUnitlessZero())
            return m_lengthUnitType;

        return UnitTypeCalc;
    }

    double m_lengthValue;
    const LengthUnitType m_lengthUnitType;

    RefPtrWillBeMember<CSSCalcExpressionNode> m_calcExpression;

    mutable RefPtrWillBeMember<CSSPrimitiveValue> m_cachedCSSPrimitiveValue;

    friend class AnimationAnimatableLengthTest;
};

DEFINE_ANIMATABLE_VALUE_TYPE_CASTS(AnimatableLength, isLength());

} // namespace WebCore

#endif // AnimatableLength_h
