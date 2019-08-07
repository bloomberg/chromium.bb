// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/document_layout.h"

#include "base/test/gtest_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chrome_pdf {

namespace {

class DocumentLayoutOptionsTest : public testing::Test {
 protected:
  DocumentLayout::Options options_;
};

class DocumentLayoutTest : public testing::Test {
 protected:
  DocumentLayout layout_;
};

using DocumentLayoutDeathTest = DocumentLayoutTest;

// TODO(kmoon): Need to use this with EXPECT_PRED2 instead of just using
// EXPECT_EQ, due to ADL issues with pp::Size's operator== (defined in global
// namespace, instead of in "pp").
inline bool PpSizeEq(const pp::Size& lhs, const pp::Size& rhs) {
  return lhs == rhs;
}

inline bool PpRectEq(const pp::Rect& lhs, const pp::Rect& rhs) {
  return lhs == rhs;
}

TEST_F(DocumentLayoutOptionsTest, DefaultConstructor) {
  EXPECT_EQ(options_.default_page_orientation(), 0);
}

TEST_F(DocumentLayoutOptionsTest, CopyConstructor) {
  options_.RotatePagesClockwise();

  DocumentLayout::Options copy(options_);
  EXPECT_EQ(copy.default_page_orientation(), 1);

  options_.RotatePagesClockwise();
  EXPECT_EQ(copy.default_page_orientation(), 1);
}

TEST_F(DocumentLayoutOptionsTest, CopyAssignment) {
  options_.RotatePagesClockwise();

  DocumentLayout::Options copy;
  EXPECT_EQ(copy.default_page_orientation(), 0);
  copy = options_;
  EXPECT_EQ(copy.default_page_orientation(), 1);

  options_.RotatePagesClockwise();
  EXPECT_EQ(copy.default_page_orientation(), 1);
}

TEST_F(DocumentLayoutOptionsTest, RotatePagesClockwise) {
  options_.RotatePagesClockwise();
  EXPECT_EQ(options_.default_page_orientation(), 1);

  options_.RotatePagesClockwise();
  EXPECT_EQ(options_.default_page_orientation(), 2);

  options_.RotatePagesClockwise();
  EXPECT_EQ(options_.default_page_orientation(), 3);

  options_.RotatePagesClockwise();
  EXPECT_EQ(options_.default_page_orientation(), 0);
}

TEST_F(DocumentLayoutOptionsTest, RotatePagesCounterclockwise) {
  options_.RotatePagesCounterclockwise();
  EXPECT_EQ(options_.default_page_orientation(), 3);

  options_.RotatePagesCounterclockwise();
  EXPECT_EQ(options_.default_page_orientation(), 2);

  options_.RotatePagesCounterclockwise();
  EXPECT_EQ(options_.default_page_orientation(), 1);

  options_.RotatePagesCounterclockwise();
  EXPECT_EQ(options_.default_page_orientation(), 0);
}

TEST_F(DocumentLayoutTest, DefaultConstructor) {
  EXPECT_EQ(layout_.options().default_page_orientation(), 0);
  EXPECT_PRED2(PpSizeEq, layout_.size(), pp::Size(0, 0));
}

TEST_F(DocumentLayoutTest, SetOptionsDoesNotRecomputeLayout) {
  layout_.set_size(pp::Size(1, 2));

  DocumentLayout::Options options;
  options.RotatePagesClockwise();
  layout_.set_options(options);
  EXPECT_EQ(layout_.options().default_page_orientation(), 1);
  EXPECT_PRED2(PpSizeEq, layout_.size(), pp::Size(1, 2));
}

TEST_F(DocumentLayoutTest, EnlargeHeight) {
  layout_.EnlargeHeight(5);
  EXPECT_PRED2(PpSizeEq, layout_.size(), pp::Size(0, 5));

  layout_.EnlargeHeight(11);
  EXPECT_PRED2(PpSizeEq, layout_.size(), pp::Size(0, 16));
}

TEST_F(DocumentLayoutTest, GetTwoUpViewLayout) {
  std::vector<pp::Rect> two_up_view_layout;

  // Test case where the widest page is on the right.
  std::vector<pp::Size> page_sizes{
      {826, 1066}, {1066, 826}, {826, 1066}, {826, 900}};
  layout_.set_size({1066, 0});
  two_up_view_layout = layout_.GetTwoUpViewLayout(page_sizes);
  ASSERT_EQ(4u, two_up_view_layout.size());
  EXPECT_PRED2(PpRectEq, pp::Rect(245, 3, 820, 1056), two_up_view_layout[0]);
  EXPECT_PRED2(PpRectEq, pp::Rect(1067, 3, 1060, 816), two_up_view_layout[1]);
  EXPECT_PRED2(PpRectEq, pp::Rect(245, 1069, 820, 1056), two_up_view_layout[2]);
  EXPECT_PRED2(PpRectEq, pp::Rect(1067, 1069, 820, 890), two_up_view_layout[3]);
  EXPECT_PRED2(PpSizeEq, pp::Size(1066, 2132), layout_.size());

  // Test case where the widest page is on the left.
  page_sizes = {{1066, 826}, {820, 1056}, {820, 890}, {826, 1066}};
  layout_.set_size({1066, 0});
  two_up_view_layout = layout_.GetTwoUpViewLayout(page_sizes);
  ASSERT_EQ(4u, two_up_view_layout.size());
  EXPECT_PRED2(PpRectEq, pp::Rect(5, 3, 1060, 816), two_up_view_layout[0]);
  EXPECT_PRED2(PpRectEq, pp::Rect(1067, 3, 814, 1046), two_up_view_layout[1]);
  EXPECT_PRED2(PpRectEq, pp::Rect(251, 1059, 814, 880), two_up_view_layout[2]);
  EXPECT_PRED2(PpRectEq, pp::Rect(1067, 1059, 820, 1056),
               two_up_view_layout[3]);
  EXPECT_PRED2(PpSizeEq, pp::Size(1066, 2122), layout_.size());

  // Test case where there's an odd # of pages.
  page_sizes = {{200, 300}, {400, 200}, {300, 600}, {250, 500}, {300, 400}};
  layout_.set_size({400, 0});
  two_up_view_layout = layout_.GetTwoUpViewLayout(page_sizes);
  ASSERT_EQ(5u, two_up_view_layout.size());
  EXPECT_PRED2(PpRectEq, pp::Rect(205, 3, 194, 290), two_up_view_layout[0]);
  EXPECT_PRED2(PpRectEq, pp::Rect(401, 3, 394, 190), two_up_view_layout[1]);
  EXPECT_PRED2(PpRectEq, pp::Rect(105, 303, 294, 590), two_up_view_layout[2]);
  EXPECT_PRED2(PpRectEq, pp::Rect(401, 303, 244, 490), two_up_view_layout[3]);
  EXPECT_PRED2(PpRectEq, pp::Rect(105, 903, 290, 390), two_up_view_layout[4]);
  EXPECT_PRED2(PpSizeEq, pp::Size(400, 1300), layout_.size());
}

TEST_F(DocumentLayoutDeathTest, EnlargeHeightNegativeIncrement) {
  EXPECT_DCHECK_DEATH(layout_.EnlargeHeight(-5));
}

TEST_F(DocumentLayoutTest, AppendPageRect) {
  layout_.AppendPageRect(pp::Size(3, 5));
  EXPECT_PRED2(PpSizeEq, layout_.size(), pp::Size(3, 5));

  layout_.AppendPageRect(pp::Size(7, 11));
  EXPECT_PRED2(PpSizeEq, layout_.size(), pp::Size(7, 16));

  layout_.AppendPageRect(pp::Size(5, 11));
  EXPECT_PRED2(PpSizeEq, layout_.size(), pp::Size(7, 27));
}

}  // namespace

}  // namespace chrome_pdf
