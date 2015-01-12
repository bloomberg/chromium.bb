// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/accessibility/ax_text_utils.h"

namespace ui {

TEST(AXTextUtils, FindAccessibleTextBoundaryLine) {
  const base::string16 text = base::UTF8ToUTF16("Line 1.\nLine 2\n");
  const size_t text_length = text.length();
  std::vector<int> line_breaks;
  line_breaks.push_back(7);
  line_breaks.push_back(14);
  size_t result;


  // Basic cases.
  result = FindAccessibleTextBoundary(text, line_breaks, LINE_BOUNDARY, 5,
                                      FORWARDS_DIRECTION);
  EXPECT_EQ(7UL, result);
  result = FindAccessibleTextBoundary(text, line_breaks, LINE_BOUNDARY, 9,
                                      BACKWARDS_DIRECTION);
  EXPECT_EQ(7UL, result);
  result = FindAccessibleTextBoundary(text, line_breaks, LINE_BOUNDARY, 10,
                                      FORWARDS_DIRECTION);
  EXPECT_EQ(14UL, result);


  // Edge cases.

  result = FindAccessibleTextBoundary(text, line_breaks, LINE_BOUNDARY,
                                      text_length, BACKWARDS_DIRECTION);
  EXPECT_EQ(14UL, result);

  // When the start_offset is on a line break and we are searching backwards,
  // it should return the previous line break.
  result = FindAccessibleTextBoundary(text, line_breaks, LINE_BOUNDARY, 14,
                                      BACKWARDS_DIRECTION);
  EXPECT_EQ(7UL, result);

  // When the start_offset is on a line break and we are searching forwards,
  // it should return the next line break.
  result = FindAccessibleTextBoundary(text, line_breaks, LINE_BOUNDARY, 7,
                                      FORWARDS_DIRECTION);
  EXPECT_EQ(14UL, result);

  // When there is no previous line break and we are searching backwards,
  // it should return 0.
  result = FindAccessibleTextBoundary(text, line_breaks, LINE_BOUNDARY, 4,
                                      BACKWARDS_DIRECTION);
  EXPECT_EQ(0UL, result);

  // When we are on the last line break and we are searching forwards.
  // it should return the text length.
  result = FindAccessibleTextBoundary(text, line_breaks, LINE_BOUNDARY, 14,
                                      FORWARDS_DIRECTION);
  EXPECT_EQ(text_length, result);
}

} // Namespace ui.
