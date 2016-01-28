// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/cssom/StyleCalcLength.h"

#include "core/css/CSSCalculationValue.h"
#include "core/css/CSSPrimitiveValue.h"
#include "core/css/cssom/CalcDictionary.h"
#include "core/css/cssom/SimpleLength.h"
#include "wtf/Vector.h"
#include "wtf/text/StringBuilder.h"

namespace blink {

StyleCalcLength::StyleCalcLength() : m_values(LengthUnit::Count), m_hasValues(LengthUnit::Count) {}

StyleCalcLength::StyleCalcLength(const StyleCalcLength& other) :
    m_values(other.m_values),
    m_hasValues(other.m_hasValues)
{}

StyleCalcLength::StyleCalcLength(const SimpleLength& other) :
    m_values(LengthUnit::Count), m_hasValues(LengthUnit::Count)
{
    set(other.value(), other.lengthUnit());
}

StyleCalcLength* StyleCalcLength::create(const LengthValue* length)
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

#define setFromDictValue(name, camelName) \
    if (dictionary.has##camelName()) { \
        result->set(dictionary.name(), LengthValue::camelName); \
        numSet++; \
    }

    setFromDictValue(px, Px)
    setFromDictValue(percent, Percent)
    setFromDictValue(em, Em)
    setFromDictValue(ex, Ex)
    setFromDictValue(ch, Ch)
    setFromDictValue(rem, Rem)
    setFromDictValue(vw, Vw)
    setFromDictValue(vh, Vh)
    setFromDictValue(vmin, Vmin)
    setFromDictValue(vmax, Vmax)
    setFromDictValue(cm, Cm)
    setFromDictValue(mm, Mm)
    setFromDictValue(in, In)
    setFromDictValue(pc, Pc)
    setFromDictValue(pt, Pt)

    if (numSet == 0) {
        exceptionState.throwTypeError("Must specify at least one value in CalcDictionary for creating a CalcLength.");
    }
    return result;
}

LengthValue* StyleCalcLength::addInternal(const LengthValue* other, ExceptionState& exceptionState)
{
    StyleCalcLength* result = StyleCalcLength::create(other, exceptionState);
    for (unsigned i = 0; i < LengthUnit::Count; ++i) {
        LengthUnit lengthUnit = static_cast<LengthUnit>(i);
        if (has(lengthUnit)) {
            result->set(m_values[i] + result->get(lengthUnit), lengthUnit);
        }
    }
    return result;
}

LengthValue* StyleCalcLength::subtractInternal(const LengthValue* other, ExceptionState& exceptionState)
{

    StyleCalcLength* result = StyleCalcLength::create(this, exceptionState);
    if (other->type() == CalcLengthType) {
        const StyleCalcLength* o = toStyleCalcLength(other);
        for (unsigned i = 0; i < LengthUnit::Count; ++i) {
            LengthUnit lengthUnit = static_cast<LengthUnit>(i);
            if (o->has(lengthUnit)) {
                result->set(m_values[i] - o->get(lengthUnit), lengthUnit);
            }
        }
    } else {
        const SimpleLength* o = toSimpleLength(other);
        result->set(m_values[o->lengthUnit()] - o->value(), o->lengthUnit());
    }
    return result;
}

LengthValue* StyleCalcLength::multiplyInternal(double x, ExceptionState& exceptionState)
{
    StyleCalcLength* result = StyleCalcLength::create(this, exceptionState);
    for (unsigned i = 0; i < LengthUnit::Count; ++i) {
        LengthUnit lengthUnit = static_cast<LengthUnit>(i);
        if (has(lengthUnit)) {
            result->set(m_values[i] * x, lengthUnit);
        }
    }
    return result;
}

LengthValue* StyleCalcLength::divideInternal(double x, ExceptionState& exceptionState)
{
    StyleCalcLength* result = StyleCalcLength::create(this, exceptionState);
    for (unsigned i = 0; i < LengthUnit::Count; ++i) {
        LengthUnit lengthUnit = static_cast<LengthUnit>(i);
        if (has(lengthUnit)) {
            result->set(m_values[i] / x, lengthUnit);
        }
    }
    return result;
}

String StyleCalcLength::cssString() const
{
    StringBuilder builder;
    builder.appendLiteral("calc(");
    for (unsigned i = 0; i < LengthUnit::Count; ++i) {
        LengthUnit lengthUnit = static_cast<LengthUnit>(i);
        if (has(lengthUnit)) {
            double value = get(lengthUnit);
            if (value >= 0 && i > 0) {
                builder.appendLiteral(" + ");
            } else if (value < 0 && i > 0) {
                builder.appendLiteral(" - ");
            } else if (value < 0) {
                builder.append('-');
            }
            builder.appendNumber(std::abs(get(lengthUnit)));
            builder.append(lengthTypeToString(lengthUnit));
        }
    }
    builder.append(')');
    return builder.toString();
}

PassRefPtrWillBeRawPtr<CSSValue> StyleCalcLength::toCSSValue() const
{
    // Create a CSS Calc Value, then put it into a CSSPrimitiveValue
    RefPtrWillBeRawPtr<CSSCalcExpressionNode> node = nullptr;
    for (unsigned i = 0; i < LengthUnit::Count; ++i) {
        LengthUnit lengthUnit = static_cast<LengthUnit>(i);
        if (!has(lengthUnit))
            break;
        double value = get(lengthUnit);
        CSSPrimitiveValue::UnitType primitiveUnit;
        if (lengthUnit == LengthUnit::Percent) {
            primitiveUnit = CSSPrimitiveValue::UnitType::Percentage;
        } else {
            // TODO: Don't re-parse the unit here.
            primitiveUnit = CSSPrimitiveValue::fromName(lengthTypeToString(lengthUnit));
        }
        if (node) {
            node = CSSCalcValue::createExpressionNode(
                node,
                CSSCalcValue::createExpressionNode(CSSPrimitiveValue::create(value, primitiveUnit)),
                CalcAdd);
        } else {
            node = CSSCalcValue::createExpressionNode(CSSPrimitiveValue::create(value, primitiveUnit));
        }
    }
    return CSSPrimitiveValue::create(CSSCalcValue::create(node));
}

} // namespace blink
