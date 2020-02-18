// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/string_matching/term_break_iterator.h"

#include "base/strings/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::UTF8ToUTF16;

namespace {

TEST(TermBreakIteratorTest, EmptyWord) {
  base::string16 empty;
  TermBreakIterator iter(empty);
  EXPECT_FALSE(iter.Advance());
}

TEST(TermBreakIteratorTest, Simple) {
  base::string16 word(UTF8ToUTF16("simple"));
  TermBreakIterator iter(word);
  EXPECT_TRUE(iter.Advance());
  EXPECT_EQ(UTF8ToUTF16("simple"), iter.GetCurrentTerm());
  EXPECT_FALSE(iter.Advance());  // Test unexpected advance after end.
}

TEST(TermBreakIteratorTest, CamelCase) {
  base::string16 word(UTF8ToUTF16("CamelCase"));
  TermBreakIterator iter(word);
  EXPECT_TRUE(iter.Advance());
  EXPECT_EQ(UTF8ToUTF16("Camel"), iter.GetCurrentTerm());
  EXPECT_TRUE(iter.Advance());
  EXPECT_EQ(UTF8ToUTF16("Case"), iter.GetCurrentTerm());
  EXPECT_FALSE(iter.Advance());  // Test unexpected advance after end.
}

TEST(TermBreakIteratorTest, LowerToUpper) {
  base::string16 word(UTF8ToUTF16("lowerToUpper"));
  TermBreakIterator iter(word);
  EXPECT_TRUE(iter.Advance());
  EXPECT_EQ(UTF8ToUTF16("lower"), iter.GetCurrentTerm());
  EXPECT_TRUE(iter.Advance());
  EXPECT_EQ(UTF8ToUTF16("To"), iter.GetCurrentTerm());
  EXPECT_TRUE(iter.Advance());
  EXPECT_EQ(UTF8ToUTF16("Upper"), iter.GetCurrentTerm());
  EXPECT_FALSE(iter.Advance());  // Test unexpected advance after end.
}

TEST(TermBreakIteratorTest, AlphaNumber) {
  base::string16 word(UTF8ToUTF16("Chromium26.0.0.0"));
  TermBreakIterator iter(word);
  EXPECT_TRUE(iter.Advance());
  EXPECT_EQ(UTF8ToUTF16("Chromium"), iter.GetCurrentTerm());
  EXPECT_TRUE(iter.Advance());
  EXPECT_EQ(UTF8ToUTF16("26.0.0.0"), iter.GetCurrentTerm());
  EXPECT_FALSE(iter.Advance());  // Test unexpected advance after end.
}

TEST(TermBreakIteratorTest, StartsWithNumber) {
  base::string16 word(UTF8ToUTF16("123startWithNumber"));
  TermBreakIterator iter(word);
  EXPECT_TRUE(iter.Advance());
  EXPECT_EQ(UTF8ToUTF16("123"), iter.GetCurrentTerm());
  EXPECT_TRUE(iter.Advance());
  EXPECT_EQ(UTF8ToUTF16("start"), iter.GetCurrentTerm());
  EXPECT_TRUE(iter.Advance());
  EXPECT_EQ(UTF8ToUTF16("With"), iter.GetCurrentTerm());
  EXPECT_TRUE(iter.Advance());
  EXPECT_EQ(UTF8ToUTF16("Number"), iter.GetCurrentTerm());
  EXPECT_FALSE(iter.Advance());  // Test unexpected advance after end.
}

TEST(TermBreakIteratorTest, CaseAndNoCase) {
  // "English" + two Chinese chars U+4E2D U+6587 + "Word"
  base::string16 word(UTF8ToUTF16("English\xe4\xb8\xad\xe6\x96\x87Word"));
  TermBreakIterator iter(word);
  EXPECT_TRUE(iter.Advance());
  EXPECT_EQ(UTF8ToUTF16("English"), iter.GetCurrentTerm());
  EXPECT_TRUE(iter.Advance());
  EXPECT_EQ(UTF8ToUTF16("\xe4\xb8\xad\xe6\x96\x87"), iter.GetCurrentTerm());
  EXPECT_TRUE(iter.Advance());
  EXPECT_EQ(UTF8ToUTF16("Word"), iter.GetCurrentTerm());
  EXPECT_FALSE(iter.Advance());  // Test unexpected advance after end.
}

}  // namespace
