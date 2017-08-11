// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/parser/CSSParserTokenStream.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

TEST(CSSParserTokenStreamTest, EmptyStream) {
  CSSTokenizer tokenizer("");
  CSSParserTokenStream stream(tokenizer);
  EXPECT_TRUE(stream.Consume().IsEOF());
  EXPECT_TRUE(stream.Peek().IsEOF());
  EXPECT_TRUE(stream.AtEnd());
  EXPECT_TRUE(stream.MakeRangeToEOF().AtEnd());
}

TEST(CSSParserTokenStreamTest, PeekThenConsume) {
  CSSTokenizer tokenizer("A");  // kIdent
  CSSParserTokenStream stream(tokenizer);
  EXPECT_EQ(kIdentToken, stream.Peek().GetType());
  EXPECT_EQ(kIdentToken, stream.Consume().GetType());
  EXPECT_TRUE(stream.AtEnd());
}

TEST(CSSParserTokenStreamTest, ConsumeThenPeek) {
  CSSTokenizer tokenizer("A");  // kIdent
  CSSParserTokenStream stream(tokenizer);
  EXPECT_EQ(kIdentToken, stream.Consume().GetType());
  EXPECT_TRUE(stream.AtEnd());
}

TEST(CSSParserTokenStreamTest, ConsumeMultipleTokens) {
  CSSTokenizer tokenizer("A 1");  // kIdent kWhitespace kNumber
  CSSParserTokenStream stream(tokenizer);
  EXPECT_EQ(kIdentToken, stream.Consume().GetType());
  EXPECT_EQ(kWhitespaceToken, stream.Consume().GetType());
  EXPECT_EQ(kNumberToken, stream.Consume().GetType());
  EXPECT_TRUE(stream.AtEnd());
}

TEST(CSSParserTokenStreamTest, UncheckedPeekAndConsumeAfterPeek) {
  CSSTokenizer tokenizer("A");  // kIdent
  CSSParserTokenStream stream(tokenizer);
  EXPECT_EQ(kIdentToken, stream.Peek().GetType());
  EXPECT_EQ(kIdentToken, stream.UncheckedPeek().GetType());
  EXPECT_EQ(kIdentToken, stream.UncheckedConsume().GetType());
  EXPECT_TRUE(stream.AtEnd());
}

TEST(CSSParserTokenStreamTest, UncheckedPeekAndConsumeAfterAtEnd) {
  CSSTokenizer tokenizer("A");  // kIdent
  CSSParserTokenStream stream(tokenizer);
  EXPECT_FALSE(stream.AtEnd());
  EXPECT_EQ(kIdentToken, stream.UncheckedPeek().GetType());
  EXPECT_EQ(kIdentToken, stream.UncheckedConsume().GetType());
  EXPECT_TRUE(stream.AtEnd());
}

TEST(CSSParserTokenStreamTest, MakeRangeToEOF) {
  CSSTokenizer tokenizer("A 1");  // kIdent kWhitespace kNumber
  CSSParserTokenStream stream(tokenizer);
  EXPECT_EQ(kIdentToken, stream.Consume().GetType());

  auto range = stream.MakeRangeToEOF();
  EXPECT_FALSE(stream.AtEnd());
  EXPECT_EQ(kWhitespaceToken, range.Consume().GetType());
  EXPECT_FALSE(stream.AtEnd());
  EXPECT_EQ(kNumberToken, range.Consume().GetType());
  EXPECT_TRUE(range.AtEnd());

  EXPECT_FALSE(stream.AtEnd());
}

TEST(CSSParserTokenStreamTest, UncheckedConsumeComponentValue) {
  CSSTokenizer tokenizer("A{1}{2{3}}B");
  CSSParserTokenStream stream(tokenizer);

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

}  // namespace blink
