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

} // namespace
