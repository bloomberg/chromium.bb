// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/css/parser/MediaQueryTokenizer.h"

#include "wtf/PassOwnPtr.h"
#include <gtest/gtest.h>

namespace WebCore {

typedef pair<String, MediaQueryTokenType* > TestCase;
TEST(MediaQueryTokenizerTest, Basic)
{
    Vector<TestCase> testcases;
    MediaQueryTokenType tokenTypeArr[] = {LeftParenthesisToken, IdentToken, ColonToken, WhitespaceToken, PercentageToken, RightParenthesisToken, EOFToken };
    TestCase testCase1("(max-width: 50%)", (MediaQueryTokenType*)&tokenTypeArr);
    testcases.append(testCase1);
    Vector<MediaQueryToken> tokens;
    MediaQueryTokenizer::tokenize(testcases[0].first, tokens);
    for (size_t i = 0; i < tokens.size(); i++) {
        ASSERT_EQ(testcases[0].second[i], tokens[i].type());
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
