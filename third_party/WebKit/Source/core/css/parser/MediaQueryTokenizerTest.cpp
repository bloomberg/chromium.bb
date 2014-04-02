// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/css/parser/MediaQueryTokenizer.h"

#include "wtf/PassOwnPtr.h"
#include <gtest/gtest.h>

namespace WebCore {

typedef struct {
    const char* input;
    const char* output;
} TestCase;

TEST(MediaQueryTokenizerTest, Basic)
{
    TestCase testCases[] = {
        { "(max-width: 50px)", "(max-width: 50px)" },
        { "(max-width: 50\\70\\78)", "(max-width: 50px)" },
        { "(max-width: /* comment */50px)", "(max-width: 50px)" },
        { "(max-width: /** *commen*t */60px)", "(max-width: 60px)" },
        { "(max-width: /** *commen*t **/70px)", "(max-width: 70px)" },
        { "(max-width: /** *commen*t **//**/80px)", "(max-width: 80px)" },
        { "(max-width: /*/ **/90px)", "(max-width: 90px)" },
        { "(max-width: /*/ **/*100px)", "(max-width: *100px)" },
        { "(max-width: 110px/*)", "(max-width: 110px" },
        { "(max-width: 120px)/*", "(max-width: 120px)" },
        { "(max-width: 130px)/**", "(max-width: 130px)" },
        { "(max-width: /***/140px)/**/", "(max-width: 140px)" },
        { 0, 0 } // Do not remove the terminator line.
    };

    for (int i = 0; testCases[i].input; ++i) {
        Vector<MediaQueryToken> tokens;
        MediaQueryTokenizer::tokenize(testCases[i].input, tokens);
        StringBuilder output;
        for (size_t j = 0; j < tokens.size(); ++j)
            output.append(tokens[j].textForUnitTests());
        ASSERT_STREQ(testCases[i].output, output.toString().ascii().data());
    }
}

void testToken(UChar c, MediaQueryTokenType tokenType)
{
    Vector<MediaQueryToken> tokens;
    StringBuilder input;
    input.append(c);
    MediaQueryTokenizer::tokenize(input.toString(), tokens);
    ASSERT_EQ(tokens[0].type(), tokenType);
}

TEST(MediaQueryTokenizerCodepointsTest, Basic)
{
    for (UChar c = 0; c <= 1000; ++c) {
        if (isASCIIDigit(c))
            testToken(c, NumberToken);
        else if (isASCIIAlpha(c))
            testToken(c, IdentToken);
        else if (c == '_')
            testToken(c, IdentToken);
        else if (c == '\r' || c == ' ' || c == '\n' || c == '\t' || c == '\f')
            testToken(c, WhitespaceToken);
        else if (c == '(')
            testToken(c, LeftParenthesisToken);
        else if (c == ')')
            testToken(c, RightParenthesisToken);
        else if (c == '[')
            testToken(c, LeftBracketToken);
        else if (c == ']')
            testToken(c, RightBracketToken);
        else if (c == '{')
            testToken(c, LeftBraceToken);
        else if (c == '}')
            testToken(c, RightBraceToken);
        else if (c == '.' || c == '+' || c == '-' || c == '/' || c == '\\')
            testToken(c, DelimiterToken);
        else if (c == ',')
            testToken(c, CommaToken);
        else if (c == ':')
            testToken(c, ColonToken);
        else if (c == ';')
            testToken(c, SemicolonToken);
        else if (!c)
            testToken(c, EOFToken);
        else if (c > SCHAR_MAX)
            testToken(c, IdentToken);
        else
            testToken(c, DelimiterToken);
    }
    testToken(USHRT_MAX, IdentToken);
}

} // namespace
