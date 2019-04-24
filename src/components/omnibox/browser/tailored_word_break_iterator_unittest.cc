// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include "base/i18n/break_iterator.h"
#include "base/strings/utf_string_conversions.h"
#include "components/omnibox/browser/tailored_word_break_iterator.h"
#include "testing/gtest/include/gtest/gtest.h"

TEST(TailoredWordBreakIterator, BreakWord) {
  base::string16 underscore(base::UTF8ToUTF16("_"));
  base::string16 str(base::UTF8ToUTF16("_foo_bar!_\npouet_boom"));
  TailoredWordBreakIterator iter(str, TailoredWordBreakIterator::BREAK_WORD);
  ASSERT_TRUE(iter.Init());
  EXPECT_TRUE(iter.Advance());
  EXPECT_FALSE(iter.IsWord());
  EXPECT_EQ(underscore, iter.GetString());
  EXPECT_TRUE(iter.Advance());
  EXPECT_TRUE(iter.IsWord());
  EXPECT_EQ(base::UTF8ToUTF16("foo"), iter.GetString());
  EXPECT_TRUE(iter.Advance());
  EXPECT_FALSE(iter.IsWord());
  EXPECT_EQ(underscore, iter.GetString());
  EXPECT_TRUE(iter.Advance());
  EXPECT_TRUE(iter.IsWord());
  EXPECT_EQ(base::UTF8ToUTF16("bar"), iter.GetString());
  EXPECT_TRUE(iter.Advance());
  EXPECT_FALSE(iter.IsWord());
  EXPECT_EQ(base::UTF8ToUTF16("!"), iter.GetString());
  EXPECT_TRUE(iter.Advance());
  EXPECT_FALSE(iter.IsWord());
  EXPECT_EQ(underscore, iter.GetString());
  EXPECT_TRUE(iter.Advance());
  EXPECT_FALSE(iter.IsWord());
  EXPECT_EQ(base::UTF8ToUTF16("\n"), iter.GetString());
  EXPECT_TRUE(iter.Advance());
  EXPECT_TRUE(iter.IsWord());
  EXPECT_EQ(base::UTF8ToUTF16("pouet"), iter.GetString());
  EXPECT_TRUE(iter.Advance());
  EXPECT_FALSE(iter.IsWord());
  EXPECT_EQ(underscore, iter.GetString());
  EXPECT_TRUE(iter.Advance());
  EXPECT_TRUE(iter.IsWord());
  EXPECT_EQ(base::UTF8ToUTF16("boom"), iter.GetString());
  EXPECT_FALSE(iter.Advance());
  EXPECT_FALSE(iter.IsWord());
  EXPECT_FALSE(iter.Advance());
  EXPECT_FALSE(iter.IsWord());
}
