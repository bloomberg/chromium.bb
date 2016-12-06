// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/html/parser/HTMLTreeBuilderSimulator.h"

#include "core/html/parser/CompactHTMLToken.h"
#include "core/html/parser/HTMLToken.h"
#include "core/html/parser/HTMLTokenizer.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

TEST(HTMLTreeBuilderSimulatorTest, SelfClosingSVGFollowedByScript) {
  HTMLParserOptions options;
  HTMLTreeBuilderSimulator simulator(options);
  std::unique_ptr<HTMLTokenizer> tokenizer = HTMLTokenizer::create(options);
  SegmentedString input("<svg/><script></script>");
  HTMLToken token;
  EXPECT_TRUE(tokenizer->nextToken(input, token));
  EXPECT_EQ(HTMLTreeBuilderSimulator::OtherToken,
            simulator.simulate(CompactHTMLToken(&token, TextPosition()),
                               tokenizer.get()));

  token.clear();
  EXPECT_TRUE(tokenizer->nextToken(input, token));
  EXPECT_EQ(HTMLTreeBuilderSimulator::ScriptStart,
            simulator.simulate(CompactHTMLToken(&token, TextPosition()),
                               tokenizer.get()));

  EXPECT_EQ(HTMLTokenizer::ScriptDataState, tokenizer->getState());

  token.clear();
  EXPECT_TRUE(tokenizer->nextToken(input, token));
  EXPECT_EQ(HTMLTreeBuilderSimulator::ScriptEnd,
            simulator.simulate(CompactHTMLToken(&token, TextPosition()),
                               tokenizer.get()));
}

TEST(HTMLTreeBuilderSimulatorTest, SelfClosingMathFollowedByScript) {
  HTMLParserOptions options;
  HTMLTreeBuilderSimulator simulator(options);
  std::unique_ptr<HTMLTokenizer> tokenizer = HTMLTokenizer::create(options);
  SegmentedString input("<math/><script></script>");
  HTMLToken token;
  EXPECT_TRUE(tokenizer->nextToken(input, token));
  EXPECT_EQ(HTMLTreeBuilderSimulator::OtherToken,
            simulator.simulate(CompactHTMLToken(&token, TextPosition()),
                               tokenizer.get()));

  token.clear();
  EXPECT_TRUE(tokenizer->nextToken(input, token));
  EXPECT_EQ(HTMLTreeBuilderSimulator::ScriptStart,
            simulator.simulate(CompactHTMLToken(&token, TextPosition()),
                               tokenizer.get()));

  EXPECT_EQ(HTMLTokenizer::ScriptDataState, tokenizer->getState());

  token.clear();
  EXPECT_TRUE(tokenizer->nextToken(input, token));
  EXPECT_EQ(HTMLTreeBuilderSimulator::ScriptEnd,
            simulator.simulate(CompactHTMLToken(&token, TextPosition()),
                               tokenizer.get()));
}

}  // namespace blink
