// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "SizesAttributeParser.h"

#include "MediaTypeNames.h"
#include "core/css/MediaQueryEvaluator.h"
#include "core/css/parser/MediaQueryTokenizer.h"

namespace WebCore {

unsigned SizesAttributeParser::findEffectiveSize(const String& attribute, PassRefPtr<MediaValues> mediaValues)
{
    Vector<MediaQueryToken> tokens;
    SizesAttributeParser parser(mediaValues);

    MediaQueryTokenizer::tokenize(attribute, tokens);
    if (!parser.parse(tokens))
        return parser.effectiveSizeDefaultValue();
    return parser.effectiveSize();
}

bool SizesAttributeParser::calculateLengthInPixels(TokenIterator startToken, TokenIterator endToken, unsigned& result)
{
    MediaQueryTokenType type = startToken->type();
    if (type == DimensionToken) {
        int length;
        if (!CSSPrimitiveValue::isLength(startToken->unitType()))
            return false;
        if (m_mediaValues->computeLength(startToken->numericValue(), startToken->unitType(), length)) {
            if (length > 0) {
                result = (unsigned)length;
                return true;
            }
        }
    }
    if (type == FunctionToken) {
        // FIXME - Handle calc() functions here!
    }
    return false;
}

static void reverseSkipIrrelevantTokens(TokenIterator& token, TokenIterator startToken)
{
    TokenIterator endToken = token;
    while (token != startToken && (token->type() == WhitespaceToken || token->type() == CommentToken || token->type() == EOFToken))
        --token;
    if (token != endToken)
        ++token;
}

static void reverseSkipUntilComponentStart(TokenIterator& token, TokenIterator startToken)
{
    if (token == startToken)
        return;
    --token;
    if (token->blockType() != MediaQueryToken::BlockEnd)
        return;
    unsigned blockLevel = 0;
    while (token != startToken) {
        if (token->blockType() == MediaQueryToken::BlockEnd) {
            ++blockLevel;
        } else if (token->blockType() == MediaQueryToken::BlockStart) {
            --blockLevel;
            if (!blockLevel)
                break;
        }

        --token;
    }
}

bool SizesAttributeParser::mediaConditionMatches(PassRefPtr<MediaQuerySet> mediaCondition)
{
    // FIXME: How do I handle non-screen media types here?
    MediaQueryEvaluator mediaQueryEvaluator(MediaTypeNames::screen, *m_mediaValues);
    return mediaQueryEvaluator.eval(mediaCondition.get());
}

bool SizesAttributeParser::parseMediaConditionAndLength(TokenIterator startToken, TokenIterator endToken)
{
    TokenIterator lengthTokenStart;
    TokenIterator lengthTokenEnd;

    reverseSkipIrrelevantTokens(endToken, startToken);
    lengthTokenEnd = endToken;
    reverseSkipUntilComponentStart(endToken, startToken);
    lengthTokenStart = endToken;
    unsigned length;
    if (!calculateLengthInPixels(lengthTokenStart, lengthTokenEnd, length))
        return false;
    RefPtr<MediaQuerySet> mediaCondition = MediaQueryParser::parseMediaCondition(startToken, endToken);
    if (mediaCondition && mediaConditionMatches(mediaCondition)) {
        m_length = length;
        return true;
    }
    return false;
}

bool SizesAttributeParser::parse(Vector<MediaQueryToken>& tokens)
{
    TokenIterator startToken = tokens.begin();
    TokenIterator endToken;
    // Split on a comma token, and send the result tokens to be parsed as (media-condition, length) pairs
    for (TokenIterator token = tokens.begin(); token != tokens.end(); ++token) {
        if (token->type() == CommaToken) {
            endToken = token;
            if (parseMediaConditionAndLength(startToken, endToken))
                return true;
            startToken = token;
            ++startToken;
        }
    }
    endToken = tokens.end();
    return parseMediaConditionAndLength(startToken, --endToken);
}

unsigned SizesAttributeParser::effectiveSize()
{
    if (m_length)
        return m_length;
    return effectiveSizeDefaultValue();
}

unsigned SizesAttributeParser::effectiveSizeDefaultValue()
{
    // Returning the equivalent of "100%"
    return m_mediaValues->viewportWidth();
}

} // namespace

