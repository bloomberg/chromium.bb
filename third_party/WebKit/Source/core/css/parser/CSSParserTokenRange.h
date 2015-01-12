// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSParserTokenRange_h
#define CSSParserTokenRange_h

#include "core/css/parser/CSSParserToken.h"
#include "wtf/Vector.h"

namespace blink {

extern const CSSParserToken& staticEOFToken;

// A CSSParserTokenRange is an iterator over a subrange of a vector of CSSParserTokens.
// Accessing outside of the range will return an endless stream of EOF tokens.
// This class refers to half-open intervals [first, last).
class CSSParserTokenRange {
public:
    CSSParserTokenRange(const Vector<CSSParserToken>& vector)
    : m_first(vector.begin())
    , m_last(vector.end())
    {
    }

    // This should be called on a range with tokens returned by that range.
    CSSParserTokenRange makeSubRange(const CSSParserToken* first, const CSSParserToken* last);

    bool atEnd() const { return m_first == m_last; }
    const CSSParserToken* end() const { return m_last; }

    const CSSParserToken& peek(unsigned offset = 0) const
    {
        if (m_first + offset >= m_last)
            return staticEOFToken;
        return *(m_first + offset);
    }

    const CSSParserToken& consume()
    {
        if (m_first == m_last)
            return staticEOFToken;
        return *m_first++;
    }

    const CSSParserToken& consumeIncludingComments()
    {
        const CSSParserToken& result = consume();
        while (peek().type() == CommentToken)
            ++m_first;
        return result;
    }

    const CSSParserToken& consumeIncludingWhitespaceAndComments()
    {
        const CSSParserToken& result = consume();
        consumeWhitespaceAndComments();
        return result;
    }

    // The returned range doesn't include the brackets
    CSSParserTokenRange consumeBlock();

    void consumeComponentValue();
    void consumeComments();
    void consumeWhitespaceAndComments();

    static void initStaticEOFToken();

private:
    CSSParserTokenRange(const CSSParserToken* first, const CSSParserToken* last)
    : m_first(first)
    , m_last(last)
    { }

    const CSSParserToken* m_first;
    const CSSParserToken* m_last;
};

} // namespace

#endif // CSSParserTokenRange_h
