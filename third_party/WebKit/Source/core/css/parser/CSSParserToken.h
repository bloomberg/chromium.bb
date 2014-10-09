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
    DelimiterToken,
    NumberToken,
    PercentageToken,
    DimensionToken,
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

    // Converts NumberToken to DimensionToken.
    void convertToDimensionWithUnit(String);

    // Converts NumberToken to PercentageToken.
    void convertToPercentage();

    CSSParserTokenType type() const { return m_type; }
    String value() const { return m_value; }
    String textForUnitTests() const;

    UChar delimiter() const;
    NumericValueType numericValueType() const;
    double numericValue() const;
    BlockType blockType() const { return m_blockType; }
    CSSPrimitiveValue::UnitType unitType() const { return m_unit; }

private:
    CSSParserTokenType m_type;
    String m_value;

    UChar m_delimiter; // Could be rolled into m_value?

    NumericValueType m_numericValueType;
    double m_numericValue;
    CSSPrimitiveValue::UnitType m_unit;

    BlockType m_blockType;
};

typedef Vector<CSSParserToken>::iterator CSSParserTokenIterator;

} // namespace

#endif // MediaQueryToken_h
