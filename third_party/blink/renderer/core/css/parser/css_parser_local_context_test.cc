// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/parser/css_parser_local_context.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

TEST(CSSParserLocalContextTest, Constructor) {
  EXPECT_FALSE(CSSParserLocalContext().UseAliasParsing());
  EXPECT_EQ(CSSPropertyInvalid, CSSParserLocalContext().CurrentShorthand());
}

TEST(CSSParserLocalContextTest, WithAliasParsing) {
  const CSSParserLocalContext context;
  EXPECT_FALSE(context.WithAliasParsing(false).UseAliasParsing());
  EXPECT_TRUE(context.WithAliasParsing(true).UseAliasParsing());
}

TEST(CSSParserLocalContextTest, WithCurrentShorthand) {
  const CSSParserLocalContext context;
  const CSSPropertyID shorthand = CSSPropertyBackground;
  EXPECT_EQ(shorthand,
            context.WithCurrentShorthand(shorthand).CurrentShorthand());
}

TEST(CSSParserLocalContextTest, LocalMutation) {
  CSSParserLocalContext context;
  context = context.WithAliasParsing(true);
  context = context.WithCurrentShorthand(CSSPropertyBackground);

  // WithAliasParsing only changes that member.
  EXPECT_EQ(CSSPropertyBackground,
            context.WithAliasParsing(false).CurrentShorthand());

  // WithCurrentShorthand only changes that member.
  EXPECT_TRUE(
      context.WithCurrentShorthand(CSSPropertyInvalid).UseAliasParsing());
}

}  // namespace blink
