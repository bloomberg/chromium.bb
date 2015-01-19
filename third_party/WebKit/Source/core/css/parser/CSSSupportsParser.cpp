// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/css/parser/CSSSupportsParser.h"

#include "core/css/parser/CSSParserImpl.h"

namespace blink {

CSSSupportsParser::SupportsResult CSSSupportsParser::supportsCondition(CSSParserTokenRange range, CSSParserImpl& parser)
{
    // FIXME: The spec allows leading whitespace in @supports but not CSS.supports,
    // but major browser vendors allow it in CSS.supports also.
    range.consumeWhitespaceAndComments();
    return CSSSupportsParser(parser).consumeCondition(range);
}

enum ClauseType { Unresolved, Conjunction, Disjunction };

CSSSupportsParser::SupportsResult CSSSupportsParser::consumeCondition(CSSParserTokenRange range)
{
    if (range.peek().type() == IdentToken)
        return consumeNegation(range);

    bool result;
    ClauseType clauseType = Unresolved;

    while (true) {
        SupportsResult nextResult = consumeConditionInParenthesis(range);
        if (nextResult == Invalid)
            return Invalid;
        bool nextSupported = nextResult;
        if (clauseType == Unresolved)
            result = nextSupported;
        else if (clauseType == Conjunction)
            result &= nextSupported;
        else
            result |= nextSupported;

        range.consumeComments();
        if (range.atEnd())
            break;
        if (range.consumeIncludingWhitespaceAndComments().type() != WhitespaceToken)
            return Invalid;
        if (range.atEnd())
            break;

        const CSSParserToken& token = range.consumeIncludingComments();
        if (token.type() != IdentToken)
            return Invalid;
        if (clauseType == Unresolved)
            clauseType = token.value().length() == 3 ? Conjunction : Disjunction;
        if ((clauseType == Conjunction && !equalIgnoringCase(token.value(), "and"))
            || (clauseType == Disjunction && !equalIgnoringCase(token.value(), "or")))
            return Invalid;

        if (range.consumeIncludingWhitespaceAndComments().type() != WhitespaceToken)
            return Invalid;
    }
    return result ? Supported : Unsupported;
}

CSSSupportsParser::SupportsResult CSSSupportsParser::consumeNegation(CSSParserTokenRange range)
{
    ASSERT(range.peek().type() == IdentToken);
    if (!equalIgnoringCase(range.consumeIncludingComments().value(), "not"))
        return Invalid;
    if (range.consumeIncludingWhitespaceAndComments().type() != WhitespaceToken)
        return Invalid;
    SupportsResult result = consumeConditionInParenthesis(range);
    range.consumeWhitespaceAndComments();
    if (!range.atEnd() || result == Invalid)
        return Invalid;
    return result ? Unsupported : Supported;
}

CSSSupportsParser::SupportsResult CSSSupportsParser::consumeConditionInParenthesis(CSSParserTokenRange& range)
{
    if (range.peek().type() == FunctionToken) {
        range.consumeComponentValue();
        return Unsupported;
    }
    if (range.peek().type() != LeftParenthesisToken)
        return Invalid;
    CSSParserTokenRange innerRange = range.consumeBlock();
    innerRange.consumeWhitespaceAndComments();
    SupportsResult result = consumeCondition(innerRange);
    if (result != Invalid)
        return result;
    return innerRange.peek().type() == IdentToken && m_parser.supportsDeclaration(innerRange) ? Supported : Unsupported;
}

} // namespace blink
