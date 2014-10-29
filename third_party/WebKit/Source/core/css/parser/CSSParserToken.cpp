// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/css/parser/CSSParserToken.h"

#include "wtf/HashMap.h"
#include "wtf/text/StringHash.h"
#include <limits.h>

namespace blink {


CSSParserToken::CSSParserToken(CSSParserTokenType type, BlockType blockType)
    : m_type(type)
    , m_delimiter(0)
    , m_numericValue(0)
    , m_unit(CSSPrimitiveValue::CSS_UNKNOWN)
    , m_blockType(blockType)
{
}

// Just a helper used for Delimiter tokens.
CSSParserToken::CSSParserToken(CSSParserTokenType type, UChar c)
    : m_type(type)
    , m_delimiter(c)
    , m_numericValue(0)
    , m_unit(CSSPrimitiveValue::CSS_UNKNOWN)
    , m_blockType(NotBlock)
{
    ASSERT(m_type == DelimiterToken);
}

CSSParserToken::CSSParserToken(CSSParserTokenType type, String value, BlockType blockType)
    : m_type(type)
    , m_value(value)
    , m_delimiter(0)
    , m_numericValue(0)
    , m_unit(CSSPrimitiveValue::CSS_UNKNOWN)
    , m_blockType(blockType)
{
}

CSSParserToken::CSSParserToken(CSSParserTokenType type, double numericValue, NumericValueType numericValueType)
    : m_type(type)
    , m_delimiter(0)
    , m_numericValueType(numericValueType)
    , m_numericValue(numericValue)
    , m_unit(CSSPrimitiveValue::CSS_NUMBER)
    , m_blockType(NotBlock)
{
    ASSERT(type == NumberToken);
}

CSSParserToken::CSSParserToken(CSSParserTokenType type, UChar32 start, UChar32 end)
    : m_type(UnicodeRangeToken)
    , m_delimiter(0)
    , m_numericValue(0)
    , m_unit(CSSPrimitiveValue::CSS_UNKNOWN)
    , m_unicodeRangeStart(start)
    , m_unicodeRangeEnd(end)
    , m_blockType(NotBlock)
{
    ASSERT_UNUSED(type, type == UnicodeRangeToken);
}

CSSParserToken::CSSParserToken(HashTokenType type, String value)
    : m_type(HashToken)
    , m_value(value)
    , m_delimiter(0)
    , m_hashTokenType(type)
    , m_numericValue(0)
    , m_unit(CSSPrimitiveValue::CSS_UNKNOWN)
    , m_blockType(NotBlock)
{
}

void CSSParserToken::convertToDimensionWithUnit(String unit)
{
    ASSERT(m_type == NumberToken);
    m_type = DimensionToken;
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

NumericValueType CSSParserToken::numericValueType() const
{
    ASSERT(m_type == NumberToken || m_type == PercentageToken || m_type == DimensionToken);
    return m_numericValueType;
}

double CSSParserToken::numericValue() const
{
    ASSERT(m_type == NumberToken || m_type == PercentageToken || m_type == DimensionToken);
    return m_numericValue;
}

} // namespace blink
