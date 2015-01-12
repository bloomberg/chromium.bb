// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/css/parser/CSSParserTokenRange.h"

#include "wtf/StaticConstructors.h"

namespace blink {

DEFINE_GLOBAL(CSSParserToken, staticEOFToken);

void CSSParserTokenRange::initStaticEOFToken()
{
    new ((void*)&staticEOFToken) CSSParserToken(EOFToken);
}

CSSParserTokenRange CSSParserTokenRange::makeSubRange(const CSSParserToken* first, const CSSParserToken* last)
{
    if (first == &staticEOFToken)
        first = m_last;
    if (last == &staticEOFToken)
        last = m_last;
    ASSERT(first <= last);
    return CSSParserTokenRange(first, last);
}

CSSParserTokenRange CSSParserTokenRange::consumeBlock()
{
    ASSERT(peek().blockType() == CSSParserToken::BlockStart);
    const CSSParserToken* start = &peek() + 1;
    unsigned nestingLevel = 0;
    do {
        const CSSParserToken& token = consume();
        if (token.blockType() == CSSParserToken::BlockStart)
            nestingLevel++;
        else if (token.blockType() == CSSParserToken::BlockEnd)
            nestingLevel--;
    } while (nestingLevel && m_first < m_last);

    if (nestingLevel)
        return makeSubRange(start, m_first); // Ended at EOF
    return makeSubRange(start, m_first - 1);
}

void CSSParserTokenRange::consumeComponentValue()
{
    // FIXME: This is going to do multiple passes over large sections of a stylesheet.
    // We should consider optimising this by precomputing where each block ends.
    unsigned nestingLevel = 0;
    do {
        const CSSParserToken& token = consume();
        if (token.blockType() == CSSParserToken::BlockStart)
            nestingLevel++;
        else if (token.blockType() == CSSParserToken::BlockEnd)
            nestingLevel--;
    } while (nestingLevel && m_first < m_last);
}

void CSSParserTokenRange::consumeComments()
{
    while (peek().type() == CommentToken)
        ++m_first;
}

void CSSParserTokenRange::consumeWhitespaceAndComments()
{
    while (peek().type() == WhitespaceToken || peek().type() == CommentToken)
        ++m_first;
}

} // namespace blink
