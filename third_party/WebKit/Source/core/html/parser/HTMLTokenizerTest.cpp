// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/html/parser/HTMLTokenizer.h"

#include "core/html/parser/HTMLParserOptions.h"
#include "core/html/parser/HTMLToken.h"
#include "testing/gtest/include/gtest/gtest.h"

#include <memory>

namespace blink {

// This is a regression test for crbug.com/619141
TEST(HTMLTokenizerTest, ZeroOffsetAttributeNameRange) {
  HTMLParserOptions options;
  std::unique_ptr<HTMLTokenizer> tokenizer = HTMLTokenizer::Create(options);
  HTMLToken token;

  SegmentedString input("<script ");
  EXPECT_FALSE(tokenizer->NextToken(input, token));

  EXPECT_EQ(HTMLToken::kStartTag, token.GetType());

  SegmentedString input2("type='javascript'");
  // Below should not fail ASSERT
  EXPECT_FALSE(tokenizer->NextToken(input2, token));
}

}  // namespace blink
