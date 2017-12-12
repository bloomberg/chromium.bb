// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/paint/FragmentData.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

class FragmentDataTest : public ::testing::Test {
 protected:
  bool HasRareData(const FragmentData& data) { return !!data.rare_data_; }
};

TEST_F(FragmentDataTest, LocationInBackingAndSelectionVisualRect) {
  FragmentData fragment;

  // Default LocationInBacking and SelectionVisualRect should not create
  // RareData.
  fragment.SetVisualRect(LayoutRect(10, 20, 30, 400));
  fragment.SetLocationInBacking(LayoutPoint(10, 20));
  fragment.SetSelectionVisualRect(LayoutRect());
  EXPECT_FALSE(HasRareData(fragment));
  EXPECT_EQ(LayoutPoint(10, 20), fragment.LocationInBacking());
  EXPECT_EQ(LayoutRect(), fragment.SelectionVisualRect());

  // Non-Default LocationInBacking and SelectionVisualRect create RareData.
  fragment.SetLocationInBacking(LayoutPoint(20, 30));
  fragment.SetSelectionVisualRect(LayoutRect(1, 2, 3, 4));
  EXPECT_TRUE(HasRareData(fragment));
  EXPECT_EQ(LayoutPoint(20, 30), fragment.LocationInBacking());
  EXPECT_EQ(LayoutRect(1, 2, 3, 4), fragment.SelectionVisualRect());

  // PaintProperties should store default LocationInBacking and
  // SelectionVisualRect once it's created.
  fragment.SetLocationInBacking(LayoutPoint(10, 20));
  fragment.SetSelectionVisualRect(LayoutRect());
  EXPECT_TRUE(HasRareData(fragment));
  EXPECT_EQ(LayoutPoint(10, 20), fragment.LocationInBacking());
  EXPECT_EQ(LayoutRect(), fragment.SelectionVisualRect());
}

}  // namespace blink
