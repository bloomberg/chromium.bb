// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/cssom/SimpleLength.h"

#include "core/css/CSSPrimitiveValue.h"
#include "core/css/cssom/StyleCalcLength.h"
#include "wtf/text/StringBuilder.h"

namespace blink {

CSSValue* SimpleLength::toCSSValue() const
{
    return cssValuePool().createValue(m_value, m_unit);
}

bool SimpleLength::containsPercent() const
{
    return lengthUnit() == CSSPrimitiveValue::UnitType::Percentage;
}

CSSLengthValue* SimpleLength::addInternal(const CSSLengthValue* other, ExceptionState& exceptionState)
{
    const SimpleLength* o = toSimpleLength(other);
    if (m_unit == o->m_unit)
        return create(m_value + o->value(), m_unit);

    // Different units resolve to a calc.
    StyleCalcLength* result = StyleCalcLength::create(this, exceptionState);
    return result->add(other, exceptionState);
}

CSSLengthValue* SimpleLength::subtractInternal(const CSSLengthValue* other, ExceptionState& exceptionState)
{
    const SimpleLength* o = toSimpleLength(other);
    if (m_unit == o->m_unit)
        return create(m_value - o->value(), m_unit);

    // Different units resolve to a calc.
    StyleCalcLength* result = StyleCalcLength::create(this, exceptionState);
    return result->subtract(other, exceptionState);
}

CSSLengthValue* SimpleLength::multiplyInternal(double x, ExceptionState& exceptionState)
{
    return create(m_value * x, m_unit);
}

CSSLengthValue* SimpleLength::divideInternal(double x, ExceptionState& exceptionState)
{
    return create(m_value / x, m_unit);
}

} // namespace blink
