// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gtest/include/gtest/gtest.h"
#include "tools/gn/c_include_iterator.h"

TEST(CIncludeIterator, Basic) {
  std::string buffer;
  buffer.append("// Some comment\n");
  buffer.append("\n");
  buffer.append("#include \"foo/bar.h\"\n");
  buffer.append("\n");
  buffer.append("#include <stdio.h>\n");
  buffer.append("\n");
  buffer.append(" #include \"foo/baz.h\"\n");  // Leading whitespace
  buffer.append("#include \"la/deda.h\"\n");
  buffer.append("#import \"weird_mac_import.h\"\n");
  buffer.append("\n");
  buffer.append("void SomeCode() {\n");

  CIncludeIterator iter(buffer);

  base::StringPiece contents;
  EXPECT_TRUE(iter.GetNextIncludeString(&contents));
  EXPECT_EQ("foo/bar.h", contents);
  EXPECT_TRUE(iter.GetNextIncludeString(&contents));
  EXPECT_EQ("foo/baz.h", contents);
  EXPECT_TRUE(iter.GetNextIncludeString(&contents));
  EXPECT_EQ("la/deda.h", contents);
  EXPECT_TRUE(iter.GetNextIncludeString(&contents));
  EXPECT_EQ("weird_mac_import.h", contents);
  EXPECT_FALSE(iter.GetNextIncludeString(&contents));
}

// Tests that we don't search for includes indefinitely.
TEST(CIncludeIterator, GiveUp) {
  std::string buffer;
  for (size_t i = 0; i < 1000; i++)
    buffer.append("x\n");
  buffer.append("#include \"foo/bar.h\"\n");

  base::StringPiece contents;

  CIncludeIterator iter(buffer);
  EXPECT_FALSE(iter.GetNextIncludeString(&contents));
  EXPECT_TRUE(contents.empty());
}

// Don't count blank lines, comments, and preprocessor when giving up.
TEST(CIncludeIterator, DontGiveUp) {
  std::string buffer;
  for (size_t i = 0; i < 1000; i++)
    buffer.push_back('\n');
  for (size_t i = 0; i < 1000; i++)
    buffer.append("// comment\n");
  for (size_t i = 0; i < 1000; i++)
    buffer.append("#preproc\n");
  buffer.append("#include \"foo/bar.h\"\n");

  base::StringPiece contents;

  CIncludeIterator iter(buffer);
  EXPECT_TRUE(iter.GetNextIncludeString(&contents));
  EXPECT_EQ("foo/bar.h", contents);
}

// Tests that we'll tolerate some small numbers of non-includes interspersed
// with real includes.
TEST(CIncludeIterator, TolerateNonIncludes) {
  const size_t kSkip = CIncludeIterator::kMaxNonIncludeLines - 2;
  const size_t kGroupCount = 100;

  std::string include("foo/bar.h");

  // Allow a series of includes with blanks in between.
  std::string buffer;
  for (size_t group = 0; group < kGroupCount; group++) {
    for (size_t i = 0; i < kSkip; i++)
      buffer.append("foo\n");
    buffer.append("#include \"" + include + "\"\n");
  }

  base::StringPiece contents;

  CIncludeIterator iter(buffer);
  for (size_t group = 0; group < kGroupCount; group++) {
    EXPECT_TRUE(iter.GetNextIncludeString(&contents));
    EXPECT_EQ(include, contents.as_string());
  }
  EXPECT_FALSE(iter.GetNextIncludeString(&contents));
}
