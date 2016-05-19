// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/cssom/StyleCalcLength.h"

#include "core/css/CSSCalculationValue.h"
#include "core/css/CSSPrimitiveValue.h"
#include "core/css/cssom/CalcDictionary.h"
#include "core/css/cssom/SimpleLength.h"
#include "wtf/Vector.h"

namespace blink {

StyleCalcLength::StyleCalcLength() : m_values(CSSLengthValue::kNumSupportedUnits), m_hasValues(CSSLengthValue::kNumSupportedUnits) {}

StyleCalcLength::StyleCalcLength(const StyleCalcLength& other) :
    m_values(other.m_values),
    m_hasValues(other.m_hasValues)
{}

StyleCalcLength::StyleCalcLength(const SimpleLength& other) :
    m_values(CSSLengthValue::kNumSupportedUnits), m_hasValues(CSSLengthValue::kNumSupportedUnits)
{
    set(other.value(), other.lengthUnit());
}

StyleCalcLength* StyleCalcLength::create(const CSSLengthValue* length)
{
    if (length->type() == SimpleLengthType) {
        const SimpleLength* simpleLength = toSimpleLength(length);
        return new StyleCalcLength(*simpleLength);
    }

    return new StyleCalcLength(*toStyleCalcLength(length));
}

StyleCalcLength* StyleCalcLength::create(const CalcDictionary& dictionary, ExceptionState& exceptionState)
{
    StyleCalcLength* result = new StyleCalcLength();
    int numSet = 0;

#define setFromDictValue(name, camelName, primitiveName) \
    if (dictionary.has##camelName()) { \
        result->set(dictionary.name(), CSSPrimitiveValue::UnitType::primitiveName); \
        numSet++; \
    }

    setFromDictValue(px, Px, Pixels)
    setFromDictValue(percent, Percent, Percentage)
    setFromDictValue(em, Em, Ems)
    setFromDictValue(ex, Ex, Exs)
    setFromDictValue(ch, Ch, Chs)
    setFromDictValue(rem, Rem, Rems)
    setFromDictValue(vw, Vw, ViewportWidth)
    setFromDictValue(vh, Vh, ViewportHeight)
    setFromDictValue(vmin, Vmin, ViewportMin)
    setFromDictValue(vmax, Vmax, ViewportMax)
    setFromDictValue(cm, Cm, Centimeters)
    setFromDictValue(mm, Mm, Millimeters)
    setFromDictValue(in, In, Inches)
    setFromDictValue(pc, Pc, Picas)
    setFromDictValue(pt, Pt, Points)

    if (numSet == 0) {
        exceptionState.throwTypeError("Must specify at least one value in CalcDictionary for creating a CalcLength.");
    }
    return result;
}

bool StyleCalcLength::containsPercent() const
{
    return has(CSSPrimitiveValue::UnitType::Percentage);
}

CSSLengthValue* StyleCalcLength::addInternal(const CSSLengthValue* other, ExceptionState& exceptionState)
{
    StyleCalcLength* result = StyleCalcLength::create(other, exceptionState);
    for (int i = 0; i < CSSLengthValue::kNumSupportedUnits; ++i) {
        if (hasAtIndex(i)) {
            result->setAtIndex(getAtIndex(i) + result->getAtIndex(i), i);
        }
    }
    return result;
}

CSSLengthValue* StyleCalcLength::subtractInternal(const CSSLengthValue* other, ExceptionState& exceptionState)
{
    StyleCalcLength* result = StyleCalcLength::create(this, exceptionState);
    if (other->type() == CalcLengthType) {
        const StyleCalcLength* o = toStyleCalcLength(other);
        for (unsigned i = 0; i < CSSLengthValue::kNumSupportedUnits; ++i) {
            if (o->hasAtIndex(i)) {
                result->setAtIndex(getAtIndex(i) - o->getAtIndex(i), i);
            }
        }
    } else {
        const SimpleLength* o = toSimpleLength(other);
        result->set(get(o->lengthUnit()) - o->value(), o->lengthUnit());
    }
    return result;
}

CSSLengthValue* StyleCalcLength::multiplyInternal(double x, ExceptionState& exceptionState)
{
    StyleCalcLength* result = StyleCalcLength::create(this, exceptionState);
    for (unsigned i = 0; i < CSSLengthValue::kNumSupportedUnits; ++i) {
        if (hasAtIndex(i)) {
            result->setAtIndex(getAtIndex(i) * x, i);
        }
    }
    return result;
}

CSSLengthValue* StyleCalcLength::divideInternal(double x, ExceptionState& exceptionState)
{
    StyleCalcLength* result = StyleCalcLength::create(this, exceptionState);
    for (unsigned i = 0; i < CSSLengthValue::kNumSupportedUnits; ++i) {
        if (hasAtIndex(i)) {
            result->setAtIndex(getAtIndex(i) / x, i);
        }
    }
    return result;
}

CSSValue* StyleCalcLength::toCSSValue() const
{
    // Create a CSS Calc Value, then put it into a CSSPrimitiveValue
    CSSCalcExpressionNode* node = nullptr;
    for (unsigned i = 0; i < CSSLengthValue::kNumSupportedUnits; ++i) {
        if (!hasAtIndex(i))
            continue;
        double value = getAtIndex(i);
        if (node) {
            node = CSSCalcValue::createExpressionNode(
                node,
                CSSCalcValue::createExpressionNode(CSSPrimitiveValue::create(std::abs(value), unitFromIndex(i))),
                value >= 0 ? CalcAdd : CalcSubtract);
        } else {
            node = CSSCalcValue::createExpressionNode(CSSPrimitiveValue::create(value, unitFromIndex(i)));
        }
    }
    return CSSPrimitiveValue::create(CSSCalcValue::create(node));
}

int StyleCalcLength::indexForUnit(CSSPrimitiveValue::UnitType unit)
{
    return (static_cast<int>(unit) - static_cast<int>(CSSPrimitiveValue::UnitType::Percentage));
}

} // namespace blink
