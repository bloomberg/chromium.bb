// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/parser/CSSParserScopedTokenBuffer.h"
#include "core/css/parser/CSSParserTokenStream.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

namespace {

TEST(CSSParserTokenStreamTest, EmptyStream) {
  CSSTokenizer tokenizer("");
  CSSParserTokenStream stream(tokenizer);
  EXPECT_TRUE(stream.Consume().IsEOF());
  EXPECT_TRUE(stream.Peek().IsEOF());
  EXPECT_TRUE(stream.AtEnd());
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

TEST(CSSParserTokenStreamTest, ConsumeWhitespace) {
  CSSTokenizer tokenizer(" \t\n");  // kWhitespace
  CSSParserTokenStream stream(tokenizer);

  EXPECT_EQ(kWhitespaceToken, stream.Consume().GetType());
  EXPECT_TRUE(stream.AtEnd());
}

TEST(CSSParserTokenStreamTest, ConsumeIncludingWhitespace) {
  CSSTokenizer tokenizer("A \t\n");  // kIdent kWhitespace
  CSSParserTokenStream stream(tokenizer);

  EXPECT_EQ(kIdentToken, stream.ConsumeIncludingWhitespace().GetType());
  EXPECT_TRUE(stream.AtEnd());
}

TEST(CSSParserTokenStreamTest, RangesDoNotGetInvalidatedWhenConsuming) {
  String s = "1 ";
  for (int i = 0; i < 100; i++)
    s.append("A ");

  CSSTokenizer tokenizer(s);
  CSSParserTokenStream stream(tokenizer);

  // Consume a single token range.
  CSSParserScopedTokenBuffer buffer(stream);
  stream.EnsureLookAhead();
  stream.UncheckedConsumeComponentValue(buffer);

  auto range = buffer.Range();
  EXPECT_EQ(kNumberToken, range.Peek().GetType());

  // Consume remaining tokens to try to invalidate the range.
  while (!stream.AtEnd())
    stream.ConsumeIncludingWhitespace();

  EXPECT_EQ(kNumberToken, range.ConsumeIncludingWhitespace().GetType());
  EXPECT_TRUE(range.AtEnd());
}

TEST(CSSParserTokenStreamTest, BlockErrorRecoveryConsumesRestOfBlock) {
  CSSTokenizer tokenizer("{B }1");
  CSSParserTokenStream stream(tokenizer);

  {
    CSSParserTokenStream::BlockGuard guard(stream);
    EXPECT_EQ(kIdentToken, stream.Consume().GetType());
    EXPECT_FALSE(stream.AtEnd());
  }  // calls destructor

  EXPECT_EQ(kNumberToken, stream.Consume().GetType());
}

TEST(CSSParserTokenStreamTest, BlockErrorRecoveryOnSuccess) {
  CSSTokenizer tokenizer("{B }1");
  CSSParserTokenStream stream(tokenizer);

  {
    CSSParserTokenStream::BlockGuard guard(stream);
    EXPECT_EQ(kIdentToken, stream.Consume().GetType());
    EXPECT_EQ(kWhitespaceToken, stream.Consume().GetType());
    EXPECT_TRUE(stream.AtEnd());
  }  // calls destructor

  EXPECT_EQ(kNumberToken, stream.Consume().GetType());
}

TEST(CSSParserTokenStreamTest, BlockErrorRecoveryConsumeComponentValue) {
  CSSTokenizer tokenizer("{{B} C}1");
  CSSParserTokenStream stream(tokenizer);

  {
    CSSParserTokenStream::BlockGuard guard(stream);
    stream.EnsureLookAhead();
    stream.UncheckedConsumeComponentValue();
  }  // calls destructor

  EXPECT_EQ(kNumberToken, stream.Consume().GetType());
}

TEST(CSSParserTokenStreamTest, OffsetAfterPeek) {
  CSSTokenizer tokenizer("ABC");
  CSSParserTokenStream stream(tokenizer);

  EXPECT_EQ(0U, stream.Offset());
  EXPECT_EQ(kIdentToken, stream.Peek().GetType());
  EXPECT_EQ(0U, stream.Offset());
}

TEST(CSSParserTokenStreamTest, OffsetAfterConsumes) {
  CSSTokenizer tokenizer("ABC 1 {23 }");
  CSSParserTokenStream stream(tokenizer);

  EXPECT_EQ(0U, stream.Offset());
  EXPECT_EQ(kIdentToken, stream.Consume().GetType());
  EXPECT_EQ(3U, stream.Offset());
  EXPECT_EQ(kWhitespaceToken, stream.Consume().GetType());
  EXPECT_EQ(4U, stream.Offset());
  EXPECT_EQ(kNumberToken, stream.ConsumeIncludingWhitespace().GetType());
  EXPECT_EQ(6U, stream.Offset());
  stream.EnsureLookAhead();
  stream.UncheckedConsumeComponentValue();
  EXPECT_EQ(11U, stream.Offset());
}

TEST(CSSParserTokenStreamTest, LookAheadOffset) {
  CSSTokenizer tokenizer("ABC/* *//* */1");
  CSSParserTokenStream stream(tokenizer);

  stream.EnsureLookAhead();
  EXPECT_EQ(0U, stream.Offset());
  EXPECT_EQ(0U, stream.LookAheadOffset());
  EXPECT_EQ(kIdentToken, stream.Consume().GetType());

  stream.EnsureLookAhead();
  EXPECT_EQ(3U, stream.Offset());
  EXPECT_EQ(13U, stream.LookAheadOffset());
}

TEST(CSSParserTokenStreamTest, ConsumeComponentValueWithBuffer) {
  CSSTokenizer tokenizer("{23 }");
  CSSParserTokenStream stream(tokenizer);

  CSSParserScopedTokenBuffer buffer(stream);
  stream.EnsureLookAhead();
  stream.UncheckedConsumeComponentValue(buffer);
  EXPECT_TRUE(stream.AtEnd());

  auto range = buffer.Range();
  EXPECT_EQ(kLeftBraceToken, range.Consume().GetType());
  EXPECT_EQ(kNumberToken, range.Consume().GetType());
  EXPECT_EQ(kWhitespaceToken, range.Consume().GetType());
  EXPECT_EQ(kRightBraceToken, range.Consume().GetType());
  EXPECT_TRUE(range.AtEnd());
}

}  // namespace

}  // namespace blink
