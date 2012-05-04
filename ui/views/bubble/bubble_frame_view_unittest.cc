// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/hit_test.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/bubble/bubble_delegate.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/widget/widget.h"

namespace views {

typedef ViewsTestBase BubbleFrameViewTest;

const BubbleBorder::ArrowLocation kArrow = BubbleBorder::TOP_LEFT;
const gfx::Rect kRect(10, 10, 200, 200);
const SkColor kBackgroundColor = SK_ColorRED;
const int kDefaultMargin = 6;

namespace {

class SizedBubbleDelegateView : public BubbleDelegateView {
 public:
  SizedBubbleDelegateView();
  virtual ~SizedBubbleDelegateView();

  // View overrides:
  virtual gfx::Size GetPreferredSize() OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(SizedBubbleDelegateView);
};

SizedBubbleDelegateView::SizedBubbleDelegateView() {}

SizedBubbleDelegateView::~SizedBubbleDelegateView() {}

gfx::Size SizedBubbleDelegateView::GetPreferredSize() { return kRect.size(); }

class TestBubbleFrameView : public BubbleFrameView {
 public:
  explicit TestBubbleFrameView(const gfx::Rect& bounds);
  virtual ~TestBubbleFrameView();

 protected:
  virtual gfx::Rect GetMonitorBounds(const gfx::Rect& rect) OVERRIDE;

 private:
  gfx::Rect monitor_bounds_;

