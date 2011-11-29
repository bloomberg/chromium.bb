// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/controls/single_split_view.h"
#include "ui/views/controls/single_split_view_listener.h"

using ::testing::_;
using ::testing::Return;

namespace {

static void VerifySplitViewLayout(const views::SingleSplitView& split) {
  ASSERT_EQ(2, split.child_count());

  const views::View* leading = split.child_at(0);
  const views::View* trailing = split.child_at(1);

  if (split.bounds().IsEmpty()) {
    EXPECT_TRUE(leading->bounds().IsEmpty());
    EXPECT_TRUE(trailing->bounds().IsEmpty());
    return;
  }

  EXPECT_FALSE(leading->bounds().IsEmpty());
  EXPECT_FALSE(trailing->bounds().IsEmpty());
  EXPECT_FALSE(leading->bounds().Intersects(trailing->bounds()));

  if (split.orientation() == views::SingleSplitView::HORIZONTAL_SPLIT) {
    EXPECT_EQ(leading->bounds().height(), split.bounds().height());
    EXPECT_EQ(trailing->bounds().height(), split.bounds().height());
    EXPECT_LT(leading->bounds().width() + trailing->bounds().width(),
              split.bounds().width());
  } else if (split.orientation() == views::SingleSplitView::VERTICAL_SPLIT) {
    EXPECT_EQ(leading->bounds().width(), split.bounds().width());
    EXPECT_EQ(trailing->bounds().width(), split.bounds().width());
    EXPECT_LT(leading->bounds().height() + trailing->bounds().height(),
              split.bounds().height());
  } else {
    NOTREACHED();
  }
}

class MockObserver : public views::SingleSplitViewListener {
 public:
  MOCK_METHOD1(SplitHandleMoved, bool(views::SingleSplitView*));
};

}  // namespace

namespace views {

TEST(SingleSplitViewTest, Resize) {
  // Test cases to iterate through for horizontal and vertical split views.
  struct TestCase {
    // Split view resize policy for this test case.
    bool resize_leading_on_bounds_change;
    // Split view size to set.
    int primary_axis_size;
    int secondary_axis_size;
    // Expected divider offset.
    int divider_offset;
  } test_cases[] = {
    // The initial split size is 100x100, divider at 33.
    { true, 100, 100, 33 },
    // Grow the split view, leading view should grow.
    { true, 1000, 100, 933 },
    // Shrink the split view, leading view should shrink.
    { true, 200, 100, 133 },
    // Minimize the split view, divider should not move.
    { true, 0, 0, 133 },
    // Restore the split view, divider should not move.
    { false, 500, 100, 133 },
    // Resize the split view by secondary axis, divider should not move.
    { false,  500, 600, 133 }
  };

  SingleSplitView::Orientation orientations[] = {
    SingleSplitView::HORIZONTAL_SPLIT,
    SingleSplitView::VERTICAL_SPLIT
  };

  for (size_t orientation = 0; orientation < arraysize(orientations);
       ++orientation) {
    // Create a split view.
    SingleSplitView split(
        new View(), new View(), orientations[orientation], NULL);

    // Set initial size and divider offset.
    EXPECT_EQ(test_cases[0].primary_axis_size,
              test_cases[0].secondary_axis_size);
    split.SetBounds(0, 0, test_cases[0].primary_axis_size,
                    test_cases[0].secondary_axis_size);
    split.set_divider_offset(test_cases[0].divider_offset);
    split.Layout();

    // Run all test cases.
    for (size_t i = 0; i < ARRAYSIZE_UNSAFE(test_cases); ++i) {
      split.set_resize_leading_on_bounds_change(
          test_cases[i].resize_leading_on_bounds_change);
      if (split.orientation() == SingleSplitView::HORIZONTAL_SPLIT) {
        split.SetBounds(0, 0, test_cases[i].primary_axis_size,
                        test_cases[i].secondary_axis_size);
      } else {
        split.SetBounds(0, 0, test_cases[i].secondary_axis_size,
                        test_cases[i].primary_axis_size);
      }

      EXPECT_EQ(test_cases[i].divider_offset, split.divider_offset());
      VerifySplitViewLayout(split);
    }

    // Special cases, one of the child views is hidden.
    split.child_at(0)->SetVisible(false);
    split.Layout();

    EXPECT_EQ(split.size(), split.child_at(1)->size());

    split.child_at(0)->SetVisible(true);
    split.child_at(1)->SetVisible(false);
    split.Layout();

    EXPECT_EQ(split.size(), split.child_at(0)->size());
  }
}

TEST(SingleSplitViewTest, MouseDrag) {
  MockObserver observer;
  SingleSplitView split(
      new View(), new View(), SingleSplitView::VERTICAL_SPLIT, &observer);

  ON_CALL(observer, SplitHandleMoved(_))
      .WillByDefault(Return(true));
  // SplitHandleMoved is called for two mouse moves and one mouse capture loss.
  EXPECT_CALL(observer, SplitHandleMoved(_))
      .Times(3);

  split.SetBounds(0, 0, 10, 100);
  const int kInitialDividerOffset = 33;
  const int kMouseOffset = 2;  // Mouse offset in the divider.
  const int kMouseMoveDelta = 7;
  split.set_divider_offset(kInitialDividerOffset);
  split.Layout();

  // Drag divider to the right, in 2 steps.
  MouseEvent mouse_pressed(
      ui::ET_MOUSE_PRESSED, 7, kInitialDividerOffset + kMouseOffset, 0);
  ASSERT_TRUE(split.OnMousePressed(mouse_pressed));
  EXPECT_EQ(kInitialDividerOffset, split.divider_offset());

  MouseEvent mouse_dragged_1(
      ui::ET_MOUSE_DRAGGED, 5,
      kInitialDividerOffset + kMouseOffset + kMouseMoveDelta, 0);
  ASSERT_TRUE(split.OnMouseDragged(mouse_dragged_1));
  EXPECT_EQ(kInitialDividerOffset + kMouseMoveDelta, split.divider_offset());

  MouseEvent mouse_dragged_2(
      ui::ET_MOUSE_DRAGGED, 6,
      kInitialDividerOffset + kMouseOffset + kMouseMoveDelta * 2, 0);
  ASSERT_TRUE(split.OnMouseDragged(mouse_dragged_2));
  EXPECT_EQ(kInitialDividerOffset + kMouseMoveDelta * 2,
            split.divider_offset());

  MouseEvent mouse_released(
      ui::ET_MOUSE_RELEASED, 7,
      kInitialDividerOffset + kMouseOffset + kMouseMoveDelta * 2, 0);
  split.OnMouseReleased(mouse_released);
  EXPECT_EQ(kInitialDividerOffset + kMouseMoveDelta * 2,
            split.divider_offset());

  // Expect intial offset after a system/user gesture cancels the drag.
  // This shouldn't occur after mouse release, but it's sufficient for testing.
  split.OnMouseCaptureLost();
  EXPECT_EQ(kInitialDividerOffset, split.divider_offset());
}

}  // namespace views
