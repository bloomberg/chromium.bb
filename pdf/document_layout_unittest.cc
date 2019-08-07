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
  EXPECT_EQ(options_.default_page_orientation(), PageOrientation::kOriginal);
}

TEST_F(DocumentLayoutOptionsTest, CopyConstructor) {
  options_.RotatePagesClockwise();

  DocumentLayout::Options copy(options_);
  EXPECT_EQ(copy.default_page_orientation(), PageOrientation::kClockwise90);

  options_.RotatePagesClockwise();
  EXPECT_EQ(copy.default_page_orientation(), PageOrientation::kClockwise90);
}

TEST_F(DocumentLayoutOptionsTest, CopyAssignment) {
  options_.RotatePagesClockwise();

  DocumentLayout::Options copy;
  EXPECT_EQ(copy.default_page_orientation(), PageOrientation::kOriginal);
  copy = options_;
  EXPECT_EQ(copy.default_page_orientation(), PageOrientation::kClockwise90);

  options_.RotatePagesClockwise();
  EXPECT_EQ(copy.default_page_orientation(), PageOrientation::kClockwise90);
}

TEST_F(DocumentLayoutOptionsTest, RotatePagesClockwise) {
  options_.RotatePagesClockwise();
  EXPECT_EQ(options_.default_page_orientation(), PageOrientation::kClockwise90);

  options_.RotatePagesClockwise();
  EXPECT_EQ(options_.default_page_orientation(),
            PageOrientation::kClockwise180);

  options_.RotatePagesClockwise();
  EXPECT_EQ(options_.default_page_orientation(),
            PageOrientation::kClockwise270);

  options_.RotatePagesClockwise();
  EXPECT_EQ(options_.default_page_orientation(), PageOrientation::kOriginal);
}

TEST_F(DocumentLayoutOptionsTest, RotatePagesCounterclockwise) {
  options_.RotatePagesCounterclockwise();
  EXPECT_EQ(options_.default_page_orientation(),
            PageOrientation::kClockwise270);

  options_.RotatePagesCounterclockwise();
  EXPECT_EQ(options_.default_page_orientation(),
            PageOrientation::kClockwise180);

  options_.RotatePagesCounterclockwise();
  EXPECT_EQ(options_.default_page_orientation(), PageOrientation::kClockwise90);

  options_.RotatePagesCounterclockwise();
  EXPECT_EQ(options_.default_page_orientation(), PageOrientation::kOriginal);
}

TEST_F(DocumentLayoutTest, DefaultConstructor) {
  EXPECT_EQ(layout_.options().default_page_orientation(),
            PageOrientation::kOriginal);
  EXPECT_PRED2(PpSizeEq, layout_.size(), pp::Size(0, 0));
}

TEST_F(DocumentLayoutTest, SetOptionsDoesNotRecomputeLayout) {
  layout_.set_size(pp::Size(1, 2));

  DocumentLayout::Options options;
  options.RotatePagesClockwise();
  layout_.set_options(options);
  EXPECT_EQ(layout_.options().default_page_orientation(),
            PageOrientation::kClockwise90);
  EXPECT_PRED2(PpSizeEq, layout_.size(), pp::Size(1, 2));
}

TEST_F(DocumentLayoutTest, EnlargeHeight) {
  layout_.EnlargeHeight(5);
  EXPECT_PRED2(PpSizeEq, layout_.size(), pp::Size(0, 5));

  layout_.EnlargeHeight(11);
  EXPECT_PRED2(PpSizeEq, layout_.size(), pp::Size(0, 16));
}

TEST_F(DocumentLayoutTest, GetSingleViewLayout) {
  std::vector<pp::Rect> single_view_layout;

  std::vector<pp::Size> page_sizes{
      {300, 400}, {400, 500}, {300, 400}, {200, 300}};
  layout_.set_size({400, 0});
  single_view_layout = layout_.GetSingleViewLayout(page_sizes);
  ASSERT_EQ(4u, single_view_layout.size());
  EXPECT_PRED2(PpRectEq, pp::Rect(55, 3, 290, 390), single_view_layout[0]);
  EXPECT_PRED2(PpRectEq, pp::Rect(5, 407, 390, 490), single_view_layout[1]);
  EXPECT_PRED2(PpRectEq, pp::Rect(55, 911, 290, 390), single_view_layout[2]);
  EXPECT_PRED2(PpRectEq, pp::Rect(105, 1315, 190, 290), single_view_layout[3]);
  EXPECT_PRED2(PpSizeEq, pp::Size(400, 1612), layout_.size());

  page_sizes = {{240, 300}, {320, 400}, {250, 360}, {300, 600}, {270, 555}};
  layout_.set_size({320, 0});
  single_view_layout = layout_.GetSingleViewLayout(page_sizes);
  ASSERT_EQ(5u, single_view_layout.size());
  EXPECT_PRED2(PpRectEq, pp::Rect(45, 3, 230, 290), single_view_layout[0]);
  EXPECT_PRED2(PpRectEq, pp::Rect(5, 307, 310, 390), single_view_layout[1]);
  EXPECT_PRED2(PpRectEq, pp::Rect(40, 711, 240, 350), single_view_layout[2]);
  EXPECT_PRED2(PpRectEq, pp::Rect(15, 1075, 290, 590), single_view_layout[3]);
  EXPECT_PRED2(PpRectEq, pp::Rect(30, 1679, 260, 545), single_view_layout[4]);
  EXPECT_PRED2(PpSizeEq, pp::Size(320, 2231), layout_.size());
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
