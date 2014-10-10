// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/css/parser/SizesAttributeParser.h"

#include "core/MediaTypeNames.h"
#include "core/css/MediaQueryEvaluator.h"
#include "core/css/parser/CSSTokenizer.h"
#include "core/css/parser/SizesCalcParser.h"

namespace blink {

SizesAttributeParser::SizesAttributeParser(PassRefPtr<MediaValues> mediaValues, const String& attribute)
    : m_mediaValues(mediaValues)
    , m_length(0)
    , m_lengthWasSet(false)
    , m_viewportDependant(false)
{
    CSSTokenizer::tokenize(attribute, m_tokens);
    m_isValid = parse(m_tokens);
}

float SizesAttributeParser::length()
{
    if (m_isValid)
        return effectiveSize();
    return effectiveSizeDefaultValue();
}

bool SizesAttributeParser::calculateLengthInPixels(CSSParserTokenIterator startToken, CSSParserTokenIterator endToken, float& result)
{
    if (startToken == endToken)
        return false;
    CSSParserTokenType type = startToken->type();
    if (type == DimensionToken) {
        double length;
        if (!CSSPrimitiveValue::isLength(startToken->unitType()))
            return false;
        m_viewportDependant = CSSPrimitiveValue::isViewportPercentageLength(startToken->unitType());
        if ((m_mediaValues->computeLength(startToken->numericValue(), startToken->unitType(), length)) && (length >= 0)) {
            result = clampTo<float>(length);
            return true;
        }
    } else if (type == FunctionToken) {
        SizesCalcParser calcParser(startToken, endToken, m_mediaValues);
        if (!calcParser.isValid())
            return false;
        m_viewportDependant = calcParser.viewportDependant();
        result = calcParser.result();
        return true;
    } else if (type == NumberToken && !startToken->numericValue()) {
        result = 0;
        return true;
    }

    return false;
}

static void reverseSkipIrrelevantTokens(CSSParserTokenIterator& token, CSSParserTokenIterator startToken)
{
    CSSParserTokenIterator endToken = token;
    while (token != startToken && (token->type() == WhitespaceToken || token->type() == CommentToken || token->type() == EOFToken))
        --token;
    if (token != endToken)
        ++token;
}

static void reverseSkipUntilComponentStart(CSSParserTokenIterator& token, CSSParserTokenIterator startToken)
{
    if (token == startToken)
        return;
    --token;
    if (token->blockType() != CSSParserToken::BlockEnd)
        return;
    unsigned blockLevel = 0;
    while (token != startToken) {
        if (token->blockType() == CSSParserToken::BlockEnd) {
            ++blockLevel;
        } else if (token->blockType() == CSSParserToken::BlockStart) {
            --blockLevel;
            if (!blockLevel)
                break;
        }

        --token;
    }
}

bool SizesAttributeParser::mediaConditionMatches(PassRefPtrWillBeRawPtr<MediaQuerySet> mediaCondition)
{
    // A Media Condition cannot have a media type other then screen.
    MediaQueryEvaluator mediaQueryEvaluator(*m_mediaValues);
    return mediaQueryEvaluator.eval(mediaCondition.get());
}

bool SizesAttributeParser::parseMediaConditionAndLength(CSSParserTokenIterator startToken, CSSParserTokenIterator endToken)
{
    CSSParserTokenIterator lengthTokenStart;
    CSSParserTokenIterator lengthTokenEnd;

    reverseSkipIrrelevantTokens(endToken, startToken);
    lengthTokenEnd = endToken;
    reverseSkipUntilComponentStart(endToken, startToken);
    lengthTokenStart = endToken;
    float length;
    if (!calculateLengthInPixels(lengthTokenStart, lengthTokenEnd, length))
        return false;
    RefPtrWillBeRawPtr<MediaQuerySet> mediaCondition = MediaQueryParser::parseMediaCondition(startToken, endToken);
    if (mediaCondition && mediaConditionMatches(mediaCondition)) {
        m_length = length;
        m_lengthWasSet = true;
        return true;
    }
    return false;
}

bool SizesAttributeParser::parse(Vector<CSSParserToken>& tokens)
{
    if (tokens.isEmpty())
        return false;
    CSSParserTokenIterator startToken = tokens.begin();
    CSSParserTokenIterator endToken;
    // Split on a comma token, and send the result tokens to be parsed as (media-condition, length) pairs
    for (auto& token : tokens) {
        m_blockWatcher.handleToken(token);
        if (token.type() == CommaToken && !m_blockWatcher.blockLevel()) {
            endToken = &token;
            if (parseMediaConditionAndLength(startToken, endToken))
                return true;
            startToken = &token;
            ++startToken;
        }
    }
    endToken = tokens.end();
    return parseMediaConditionAndLength(startToken, --endToken);
}

float SizesAttributeParser::effectiveSize()
{
    if (m_lengthWasSet)
        return m_length;
    return effectiveSizeDefaultValue();
}

unsigned SizesAttributeParser::effectiveSizeDefaultValue()
{
    // Returning the equivalent of "100vw"
    m_viewportDependant = true;
    return m_mediaValues->viewportWidth();
}

} // namespace
