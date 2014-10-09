// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/css/parser/CSSTokenizer.h"

namespace blink {
#include "core/CSSTokenizerCodepoints.cpp"
}

#include "core/css/parser/CSSTokenizerInputStream.h"
#include "core/html/parser/HTMLParserIdioms.h"
#include "wtf/unicode/CharacterNames.h"

namespace blink {

// http://dev.w3.org/csswg/css-syntax/#name-start-code-point
static bool isNameStart(UChar c)
{
    if (isASCIIAlpha(c))
        return true;
    if (c == '_')
        return true;
    return !isASCII(c);
}

// http://dev.w3.org/csswg/css-syntax/#name-code-point
static bool isNameChar(UChar c)
{
    return isNameStart(c) || isASCIIDigit(c) || c == '-';
}

// http://dev.w3.org/csswg/css-syntax/#check-if-two-code-points-are-a-valid-escape
static bool twoCharsAreValidEscape(UChar first, UChar second)
{
    return ((first == '\\') && (second != '\n') && (second != kEndOfFileMarker));
}

CSSTokenizer::CSSTokenizer(CSSTokenizerInputStream& inputStream)
    : m_input(inputStream)
{
}

void CSSTokenizer::reconsume(UChar c)
{
    m_input.pushBack(c);
}

UChar CSSTokenizer::consume()
{
    UChar current = m_input.nextInputChar();
    m_input.advance();
    return current;
}

void CSSTokenizer::consume(unsigned offset)
{
    m_input.advance(offset);
}

CSSParserToken CSSTokenizer::whiteSpace(UChar cc)
{
    // CSS Tokenization is currently lossy, but we could record
    // the exact whitespace instead of discarding it here.
    consumeUntilNonWhitespace();
    return CSSParserToken(WhitespaceToken);
}

static bool popIfBlockMatches(Vector<CSSParserTokenType>& blockStack, CSSParserTokenType type)
{
    if (!blockStack.isEmpty() && blockStack.last() == type) {
        blockStack.removeLast();
        return true;
    }
    return false;
}

CSSParserToken CSSTokenizer::blockStart(CSSParserTokenType type)
{
    m_blockStack.append(type);
    return CSSParserToken(type, CSSParserToken::BlockStart);
}

CSSParserToken CSSTokenizer::blockStart(CSSParserTokenType blockType, CSSParserTokenType type, String name)
{
    m_blockStack.append(blockType);
    return CSSParserToken(type, name, CSSParserToken::BlockStart);
}

CSSParserToken CSSTokenizer::blockEnd(CSSParserTokenType type, CSSParserTokenType startType)
{
    if (popIfBlockMatches(m_blockStack, startType))
        return CSSParserToken(type, CSSParserToken::BlockEnd);
    return CSSParserToken(type);
}

CSSParserToken CSSTokenizer::leftParenthesis(UChar cc)
{
    return blockStart(LeftParenthesisToken);
}

CSSParserToken CSSTokenizer::rightParenthesis(UChar cc)
{
    return blockEnd(RightParenthesisToken, LeftParenthesisToken);
}

CSSParserToken CSSTokenizer::leftBracket(UChar cc)
{
    return blockStart(LeftBracketToken);
}

CSSParserToken CSSTokenizer::rightBracket(UChar cc)
{
    return blockEnd(RightBracketToken, LeftBracketToken);
}

CSSParserToken CSSTokenizer::leftBrace(UChar cc)
{
    return blockStart(LeftBraceToken);
}

CSSParserToken CSSTokenizer::rightBrace(UChar cc)
{
    return blockEnd(RightBraceToken, LeftBraceToken);
}

CSSParserToken CSSTokenizer::plusOrFullStop(UChar cc)
{
    if (nextCharsAreNumber(cc)) {
        reconsume(cc);
        return consumeNumericToken();
    }
    return CSSParserToken(DelimiterToken, cc);
}

CSSParserToken CSSTokenizer::asterisk(UChar cc)
{
    return CSSParserToken(DelimiterToken, cc);
}

CSSParserToken CSSTokenizer::comma(UChar cc)
{
    return CSSParserToken(CommaToken);
}

CSSParserToken CSSTokenizer::hyphenMinus(UChar cc)
{
    if (nextCharsAreNumber(cc)) {
        reconsume(cc);
        return consumeNumericToken();
    }
    if (nextCharsAreIdentifier(cc)) {
        reconsume(cc);
        return consumeIdentLikeToken();
    }
    return CSSParserToken(DelimiterToken, cc);
}

CSSParserToken CSSTokenizer::solidus(UChar cc)
{
    if (consumeIfNext('*')) {
        // We're intentionally deviating from the spec here, by creating tokens for CSS comments.
        return consumeUntilCommentEndFound()? CSSParserToken(CommentToken): CSSParserToken(EOFToken);
    }

    return CSSParserToken(DelimiterToken, cc);
}

CSSParserToken CSSTokenizer::colon(UChar cc)
{
    return CSSParserToken(ColonToken);
}

CSSParserToken CSSTokenizer::semiColon(UChar cc)
{
    return CSSParserToken(SemicolonToken);
}

CSSParserToken CSSTokenizer::reverseSolidus(UChar cc)
{
    if (twoCharsAreValidEscape(cc, m_input.nextInputChar())) {
        reconsume(cc);
        return consumeIdentLikeToken();
    }
    return CSSParserToken(DelimiterToken, cc);
}

CSSParserToken CSSTokenizer::asciiDigit(UChar cc)
{
    reconsume(cc);
    return consumeNumericToken();
}

CSSParserToken CSSTokenizer::nameStart(UChar cc)
{
    reconsume(cc);
    return consumeIdentLikeToken();
}

CSSParserToken CSSTokenizer::stringStart(UChar cc)
{
    return consumeStringTokenUntil(cc);
}

CSSParserToken CSSTokenizer::endOfFile(UChar cc)
{
    return CSSParserToken(EOFToken);
}

void CSSTokenizer::tokenize(String string, Vector<CSSParserToken>& outTokens)
{
    // According to the spec, we should perform preprocessing here.
    // See: http://dev.w3.org/csswg/css-syntax/#input-preprocessing
    //
    // However, we can skip this step since:
    // * We're using HTML spaces (which accept \r and \f as a valid white space)
    // * Do not count white spaces
    // * consumeEscape replaces NULLs for replacement characters

    if (string.isEmpty())
        return;

    CSSTokenizerInputStream input(string);
    CSSTokenizer tokenizer(input);
    while (true) {
        CSSParserToken token = tokenizer.nextToken();
        outTokens.append(token);
        if (token.type() == EOFToken)
            return;
    }
}

CSSParserToken CSSTokenizer::nextToken()
{
    // Unlike the HTMLTokenizer, the CSS Syntax spec is written
    // as a stateless, (fixed-size) look-ahead tokenizer.
    // We could move to the stateful model and instead create
    // states for all the "next 3 codepoints are X" cases.
    // State-machine tokenizers are easier to write to handle
    // incremental tokenization of partial sources.
    // However, for now we follow the spec exactly.
    UChar cc = consume();
    CodePoint codePointFunc = 0;

    if (isASCII(cc)) {
        ASSERT_WITH_SECURITY_IMPLICATION(cc < codePointsNumber);
        codePointFunc = codePoints[cc];
    } else {
        codePointFunc = &CSSTokenizer::nameStart;
    }

    if (codePointFunc)
        return ((this)->*(codePointFunc))(cc);
    return CSSParserToken(DelimiterToken, cc);
}

static int getSign(CSSTokenizerInputStream& input, unsigned& offset)
{
    int sign = 1;
    if (input.nextInputChar() == '+') {
        ++offset;
    } else if (input.peek(offset) == '-') {
        sign = -1;
        ++offset;
    }
    return sign;
}

static unsigned long long getInteger(CSSTokenizerInputStream& input, unsigned& offset)
{
    unsigned intStartPos = offset;
    offset = input.skipWhilePredicate<isASCIIDigit>(offset);
    unsigned intEndPos = offset;
    return input.getUInt(intStartPos, intEndPos);
}

static double getFraction(CSSTokenizerInputStream& input, unsigned& offset, unsigned& digitsNumber)
{
    unsigned fractionStartPos = 0;
    unsigned fractionEndPos = 0;
    if (input.peek(offset) == '.' && isASCIIDigit(input.peek(++offset))) {
        fractionStartPos = offset - 1;
        offset = input.skipWhilePredicate<isASCIIDigit>(offset);
        fractionEndPos = offset;
    }
    digitsNumber = fractionEndPos- fractionStartPos;
    return input.getDouble(fractionStartPos, fractionEndPos);
}

static unsigned long long getExponent(CSSTokenizerInputStream& input, unsigned& offset, int& sign)
{
    unsigned exponentStartPos = 0;
    unsigned exponentEndPos = 0;
    if ((input.peek(offset) == 'E' || input.peek(offset) == 'e')) {
        int offsetBeforeExponent = offset;
        ++offset;
        if (input.peek(offset) == '+') {
            ++offset;
        } else if (input.peek(offset) =='-') {
            sign = -1;
            ++offset;
        }
        exponentStartPos = offset;
        offset = input.skipWhilePredicate<isASCIIDigit>(offset);
        exponentEndPos = offset;
        if (exponentEndPos == exponentStartPos)
            offset = offsetBeforeExponent;
    }
    return input.getUInt(exponentStartPos, exponentEndPos);
}

// This method merges the following spec sections for efficiency
// http://www.w3.org/TR/css3-syntax/#consume-a-number
// http://www.w3.org/TR/css3-syntax/#convert-a-string-to-a-number
CSSParserToken CSSTokenizer::consumeNumber()
{
    ASSERT(nextCharsAreNumber());
    NumericValueType type = IntegerValueType;
    double value = 0;
    unsigned offset = 0;
    int exponentSign = 1;
    unsigned fractionDigits;
    int sign = getSign(m_input, offset);
    unsigned long long integerPart = getInteger(m_input, offset);
    double fractionPart = getFraction(m_input, offset, fractionDigits);
    unsigned long long exponentPart = getExponent(m_input, offset, exponentSign);
    double exponent = pow(10, (float)exponentSign * (double)exponentPart);
    value = (double)sign * ((double)integerPart + fractionPart) * exponent;

    m_input.advance(offset);
    if (fractionDigits > 0)
        type = NumberValueType;

    return CSSParserToken(NumberToken, value, type);
}

// http://www.w3.org/TR/css3-syntax/#consume-a-numeric-token
CSSParserToken CSSTokenizer::consumeNumericToken()
{
    CSSParserToken token = consumeNumber();
    if (nextCharsAreIdentifier())
        token.convertToDimensionWithUnit(consumeName());
    else if (consumeIfNext('%'))
        token.convertToPercentage();
    return token;
}

// http://www.w3.org/TR/css3-syntax/#consume-an-ident-like-token
CSSParserToken CSSTokenizer::consumeIdentLikeToken()
{
    String name = consumeName();
    if (consumeIfNext('(')) {
        return blockStart(LeftParenthesisToken, FunctionToken, name);
    }
    return CSSParserToken(IdentToken, name);
}

static bool isNewLine(UChar cc)
{
    // We check \r and \f here, since we have no preprocessing stage
    return (cc == '\r' || cc == '\n' || cc == '\f');
}

// http://dev.w3.org/csswg/css-syntax/#consume-a-string-token
CSSParserToken CSSTokenizer::consumeStringTokenUntil(UChar endingCodePoint)
{
    StringBuilder output;
    while (true) {
        UChar cc = consume();
        if (cc == endingCodePoint || cc == kEndOfFileMarker) {
            // The "reconsume" here deviates from the spec, but is required to avoid consuming past the EOF
            if (cc == kEndOfFileMarker)
                reconsume(cc);
            return CSSParserToken(StringToken, output.toString());
        }
        if (isNewLine(cc)) {
            reconsume(cc);
            return CSSParserToken(BadStringToken);
        }
        if (cc == '\\') {
            if (m_input.nextInputChar() == kEndOfFileMarker)
                continue;
            if (isNewLine(m_input.nextInputChar()))
                consume();
            else
                output.append(consumeEscape());
        } else {
            output.append(cc);
        }
    }
}

void CSSTokenizer::consumeUntilNonWhitespace()
{
    // Using HTML space here rather than CSS space since we don't do preprocessing
    while (isHTMLSpace<UChar>(m_input.nextInputChar()))
        consume();
}

bool CSSTokenizer::consumeUntilCommentEndFound()
{
    UChar c = consume();
    while (true) {
        if (c == kEndOfFileMarker)
            return false;
        if (c != '*') {
            c = consume();
            continue;
        }
        c = consume();
        if (c == '/')
            break;
    }
    return true;
}

bool CSSTokenizer::consumeIfNext(UChar character)
{
    if (m_input.nextInputChar() == character) {
        consume();
        return true;
    }
    return false;
}

// http://www.w3.org/TR/css3-syntax/#consume-a-name
String CSSTokenizer::consumeName()
{
    // FIXME: Is this as efficient as it can be?
    // The possibility of escape chars mandates a copy AFAICT.
    StringBuilder result;
    while (true) {
        UChar cc = consume();
        if (isNameChar(cc)) {
            result.append(cc);
            continue;
        }
        if (twoCharsAreValidEscape(cc, m_input.nextInputChar())) {
            result.append(consumeEscape());
            continue;
        }
        reconsume(cc);
        return result.toString();
    }
}

// http://dev.w3.org/csswg/css-syntax/#consume-an-escaped-code-point
UChar CSSTokenizer::consumeEscape()
{
    UChar cc = consume();
    ASSERT(cc != '\n');
    if (isASCIIHexDigit(cc)) {
        unsigned consumedHexDigits = 1;
        StringBuilder hexChars;
        hexChars.append(cc);
        while (consumedHexDigits < 6 && isASCIIHexDigit(m_input.nextInputChar())) {
            cc = consume();
            hexChars.append(cc);
            consumedHexDigits++;
        };
        bool ok = false;
        UChar codePoint = hexChars.toString().toUIntStrict(&ok, 16);
        if (!ok)
            return WTF::Unicode::replacementCharacter;
        return codePoint;
    }

    // Replaces NULLs with replacement characters, since we do not perform preprocessing
    if (cc == kEndOfFileMarker)
        return WTF::Unicode::replacementCharacter;
    return cc;
}

bool CSSTokenizer::nextTwoCharsAreValidEscape()
{
    if (m_input.leftChars() < 1)
        return false;
    return twoCharsAreValidEscape(m_input.nextInputChar(), m_input.peek(1));
}

// http://www.w3.org/TR/css3-syntax/#starts-with-a-number
bool CSSTokenizer::nextCharsAreNumber(UChar first)
{
    UChar second = m_input.nextInputChar();
    if (isASCIIDigit(first))
        return true;
    if (first == '+' || first == '-')
        return ((isASCIIDigit(second)) || (second == '.' && isASCIIDigit(m_input.peek(1))));
    if (first =='.')
        return (isASCIIDigit(second));
    return false;
}

bool CSSTokenizer::nextCharsAreNumber()
{
    UChar first = consume();
    bool areNumber = nextCharsAreNumber(first);
    reconsume(first);
    return areNumber;
}

// http://www.w3.org/TR/css3-syntax/#would-start-an-identifier
bool CSSTokenizer::nextCharsAreIdentifier(UChar first)
{
    UChar second = m_input.nextInputChar();
    if (isNameStart(first) || twoCharsAreValidEscape(first, second))
        return true;

    if (first == '-') {
        if (isNameStart(m_input.nextInputChar()))
            return true;
        return nextTwoCharsAreValidEscape();
    }

    return false;
}

bool CSSTokenizer::nextCharsAreIdentifier()
{
    UChar first = consume();
    bool areIdentifier = nextCharsAreIdentifier(first);
    reconsume(first);
    return areIdentifier;
}

} // namespace blink
