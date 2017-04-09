// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/editing/CompositionUnderline.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace blink {
namespace {

CompositionUnderline CreateCompositionUnderline(unsigned start_offset,
                                                unsigned end_offset) {
  return CompositionUnderline(start_offset, end_offset, Color::kTransparent,
                              false, Color::kTransparent);
}

TEST(CompositionUnderlineTest, OneChar) {
  CompositionUnderline underline = CreateCompositionUnderline(0, 1);
  EXPECT_EQ(0u, underline.StartOffset());
  EXPECT_EQ(1u, underline.EndOffset());
}

TEST(CompositionUnderlineTest, MultiChar) {
  CompositionUnderline underline = CreateCompositionUnderline(0, 5);
  EXPECT_EQ(0u, underline.StartOffset());
  EXPECT_EQ(5u, underline.EndOffset());
}

TEST(CompositionUnderlineTest, ZeroLength) {
  CompositionUnderline underline = CreateCompositionUnderline(0, 0);
  EXPECT_EQ(0u, underline.StartOffset());
  EXPECT_EQ(1u, underline.EndOffset());
}

TEST(CompositionUnderlineTest, ZeroLengthNonZeroStart) {
  CompositionUnderline underline = CreateCompositionUnderline(3, 3);
  EXPECT_EQ(3u, underline.StartOffset());
  EXPECT_EQ(4u, underline.EndOffset());
}

TEST(CompositionUnderlineTest, EndBeforeStart) {
  CompositionUnderline underline = CreateCompositionUnderline(1, 0);
  EXPECT_EQ(1u, underline.StartOffset());
  EXPECT_EQ(2u, underline.EndOffset());
}

TEST(CompositionUnderlineTest, LastChar) {
  CompositionUnderline underline =
      CreateCompositionUnderline(std::numeric_limits<unsigned>::max() - 1,
                                 std::numeric_limits<unsigned>::max());
  EXPECT_EQ(std::numeric_limits<unsigned>::max() - 1, underline.StartOffset());
  EXPECT_EQ(std::numeric_limits<unsigned>::max(), underline.EndOffset());
}

TEST(CompositionUnderlineTest, LastCharEndBeforeStart) {
  CompositionUnderline underline =
      CreateCompositionUnderline(std::numeric_limits<unsigned>::max(),
                                 std::numeric_limits<unsigned>::max() - 1);
  EXPECT_EQ(std::numeric_limits<unsigned>::max() - 1, underline.StartOffset());
  EXPECT_EQ(std::numeric_limits<unsigned>::max(), underline.EndOffset());
}

TEST(CompositionUnderlineTest, LastCharEndBeforeStartZeroEnd) {
  CompositionUnderline underline =
      CreateCompositionUnderline(std::numeric_limits<unsigned>::max(), 0);
  EXPECT_EQ(std::numeric_limits<unsigned>::max() - 1, underline.StartOffset());
  EXPECT_EQ(std::numeric_limits<unsigned>::max(), underline.EndOffset());
}

}  // namespace
}  // namespace blink
