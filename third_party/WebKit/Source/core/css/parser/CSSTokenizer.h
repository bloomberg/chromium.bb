// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSTokenizer_h
#define CSSTokenizer_h

#include "core/css/parser/CSSParserToken.h"
#include "core/html/parser/InputStreamPreprocessor.h"
#include "wtf/text/WTFString.h"

#include <climits>

namespace blink {

class CSSTokenizerInputStream;

class CSSTokenizer {
    WTF_MAKE_NONCOPYABLE(CSSTokenizer);
    WTF_MAKE_FAST_ALLOCATED;
public:
    static void tokenize(String, Vector<CSSParserToken>&);
private:
    CSSTokenizer(CSSTokenizerInputStream&);

    CSSParserToken nextToken();

    UChar consume();
    void consume(unsigned);
    void reconsume(UChar);

    CSSParserToken consumeNumericToken();
    CSSParserToken consumeIdentLikeToken();
    CSSParserToken consumeNumber();
    CSSParserToken consumeStringTokenUntil(UChar);

    void consumeUntilNonWhitespace();
    bool consumeUntilCommentEndFound();

    bool consumeIfNext(UChar);
    String consumeName();
    UChar consumeEscape();

    bool nextTwoCharsAreValidEscape();
    bool nextCharsAreNumber(UChar);
    bool nextCharsAreNumber();
    bool nextCharsAreIdentifier(UChar);
    bool nextCharsAreIdentifier();
    CSSParserToken blockStart(CSSParserTokenType);
    CSSParserToken blockStart(CSSParserTokenType blockType, CSSParserTokenType, String);
    CSSParserToken blockEnd(CSSParserTokenType, CSSParserTokenType startType);

    typedef CSSParserToken (CSSTokenizer::*CodePoint)(UChar);

    static const CodePoint codePoints[];
    Vector<CSSParserTokenType> m_blockStack;

    CSSParserToken whiteSpace(UChar);
    CSSParserToken leftParenthesis(UChar);
    CSSParserToken rightParenthesis(UChar);
    CSSParserToken leftBracket(UChar);
    CSSParserToken rightBracket(UChar);
    CSSParserToken leftBrace(UChar);
    CSSParserToken rightBrace(UChar);
    CSSParserToken plusOrFullStop(UChar);
    CSSParserToken comma(UChar);
    CSSParserToken hyphenMinus(UChar);
    CSSParserToken asterisk(UChar);
    CSSParserToken solidus(UChar);
    CSSParserToken colon(UChar);
    CSSParserToken semiColon(UChar);
    CSSParserToken reverseSolidus(UChar);
    CSSParserToken asciiDigit(UChar);
    CSSParserToken nameStart(UChar);
    CSSParserToken stringStart(UChar);
    CSSParserToken endOfFile(UChar);

    CSSTokenizerInputStream& m_input;
};



} // namespace blink

#endif // CSSTokenizer_h
