// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MediaQueryToken_h
#define MediaQueryToken_h

#include "core/css/CSSPrimitiveValue.h"
#include "wtf/text/WTFString.h"

namespace WebCore {

enum MediaQueryTokenType {
    IdentToken = 0,
    FunctionToken = 1,
    DelimiterToken = 2,
    NumberToken = 3,
    PercentageToken = 4,
    DimensionToken = 5,
    WhitespaceToken = 6,
    ColonToken = 7,
    SemicolonToken = 8,
    CommaToken = 9,
    LeftParenthesisToken = 10,
    RightParenthesisToken = 11,
    LeftBracketToken = 12,
    RightBracketToken = 13,
    LeftBraceToken = 14,
    RightBraceToken = 15,
    StringToken = 15,
    BadStringToken = 16,
    EOFToken = 17,
    CommentToken = 18,
};

enum NumericValueType {
    IntegerValueType,
    NumberValueType,
};

class MediaQueryToken {
public:
    enum BlockType {
        NotBlock,
        BlockStart,
        BlockEnd,
    };

    MediaQueryToken(MediaQueryTokenType, BlockType = NotBlock);
    MediaQueryToken(MediaQueryTokenType, String value, BlockType = NotBlock);

    MediaQueryToken(MediaQueryTokenType, UChar); // for DelimiterToken
    MediaQueryToken(MediaQueryTokenType, double, NumericValueType); // for NumberToken

    // Converts NumberToken to DimensionToken.
    void convertToDimensionWithUnit(String);

    // Converts NumberToken to PercentageToken.
    void convertToPercentage();

    MediaQueryTokenType type() const { return m_type; }
    String value() const { return m_value; }
    String textForUnitTests() const;

    UChar delimiter() const { return m_delimiter; }
    NumericValueType numericValueType() const { return m_numericValueType; }
    double numericValue() const { return m_numericValue; }
    CSSPrimitiveValue::UnitTypes unitType() const { return m_unit; }
    BlockType blockType() const { return m_blockType; }

private:
    MediaQueryTokenType m_type;
    String m_value;

    UChar m_delimiter; // Could be rolled into m_value?

    NumericValueType m_numericValueType;
    double m_numericValue;
    CSSPrimitiveValue::UnitTypes m_unit;

    BlockType m_blockType;
};

} // namespace

#endif // MediaQueryToken_h
