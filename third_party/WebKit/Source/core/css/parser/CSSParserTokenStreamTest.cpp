// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/parser/CSSParserTokenStream.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

namespace {

// Parameterized tests for whether we are constructing a stream from
// an empty or exhausted tokenizer.
// TODO(shend): Remove this once we no longer need to construct streams
// from exhausted tokenizers.
class CSSParserTokenStreamTest : public ::testing::TestWithParam<bool> {};

CSSParserTokenStream GetStream(CSSTokenizer& tokenizer,
                               bool should_tokenize_to_end) {
  if (should_tokenize_to_end)
    tokenizer.TokenRange();
  return CSSParserTokenStream(tokenizer);
}

TEST_P(CSSParserTokenStreamTest, EmptyStream) {
  CSSTokenizer tokenizer("");
  auto stream = GetStream(tokenizer, GetParam());
  EXPECT_TRUE(stream.Consume().IsEOF());
  EXPECT_TRUE(stream.Peek().IsEOF());
  EXPECT_TRUE(stream.AtEnd());
  EXPECT_TRUE(stream.MakeRangeToEOF().AtEnd());
}

TEST_P(CSSParserTokenStreamTest, PeekThenConsume) {
  CSSTokenizer tokenizer("A");  // kIdent
  auto stream = GetStream(tokenizer, GetParam());
  EXPECT_EQ(kIdentToken, stream.Peek().GetType());
  EXPECT_EQ(kIdentToken, stream.Consume().GetType());
  EXPECT_TRUE(stream.AtEnd());
}

TEST_P(CSSParserTokenStreamTest, ConsumeThenPeek) {
  CSSTokenizer tokenizer("A");  // kIdent
  auto stream = GetStream(tokenizer, GetParam());
  EXPECT_EQ(kIdentToken, stream.Consume().GetType());
  EXPECT_TRUE(stream.AtEnd());
}

TEST_P(CSSParserTokenStreamTest, ConsumeMultipleTokens) {
  CSSTokenizer tokenizer("A 1");  // kIdent kWhitespace kNumber
  auto stream = GetStream(tokenizer, GetParam());
  EXPECT_EQ(kIdentToken, stream.Consume().GetType());
  EXPECT_EQ(kWhitespaceToken, stream.Consume().GetType());
  EXPECT_EQ(kNumberToken, stream.Consume().GetType());
  EXPECT_TRUE(stream.AtEnd());
}

TEST_P(CSSParserTokenStreamTest, UncheckedPeekAndConsumeAfterPeek) {
  CSSTokenizer tokenizer("A");  // kIdent
  auto stream = GetStream(tokenizer, GetParam());
  EXPECT_EQ(kIdentToken, stream.Peek().GetType());
  EXPECT_EQ(kIdentToken, stream.UncheckedPeek().GetType());
  EXPECT_EQ(kIdentToken, stream.UncheckedConsume().GetType());
  EXPECT_TRUE(stream.AtEnd());
}

TEST_P(CSSParserTokenStreamTest, UncheckedPeekAndConsumeAfterAtEnd) {
  CSSTokenizer tokenizer("A");  // kIdent
  auto stream = GetStream(tokenizer, GetParam());
  EXPECT_FALSE(stream.AtEnd());
  EXPECT_EQ(kIdentToken, stream.UncheckedPeek().GetType());
  EXPECT_EQ(kIdentToken, stream.UncheckedConsume().GetType());
  EXPECT_TRUE(stream.AtEnd());
}

TEST_P(CSSParserTokenStreamTest, MakeRangeToEOF) {
  CSSTokenizer tokenizer("A 1");  // kIdent kWhitespace kNumber
  auto stream = GetStream(tokenizer, GetParam());
  EXPECT_EQ(kIdentToken, stream.Consume().GetType());

  auto range = stream.MakeRangeToEOF();
  EXPECT_FALSE(stream.AtEnd());
  EXPECT_EQ(kWhitespaceToken, range.Consume().GetType());
  EXPECT_FALSE(stream.AtEnd());
  EXPECT_EQ(kNumberToken, range.Consume().GetType());
  EXPECT_TRUE(range.AtEnd());

  EXPECT_FALSE(stream.AtEnd());
}

TEST_P(CSSParserTokenStreamTest, MakeLargeRangeToEOF) {
  String s = "";
  for (int i = 0; i < 100; i++)
    s.append("A ");

  CSSTokenizer tokenizer(s);
  CSSParserTokenStream stream(tokenizer);

  auto range = stream.MakeRangeToEOF();
  while (!range.AtEnd()) {
    EXPECT_EQ(kIdentToken, range.Consume().GetType());
    EXPECT_EQ(kWhitespaceToken, range.Consume().GetType());
  }

  EXPECT_FALSE(stream.AtEnd());
}

TEST_P(CSSParserTokenStreamTest, UncheckedConsumeComponentValue) {
  CSSTokenizer tokenizer("A{1}{2{3}}B");
  auto stream = GetStream(tokenizer, GetParam());

  EXPECT_EQ(kIdentToken, stream.Peek().GetType());
  stream.UncheckedConsumeComponentValue();
  EXPECT_EQ(kLeftBraceToken, stream.Peek().GetType());
  stream.UncheckedConsumeComponentValue();
  EXPECT_EQ(kLeftBraceToken, stream.Peek().GetType());
  stream.UncheckedConsumeComponentValue();
  EXPECT_EQ(kIdentToken, stream.Peek().GetType());
  stream.UncheckedConsumeComponentValue();

  EXPECT_TRUE(stream.AtEnd());
}

INSTANTIATE_TEST_CASE_P(ShouldTokenizeToEnd,
                        CSSParserTokenStreamTest,
                        ::testing::Bool());

}  // namespace

}  // namespace blink
