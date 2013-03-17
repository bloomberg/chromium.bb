// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/message_center/views/bounded_label.h"

#include <limits>

#include "base/string_util.h"
#include "base/strings/string_split.h"
#include "base/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/font.h"
#include "ui/views/controls/label.h"

namespace message_center {

/* Test fixture declaration ***************************************************/

class BoundedLabelTest : public testing::Test {
 public:
  BoundedLabelTest();
  virtual ~BoundedLabelTest();

  // Replaces all occurences of three periods ("...") in the specified string
  // with an ellipses character (UTF8 "\xE2\x80\xA6") and returns a string16
  // with the results. This allows test strings to be specified as ASCII const
  // char* strings, making tests more readable and easier to write.
  string16 ToString(const char* string);

  // Converts the specified elision width to pixels. To make tests somewhat
  // independent of the fonts of the platform on which they're run, the elision
  // widths are specified as XYZ integers, with the corresponding width in
  // pixels being X times the width of digit characters plus Y times the width
  // of spaces plus Z times the width of ellipses in the default font of the
  // test plaform. It is assumed that all digits have the same width in that
  // font, that this width is greater than the width of spaces, and that the
  // width of 3 digits is greater than the width of ellipses.
  int ToPixels(int width);

  // Exercise BounderLabel::SplitLines() using the fixture's test label.
  string16 SplitLines(int width);

  // Exercise BounderLabel::GetPreferredLines() using the fixture's test label.
  int GetPreferredLines(int width);

 protected:
  // Creates a label to test with. Returns this fixture, which can be used to
  // test the newly created label using the exercise methods above.
  BoundedLabelTest& Label(string16 text, size_t lines);

