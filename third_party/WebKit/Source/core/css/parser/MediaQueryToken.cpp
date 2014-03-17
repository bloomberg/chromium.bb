// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/css/parser/MediaQueryToken.h"

#include "wtf/HashMap.h"
#include "wtf/text/StringHash.h"

namespace WebCore {


MediaQueryToken::MediaQueryToken(MediaQueryTokenType type)
    : m_type(type)
    , m_delimiter(0)
    , m_unit(CSSPrimitiveValue::CSS_UNKNOWN)
{
}

// Just a helper used for Delimiter tokens.
MediaQueryToken::MediaQueryToken(MediaQueryTokenType type, UChar c)
    : m_type(type)
    , m_delimiter(c)
    , m_unit(CSSPrimitiveValue::CSS_UNKNOWN)
{
    ASSERT(m_type == DelimiterToken);
}

MediaQueryToken::MediaQueryToken(MediaQueryTokenType type, String value)
    : m_type(type)
    , m_value(value)
    , m_delimiter(0)
    , m_unit(CSSPrimitiveValue::CSS_UNKNOWN)
{
}

MediaQueryToken::MediaQueryToken(MediaQueryTokenType type, double numericValue, NumericValueType numericValueType)
    : m_type(type)
    , m_delimiter(0)
    , m_numericValueType(numericValueType)
    , m_numericValue(numericValue)
    , m_unit(CSSPrimitiveValue::CSS_NUMBER)
{
    ASSERT(type == NumberToken);
}

void MediaQueryToken::convertToDimensionWithUnit(String unit)
{
    ASSERT(m_type == NumberToken);
    m_type = DimensionToken;
    m_unit = CSSPrimitiveValue::getUnitTable().get(unit.lower());
}

void MediaQueryToken::convertToPercentage()
{
    ASSERT(m_type == NumberToken);
    m_type = PercentageToken;
    m_unit = CSSPrimitiveValue::CSS_PERCENTAGE;
}

} // namespace WebCore
