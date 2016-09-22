// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/cssom/CSSSimpleLength.h"

#include "core/css/CSSPrimitiveValue.h"
#include "core/css/cssom/CSSCalcLength.h"
#include "wtf/text/StringBuilder.h"

namespace blink {

CSSValue* CSSSimpleLength::toCSSValue() const
{
    return CSSPrimitiveValue::create(m_value, m_unit);
}

bool CSSSimpleLength::containsPercent() const
{
    return lengthUnit() == CSSPrimitiveValue::UnitType::Percentage;
}

CSSLengthValue* CSSSimpleLength::addInternal(const CSSLengthValue* other)
{
    const CSSSimpleLength* o = toCSSSimpleLength(other);
    DCHECK_EQ(m_unit, o->m_unit);
    return create(m_value + o->value(), m_unit);
}

CSSLengthValue* CSSSimpleLength::subtractInternal(const CSSLengthValue* other)
{
    const CSSSimpleLength* o = toCSSSimpleLength(other);
    DCHECK_EQ(m_unit, o->m_unit);
    return create(m_value - o->value(), m_unit);
}

CSSLengthValue* CSSSimpleLength::multiplyInternal(double x)
{
    return create(m_value * x, m_unit);
}

CSSLengthValue* CSSSimpleLength::divideInternal(double x)
{
    DCHECK_NE(x, 0);
    return create(m_value / x, m_unit);
}

} // namespace blink