  DISALLOW_COPY_AND_ASSIGN(TestBubbleFrameView);
};

TestBubbleFrameView::TestBubbleFrameView(const gfx::Rect& bounds)
  : BubbleFrameView(kArrow, kBackgroundColor, kDefaultMargin),
    monitor_bounds_(bounds) {
}

TestBubbleFrameView::~TestBubbleFrameView() {}

gfx::Rect TestBubbleFrameView::GetMonitorBounds(const gfx::Rect& rect) {
  return monitor_bounds_;
}

}  // namespace

TEST_F(BubbleFrameViewTest, GetBoundsForClientView) {
  BubbleFrameView frame(kArrow, kBackgroundColor, kDefaultMargin);
  EXPECT_EQ(kArrow, frame.bubble_border()->arrow_location());
  EXPECT_EQ(kBackgroundColor, frame.bubble_border()->background_color());

  int margin_x = frame.content_margins().left();
  int margin_y = frame.content_margins().top();
  gfx::Insets insets;
  frame.bubble_border()->GetInsets(&insets);
  EXPECT_EQ(insets.left() + margin_x, frame.GetBoundsForClientView().x());
  EXPECT_EQ(insets.top() + margin_y, frame.GetBoundsForClientView().y());
}

TEST_F(BubbleFrameViewTest, NonClientHitTest) {
  BubbleDelegateView* delegate = new SizedBubbleDelegateView();
  Widget* widget(BubbleDelegateView::CreateBubble(delegate));
  delegate->Show();
  gfx::Point kPtInBound(100, 100);
  gfx::Point kPtOutsideBound(1000, 1000);
  BubbleFrameView* bubble_frame_view = delegate->GetBubbleFrameView();
  EXPECT_EQ(HTCLIENT, bubble_frame_view->NonClientHitTest(kPtInBound));
  EXPECT_EQ(HTNOWHERE, bubble_frame_view->NonClientHitTest(kPtOutsideBound));
  widget->CloseNow();
  RunPendingMessages();
}

// Tests that the arrow is mirrored as needed to better fit the screen.
TEST_F(BubbleFrameViewTest, GetUpdatedWindowBounds) {
  TestBubbleFrameView frame(gfx::Rect(0, 0, 1000, 1000));
  gfx::Rect window_bounds;

  gfx::Insets insets;
  frame.bubble_border()->GetInsets(&insets);
  int xposition = 95 - insets.width();

  // Test that the info bubble displays normally when it fits.
  frame.bubble_border()->set_arrow_location(BubbleBorder::TOP_LEFT);
  window_bounds = frame.GetUpdatedWindowBounds(
      gfx::Rect(100, 100, 50, 50),  // |anchor_rect|
      gfx::Size(500, 500),          // |client_size|
      true);                        // |try_mirroring_arrow|
  EXPECT_EQ(BubbleBorder::TOP_LEFT, frame.bubble_border()->arrow_location());
  EXPECT_GT(window_bounds.x(), xposition);
  EXPECT_GT(window_bounds.y(), 100 + 50 - 10);  // -10 to roughly compensate for
                                                // arrow overlap.

  // Test bubble not fitting on left.
  frame.bubble_border()->set_arrow_location(BubbleBorder::TOP_RIGHT);
  window_bounds = frame.GetUpdatedWindowBounds(
      gfx::Rect(100, 100, 50, 50),  // |anchor_rect|
      gfx::Size(500, 500),          // |client_size|
      true);                        // |try_mirroring_arrow|
  EXPECT_EQ(BubbleBorder::TOP_LEFT, frame.bubble_border()->arrow_location());
  EXPECT_GT(window_bounds.x(), xposition);
  EXPECT_GT(window_bounds.y(), 100 + 50 - 10);  // -10 to roughly compensate for
                                                // arrow overlap.

  // Test bubble not fitting on left or top.
  frame.bubble_border()->set_arrow_location(BubbleBorder::BOTTOM_RIGHT);
  window_bounds = frame.GetUpdatedWindowBounds(
      gfx::Rect(100, 100, 50, 50),  // |anchor_rect|
      gfx::Size(500, 500),          // |client_size|
      true);                        // |try_mirroring_arrow|
  EXPECT_EQ(BubbleBorder::TOP_LEFT, frame.bubble_border()->arrow_location());
  EXPECT_GT(window_bounds.x(), xposition);
  EXPECT_GT(window_bounds.y(), 100 + 50 - 10);  // -10 to roughly compensate for
                                                // arrow overlap.

  // Test bubble not fitting on top.
  frame.bubble_border()->set_arrow_location(BubbleBorder::BOTTOM_LEFT);
  window_bounds = frame.GetUpdatedWindowBounds(
      gfx::Rect(100, 100, 50, 50),  // |anchor_rect|
      gfx::Size(500, 500),          // |client_size|
      true);                        // |try_mirroring_arrow|
  EXPECT_EQ(BubbleBorder::TOP_LEFT, frame.bubble_border()->arrow_location());
  EXPECT_GT(window_bounds.x(), xposition);
  EXPECT_GT(window_bounds.y(), 100 + 50 - 10);  // -10 to roughly compensate for
                                                // arrow overlap.

  // Test bubble not fitting on top and right.
  frame.bubble_border()->set_arrow_location(BubbleBorder::BOTTOM_LEFT);
  window_bounds = frame.GetUpdatedWindowBounds(
      gfx::Rect(900, 100, 50, 50),  // |anchor_rect|
      gfx::Size(500, 500),          // |client_size|
      true);                        // |try_mirroring_arrow|
  EXPECT_EQ(BubbleBorder::TOP_RIGHT, frame.bubble_border()->arrow_location());
  EXPECT_LT(window_bounds.x(), 900 + 50 - 500);
  EXPECT_GT(window_bounds.y(), 100 + 50 - 10);  // -10 to roughly compensate for
                                                // arrow overlap.

  // Test bubble not fitting on right.
  frame.bubble_border()->set_arrow_location(BubbleBorder::TOP_LEFT);
  window_bounds = frame.GetUpdatedWindowBounds(
      gfx::Rect(900, 100, 50, 50),  // |anchor_rect|
      gfx::Size(500, 500),          // |client_size|
      true);                        // |try_mirroring_arrow|
  EXPECT_EQ(BubbleBorder::TOP_RIGHT, frame.bubble_border()->arrow_location());
  EXPECT_LT(window_bounds.x(), 900 + 50 - 500);
  EXPECT_GT(window_bounds.y(), 100 + 50 - 10);  // -10 to roughly compensate for
                                                // arrow overlap.

  // Test bubble not fitting on bottom and right.
  frame.bubble_border()->set_arrow_location(BubbleBorder::TOP_LEFT);
  window_bounds = frame.GetUpdatedWindowBounds(
      gfx::Rect(900, 900, 50, 50),  // |anchor_rect|
      gfx::Size(500, 500),          // |client_size|
      true);                        // |try_mirroring_arrow|
  EXPECT_EQ(BubbleBorder::BOTTOM_RIGHT,
            frame.bubble_border()->arrow_location());
  EXPECT_LT(window_bounds.x(), 900 + 50 - 500);
  EXPECT_LT(window_bounds.y(), 900 - 500 - 15);  // -15 to roughly compensate
                                                 // for arrow height.

  // Test bubble not fitting at the bottom.
  frame.bubble_border()->set_arrow_location(BubbleBorder::TOP_LEFT);
  window_bounds = frame.GetUpdatedWindowBounds(
      gfx::Rect(100, 900, 50, 50),  // |anchor_rect|
      gfx::Size(500, 500),          // |client_size|
      true);                        // |try_mirroring_arrow|
  EXPECT_EQ(BubbleBorder::BOTTOM_LEFT, frame.bubble_border()->arrow_location());
  // The window should be right aligned with the anchor_rect.
  EXPECT_LT(window_bounds.x(), 900 + 50 - 500);
  EXPECT_LT(window_bounds.y(), 900 - 500 - 15);  // -15 to roughly compensate
                                                 // for arrow height.

  // Test bubble not fitting at the bottom and left.
  frame.bubble_border()->set_arrow_location(BubbleBorder::TOP_RIGHT);
  window_bounds = frame.GetUpdatedWindowBounds(
      gfx::Rect(100, 900, 50, 50),  // |anchor_rect|
      gfx::Size(500, 500),          // |client_size|
      true);                        // |try_mirroring_arrow|
  EXPECT_EQ(BubbleBorder::BOTTOM_LEFT, frame.bubble_border()->arrow_location());
  // The window should be right aligned with the anchor_rect.
  EXPECT_LT(window_bounds.x(), 900 + 50 - 500);
  EXPECT_LT(window_bounds.y(), 900 - 500 - 15);  // -15 to roughly compensate
                                                 // for arrow height.
}

// Tests that the arrow is not moved when the info-bubble does not fit the
// screen but moving it would make matter worse.
TEST_F(BubbleFrameViewTest, GetUpdatedWindowBoundsMirroringFails) {
  TestBubbleFrameView frame(gfx::Rect(0, 0, 1000, 1000));
  frame.bubble_border()->set_arrow_location(BubbleBorder::TOP_LEFT);
  gfx::Rect window_bounds = frame.GetUpdatedWindowBounds(
      gfx::Rect(400, 100, 50, 50),  // |anchor_rect|
      gfx::Size(500, 700),          // |client_size|
      true);                        // |try_mirroring_arrow|
  EXPECT_EQ(BubbleBorder::TOP_LEFT, frame.bubble_border()->arrow_location());
}

// Test that the arrow will not be mirrored when |try_mirroring_arrow| is false.
TEST_F(BubbleFrameViewTest, GetUpdatedWindowBoundsDontTryMirror) {
  TestBubbleFrameView frame(gfx::Rect(0, 0, 1000, 1000));
  frame.bubble_border()->set_arrow_location(BubbleBorder::TOP_RIGHT);
  gfx::Rect window_bounds = frame.GetUpdatedWindowBounds(
      gfx::Rect(100, 900, 50, 50),  // |anchor_rect|
      gfx::Size(500, 500),          // |client_size|
      false);                       // |try_mirroring_arrow|
  EXPECT_EQ(BubbleBorder::TOP_RIGHT, frame.bubble_border()->arrow_location());
  // The coordinates should be pointing to anchor_rect from TOP_RIGHT.
  EXPECT_LT(window_bounds.x(), 100 + 50 - 500);
  EXPECT_GT(window_bounds.y(), 900 + 50 - 10);  // -10 to roughly compensate for
                                                // arrow overlap.
}

}  // namespace views
