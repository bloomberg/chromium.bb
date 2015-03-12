// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/css/parser/CSSParserToken.h"

#include "core/css/parser/CSSPropertyParser.h"
#include "wtf/HashMap.h"
#include <limits.h>

namespace blink {


CSSParserToken::CSSParserToken(CSSParserTokenType type, BlockType blockType)
    : m_type(type)
    , m_blockType(blockType)
{
}

// Just a helper used for Delimiter tokens.
CSSParserToken::CSSParserToken(CSSParserTokenType type, UChar c)
    : m_type(type)
    , m_blockType(NotBlock)
    , m_delimiter(c)
{
    ASSERT(m_type == DelimiterToken);
}

CSSParserToken::CSSParserToken(CSSParserTokenType type, CSSParserString value, BlockType blockType)
    : m_type(type)
    , m_blockType(blockType)
    , m_value(value)
{
}

CSSParserToken::CSSParserToken(CSSParserTokenType type, double numericValue, NumericValueType numericValueType, NumericSign sign)
    : m_type(type)
    , m_blockType(NotBlock)
    , m_numericValueType(numericValueType)
    , m_numericSign(sign)
    , m_unit(CSSPrimitiveValue::CSS_NUMBER)
    , m_numericValue(numericValue)
{
    ASSERT(type == NumberToken);
}

CSSParserToken::CSSParserToken(CSSParserTokenType type, CSSParserString string, UChar32 start, UChar32 end)
    : m_type(UnicodeRangeToken)
    , m_blockType(NotBlock)
    , m_value(string)
{
    ASSERT_UNUSED(type, type == UnicodeRangeToken);
    m_unicodeRange.start = start;
    m_unicodeRange.end = end;
}

CSSParserToken::CSSParserToken(HashTokenType type, CSSParserString value)
    : m_type(HashToken)
    , m_blockType(NotBlock)
    , m_value(value)
    , m_hashTokenType(type)
{
}

void CSSParserToken::convertToDimensionWithUnit(CSSParserString unit)
{
    ASSERT(m_type == NumberToken);
    m_type = DimensionToken;
    m_value = unit;
    m_unit = CSSPrimitiveValue::fromName(unit);
}

void CSSParserToken::convertToPercentage()
{
    ASSERT(m_type == NumberToken);
    m_type = PercentageToken;
    m_unit = CSSPrimitiveValue::CSS_PERCENTAGE;
}

UChar CSSParserToken::delimiter() const
{
    ASSERT(m_type == DelimiterToken);
    return m_delimiter;
}

NumericSign CSSParserToken::numericSign() const
{
    // This is valid for DimensionToken and PercentageToken, but only used
    // in <an+b> parsing on NumberTokens.
    ASSERT(m_type == NumberToken);
    return static_cast<NumericSign>(m_numericSign);
}

NumericValueType CSSParserToken::numericValueType() const
{
    ASSERT(m_type == NumberToken || m_type == PercentageToken || m_type == DimensionToken);
    return static_cast<NumericValueType>(m_numericValueType);
}

double CSSParserToken::numericValue() const
{
    ASSERT(m_type == NumberToken || m_type == PercentageToken || m_type == DimensionToken);
    return m_numericValue;
}

CSSPropertyID CSSParserToken::parseAsCSSPropertyID() const
{
    ASSERT(m_type == IdentToken);
    return cssPropertyID(m_value);
}

} // namespace blink
