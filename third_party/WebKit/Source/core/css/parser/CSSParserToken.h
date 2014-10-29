// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSParserToken_h
#define CSSParserToken_h

#include "core/css/CSSPrimitiveValue.h"
#include "wtf/text/WTFString.h"

namespace blink {

enum CSSParserTokenType {
    IdentToken = 0,
    FunctionToken,
    HashToken,
    UrlToken,
    BadUrlToken,
    DelimiterToken,
    NumberToken,
    PercentageToken,
    DimensionToken,
    UnicodeRangeToken,
    WhitespaceToken,
    ColonToken,
    SemicolonToken,
    CommaToken,
    LeftParenthesisToken,
    RightParenthesisToken,
    LeftBracketToken,
    RightBracketToken,
    LeftBraceToken,
    RightBraceToken,
    StringToken,
    BadStringToken,
    EOFToken,
    CommentToken,
};

enum NumericValueType {
    IntegerValueType,
    NumberValueType,
};

enum HashTokenType {
    HashTokenId,
    HashTokenUnrestricted,
};

class CSSParserToken {
public:
    enum BlockType {
        NotBlock,
        BlockStart,
        BlockEnd,
    };

    CSSParserToken(CSSParserTokenType, BlockType = NotBlock);
    CSSParserToken(CSSParserTokenType, String value, BlockType = NotBlock);

    CSSParserToken(CSSParserTokenType, UChar); // for DelimiterToken
    CSSParserToken(CSSParserTokenType, double, NumericValueType); // for NumberToken
    CSSParserToken(CSSParserTokenType, UChar32, UChar32); // for UnicodeRangeToken

    CSSParserToken(HashTokenType, String);

    // Converts NumberToken to DimensionToken.
    void convertToDimensionWithUnit(String);

    // Converts NumberToken to PercentageToken.
    void convertToPercentage();

    CSSParserTokenType type() const { return m_type; }
    String value() const { return m_value; }

    UChar delimiter() const;
    NumericValueType numericValueType() const;
    double numericValue() const;
    HashTokenType hashTokenType() const { ASSERT(m_type == HashToken); return m_hashTokenType; }
    BlockType blockType() const { return m_blockType; }
    CSSPrimitiveValue::UnitType unitType() const { return m_unit; }
    UChar32 unicodeRangeStart() const { ASSERT(m_type == UnicodeRangeToken); return m_unicodeRangeStart; }
    UChar32 unicodeRangeEnd() const { ASSERT(m_type == UnicodeRangeToken); return m_unicodeRangeEnd; }

private:
    CSSParserTokenType m_type;
    String m_value;

    // This could be a union to save space
    UChar m_delimiter;
    HashTokenType m_hashTokenType;
    NumericValueType m_numericValueType;
    double m_numericValue;
    CSSPrimitiveValue::UnitType m_unit;
    UChar32 m_unicodeRangeStart;
    UChar32 m_unicodeRangeEnd;

    BlockType m_blockType;
};

typedef Vector<CSSParserToken>::iterator CSSParserTokenIterator;

} // namespace

#endif // MediaQueryToken_h