 private:
  gfx::Font font_;  // The default font, which will be used for tests.
  int digit_pixels_;
  int space_pixels_;
  int ellipsis_pixels_;
  scoped_ptr<BoundedLabel> label_;
};

/* Test fixture definition ****************************************************/

BoundedLabelTest::BoundedLabelTest() {
  digit_pixels_ = font_.GetStringWidth(UTF8ToUTF16("0"));
  space_pixels_ = font_.GetStringWidth(UTF8ToUTF16(" "));
  ellipsis_pixels_ = font_.GetStringWidth(UTF8ToUTF16("\xE2\x80\xA6"));
}

BoundedLabelTest::~BoundedLabelTest() {
}

string16 BoundedLabelTest::ToString(const char* string) {
  const string16 periods = UTF8ToUTF16("...");
  const string16 ellipses = UTF8ToUTF16("\xE2\x80\xA6");
  string16 result = UTF8ToUTF16(string);
  ReplaceSubstringsAfterOffset(&result, 0, periods, ellipses);
  return result;
}

int BoundedLabelTest::ToPixels(int width) {
  return digit_pixels_ * width / 100 +
         space_pixels_ * (width % 100) / 10 +
         ellipsis_pixels_ * (width % 10);
}

string16 BoundedLabelTest::SplitLines(int width) {
  return JoinString(label_->SplitLines(width, label_->GetMaxLines()), '\n');
}

int BoundedLabelTest::GetPreferredLines(int width) {
  label_->SetBounds(0, 0, width, font_.GetHeight() * label_->GetMaxLines());
  return label_->GetPreferredLines();
}

BoundedLabelTest& BoundedLabelTest::Label(string16 text, size_t lines) {
  label_.reset(new BoundedLabel(text, font_, lines));
  return *this;
}

/* Test macro definitions *****************************************************/

#define TEST_SPLIT_LINES(expected, text, width, lines) \
  EXPECT_EQ(ToString(expected), \
            Label(ToString(text), lines).SplitLines(ToPixels(width)))

#define TEST_GET_PREFERRED_LINES(expected, text, width, lines) \
  EXPECT_EQ(expected, \
            Label(ToString(text), lines).GetPreferredLines(ToPixels(width)))

/* Elision tests **************************************************************/

TEST_F(BoundedLabelTest, ElisionTest) {
  // One word per line: No ellision should be made when not necessary.
  TEST_SPLIT_LINES("123", "123", 301, 1);
  TEST_SPLIT_LINES("123", "123", 301, 2);
  TEST_SPLIT_LINES("123", "123", 301, 3);
  TEST_SPLIT_LINES("123\n456", "123 456", 0, 2);  // 301, 2);
  TEST_SPLIT_LINES("123\n456", "123 456", 301, 3);
  TEST_SPLIT_LINES("123\n456\n789", "123 456 789", 301, 3);

  // One word per line: Ellisions should be made when necessary.
  TEST_SPLIT_LINES("123...", "123 456", 301, 1);
  TEST_SPLIT_LINES("123...", "123 456 789", 301, 1);
  TEST_SPLIT_LINES("123\n456...", "123 456 789", 301, 2);

  // Two words per line: No ellision should be made when not necessary.
  TEST_SPLIT_LINES("123 456", "123 456", 621, 1);
  TEST_SPLIT_LINES("123 456", "123 456", 621, 2);
  TEST_SPLIT_LINES("123 456", "123 456", 621, 3);
  TEST_SPLIT_LINES("123 456\n789 012", "123 456 789 012", 621, 2);
  TEST_SPLIT_LINES("123 456\n789 012", "123 456 789 012", 621, 3);
  TEST_SPLIT_LINES("123 456\n789 012\n345 678",
                   "123 456 789 012 345 678", 621, 3);

  // Two words per line: Ellisions should be made when necessary.
  TEST_SPLIT_LINES("123 456...", "123 456 789 012", 621, 1);
  TEST_SPLIT_LINES("123 456...", "123 456 789 012 345 678", 621, 1);
  TEST_SPLIT_LINES("123 456\n789 012...", "123 456 789 012 345 678", 621, 2);

  // Single trailing spaces: No ellipses should be added.
  TEST_SPLIT_LINES("123", "123 ", 301, 1);
  TEST_SPLIT_LINES("123\n456", "123 456 ", 301, 2);
  TEST_SPLIT_LINES("123\n456\n789", "123 456 789 ", 301, 3);
  TEST_SPLIT_LINES("123 456", "123 456 ", 611, 1);
  TEST_SPLIT_LINES("123 456\n789 012", "123 456 789 012 ", 611, 2);
  TEST_SPLIT_LINES("123 456\n789 012\n345 678",
                   "123 456 789 012 345 678 ", 611, 3);

  // Multiple trailing spaces: No ellipses should be added.
  TEST_SPLIT_LINES("123", "123         ", 301, 1);
  TEST_SPLIT_LINES("123\n456", "123 456         ", 301, 2);
  TEST_SPLIT_LINES("123\n456\n789", "123 456 789         ", 301, 3);
  TEST_SPLIT_LINES("123 456", "123 456         ", 611, 1);
  TEST_SPLIT_LINES("123 456\n789 012", "123 456 789 012         ", 611, 2);
  TEST_SPLIT_LINES("123 456\n789 012\n345 678",
                   "123 456 789 012 345 678         ", 611, 3);

  // Multiple spaces between words on the same line: Spaces should be preserved.
  // Test cases for single spaces between such words are included in the "Two
  // words per line" sections above.
  TEST_SPLIT_LINES("123  456", "123  456", 621, 1);
  TEST_SPLIT_LINES("123  456...", "123  456 789   012", 631, 1);
  TEST_SPLIT_LINES("123  456\n789   012", "123  456 789   012", 631, 2);
  TEST_SPLIT_LINES("123  456...", "123  456 789   012  345    678", 621, 1);
  TEST_SPLIT_LINES("123  456\n789   012...",
                   "123  456 789   012 345    678", 631, 2);
  TEST_SPLIT_LINES("123  456\n789   012\n345    678",
                   "123  456 789   012 345    678", 641, 3);

  // Multiple spaces between words split across lines: Spaces should be removed
  // even if lines are wide enough to include those spaces. Test cases for
  // single spaces between such words are included in the "Two words  per line"
  // sections above.
  TEST_SPLIT_LINES("123\n456", "123  456", 321, 2);
  TEST_SPLIT_LINES("123\n456", "123         456", 391, 2);
  TEST_SPLIT_LINES("123\n456...", "123  456  789", 321, 2);
  TEST_SPLIT_LINES("123\n456...", "123         456         789", 391, 2);
  TEST_SPLIT_LINES("123  456\n789  012", "123  456  789  012", 641, 2);
  TEST_SPLIT_LINES("123  456\n789  012...", "123  456  789  012  345  678",
                    641, 2);

  // TODO(dharcourt): Add test cases to verify that:
  // - Spaces before elisions are removed
  // - Leading spaces are preserved
  // - Words are split when they are longer than lines
  // - Words are clipped when they are longer than the last line
  // - No blank line are created before or after clipped word
  // - Spaces at the end of the text are removed

  // TODO(dharcourt): Add test cases for:
  // - Empty and very large strings
  // - Zero and very large max lines values
  // - Other input boundary conditions
  // TODO(dharcourt): Add some randomly generated fuzz test cases.

}

/* GetPreferredLinesTest ******************************************************/

TEST_F(BoundedLabelTest, GetPreferredLinesTest) {
  // Zero, small, and negative width values should yield one word per line.
  TEST_GET_PREFERRED_LINES(2, "123 456", 0, 1);
  TEST_GET_PREFERRED_LINES(2, "123 456", 1, 1);
  TEST_GET_PREFERRED_LINES(2, "123 456", 2, 1);
  TEST_GET_PREFERRED_LINES(2, "123 456", 3, 1);
  TEST_GET_PREFERRED_LINES(2, "123 456", -1, 1);
  TEST_GET_PREFERRED_LINES(2, "123 456", -2, 1);
  TEST_GET_PREFERRED_LINES(2, "123 456", std::numeric_limits<int>::min(), 1);

  // Large width values should yield all words on one line.
  TEST_GET_PREFERRED_LINES(1, "123 456", 610, 1);
  TEST_GET_PREFERRED_LINES(1, "123 456", std::numeric_limits<int>::max(), 1);
}

/* Other tests ****************************************************************/

// TODO(dharcourt): Add test cases to verify that:
// - SetMaxLines() affects GetMaxLines(), GetHeightForWidth(), GetTextSize(),
//   and GetTextLines() return values but not GetPreferredLines() or
//   GetTextSize() ones.
// - Bound changes affects GetPreferredLines(), GetTextSize(), and
//   GetTextLines() return values.
// - GetTextFlags are as expected.

}  // namespace message_center
