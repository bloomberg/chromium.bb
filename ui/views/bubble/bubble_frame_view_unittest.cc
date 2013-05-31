// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/hit_test.h"
#include "ui/gfx/insets.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/bubble/bubble_delegate.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/widget/widget.h"

namespace views {

typedef ViewsTestBase BubbleFrameViewTest;

namespace {

const BubbleBorder::Arrow kArrow = BubbleBorder::TOP_LEFT;
const int kBubbleWidth = 200;
const int kBubbleHeight = 200;
const SkColor kColor = SK_ColorRED;
const int kMargin = 6;

class TestBubbleDelegateView : public BubbleDelegateView {
 public:
  explicit TestBubbleDelegateView(View* anchor_view);
  virtual ~TestBubbleDelegateView();

  // View overrides:
  virtual gfx::Size GetPreferredSize() OVERRIDE;

  // BubbleDelegateView overrides:
  virtual bool CanResize() const OVERRIDE;
  virtual DialogDelegate* AsDialogDelegate() OVERRIDE;

  void set_can_resize(bool can_resize) { can_resize_ = can_resize; }

 private:
  bool can_resize_;

  DISALLOW_COPY_AND_ASSIGN(TestBubbleDelegateView);
};

TestBubbleDelegateView::TestBubbleDelegateView(View* anchor_view)
    : BubbleDelegateView(anchor_view, BubbleBorder::TOP_LEFT),
      can_resize_(false) {
}

TestBubbleDelegateView::~TestBubbleDelegateView() {}

gfx::Size TestBubbleDelegateView::GetPreferredSize() {
  return gfx::Size(kBubbleWidth, kBubbleHeight);
}

bool TestBubbleDelegateView::CanResize() const {
  return can_resize_;
}

DialogDelegate* TestBubbleDelegateView::AsDialogDelegate() {
  // This hack lets the bubble frame view be tested as its used with dialogs.
  // In particular, this is checked while hit-testing for the ability to drag.
  // The value should never be dereferenced in any of the tests below.
  return reinterpret_cast<DialogDelegate*>(1);
}

class TestBubbleFrameView : public BubbleFrameView {
 public:
  TestBubbleFrameView();
  virtual ~TestBubbleFrameView();

 protected:
  virtual gfx::Rect GetMonitorBounds(const gfx::Rect& rect) OVERRIDE;

 private:
  gfx::Rect monitor_bounds_;

  DISALLOW_COPY_AND_ASSIGN(TestBubbleFrameView);
};

TestBubbleFrameView::TestBubbleFrameView()
    : BubbleFrameView(gfx::Insets(kMargin, kMargin, kMargin, kMargin)),
      monitor_bounds_(gfx::Rect(0, 0, 1000, 1000)) {
  SetBubbleBorder(new BubbleBorder(kArrow, BubbleBorder::NO_SHADOW, kColor));
}

TestBubbleFrameView::~TestBubbleFrameView() {}

gfx::Rect TestBubbleFrameView::GetMonitorBounds(const gfx::Rect& rect) {
  return monitor_bounds_;
}

}  // namespace

TEST_F(BubbleFrameViewTest, GetBoundsForClientView) {
  TestBubbleFrameView frame;
  EXPECT_EQ(kArrow, frame.bubble_border()->arrow());
  EXPECT_EQ(kColor, frame.bubble_border()->background_color());

  int margin_x = frame.content_margins().left();
  int margin_y = frame.content_margins().top();
  gfx::Insets insets = frame.bubble_border()->GetInsets();
  EXPECT_EQ(insets.left() + margin_x, frame.GetBoundsForClientView().x());
  EXPECT_EQ(insets.top() + margin_y, frame.GetBoundsForClientView().y());
}

TEST_F(BubbleFrameViewTest, NonClientHitTest) {
  // Create the anchor view, its parent widget is needed on Aura.
  Widget::InitParams params = CreateParams(Widget::InitParams::TYPE_WINDOW);
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  scoped_ptr<Widget> anchor_widget(new Widget);
  anchor_widget->Init(params);
  anchor_widget->Show();

  TestBubbleDelegateView bubble(anchor_widget->GetContentsView());
  BubbleDelegateView::CreateBubble(&bubble);
  BubbleFrameView* frame = bubble.GetBubbleFrameView();
  const int border = frame->bubble_border()->GetBorderThickness();

  struct {
    const int point;
    const int cannot_resize_hit;
    const int can_resize_hit;
  } cases[] = {
    { border,      HTCAPTION, HTTOPLEFT },
    { border + 3,  HTCAPTION, HTTOPLEFT },
    { border + 4,  HTCAPTION, HTCAPTION },
    { border + 8,  HTCAPTION, HTCAPTION },
    { border + 50, HTCLIENT,  HTCLIENT  },
    { 1000,        HTNOWHERE, HTNOWHERE },
  };

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(cases); ++i) {
    gfx::Point point(cases[i].point, cases[i].point);
    bubble.set_can_resize(false);
    EXPECT_EQ(cases[i].cannot_resize_hit, frame->NonClientHitTest(point));
    bubble.set_can_resize(true);
    EXPECT_EQ(cases[i].can_resize_hit, frame->NonClientHitTest(point));
  }
}

// Tests that the arrow is mirrored as needed to better fit the screen.
TEST_F(BubbleFrameViewTest, GetUpdatedWindowBounds) {
  TestBubbleFrameView frame;
  gfx::Rect window_bounds;

  gfx::Insets insets = frame.bubble_border()->GetInsets();
  int xposition = 95 - insets.width();

  // Test that the info bubble displays normally when it fits.
  frame.bubble_border()->set_arrow(BubbleBorder::TOP_LEFT);
  window_bounds = frame.GetUpdatedWindowBounds(
      gfx::Rect(100, 100, 50, 50),  // |anchor_rect|
      gfx::Size(500, 500),          // |client_size|
      true);                        // |adjust_if_offscreen|
  EXPECT_EQ(BubbleBorder::TOP_LEFT, frame.bubble_border()->arrow());
  EXPECT_GT(window_bounds.x(), xposition);
  EXPECT_GT(window_bounds.y(), 100 + 50 - 10);  // -10 to roughly compensate for
                                                // arrow overlap.

  // Test bubble not fitting on left.
  frame.bubble_border()->set_arrow(BubbleBorder::TOP_RIGHT);
  window_bounds = frame.GetUpdatedWindowBounds(
      gfx::Rect(100, 100, 50, 50),  // |anchor_rect|
      gfx::Size(500, 500),          // |client_size|
      true);                        // |adjust_if_offscreen|
  EXPECT_EQ(BubbleBorder::TOP_LEFT, frame.bubble_border()->arrow());
  EXPECT_GT(window_bounds.x(), xposition);
  EXPECT_GT(window_bounds.y(), 100 + 50 - 10);  // -10 to roughly compensate for
                                                // arrow overlap.

  // Test bubble not fitting on left or top.
  frame.bubble_border()->set_arrow(BubbleBorder::BOTTOM_RIGHT);
  window_bounds = frame.GetUpdatedWindowBounds(
      gfx::Rect(100, 100, 50, 50),  // |anchor_rect|
      gfx::Size(500, 500),          // |client_size|
      true);                        // |adjust_if_offscreen|
  EXPECT_EQ(BubbleBorder::TOP_LEFT, frame.bubble_border()->arrow());
  EXPECT_GT(window_bounds.x(), xposition);
  EXPECT_GT(window_bounds.y(), 100 + 50 - 10);  // -10 to roughly compensate for
                                                // arrow overlap.

  // Test bubble not fitting on top.
  frame.bubble_border()->set_arrow(BubbleBorder::BOTTOM_LEFT);
  window_bounds = frame.GetUpdatedWindowBounds(
      gfx::Rect(100, 100, 50, 50),  // |anchor_rect|
      gfx::Size(500, 500),          // |client_size|
      true);                        // |adjust_if_offscreen|
  EXPECT_EQ(BubbleBorder::TOP_LEFT, frame.bubble_border()->arrow());
  EXPECT_GT(window_bounds.x(), xposition);
  EXPECT_GT(window_bounds.y(), 100 + 50 - 10);  // -10 to roughly compensate for
                                                // arrow overlap.

  // Test bubble not fitting on top and right.
  frame.bubble_border()->set_arrow(BubbleBorder::BOTTOM_LEFT);
  window_bounds = frame.GetUpdatedWindowBounds(
      gfx::Rect(900, 100, 50, 50),  // |anchor_rect|
      gfx::Size(500, 500),          // |client_size|
      true);                        // |adjust_if_offscreen|
  EXPECT_EQ(BubbleBorder::TOP_RIGHT, frame.bubble_border()->arrow());
  EXPECT_LT(window_bounds.x(), 900 + 50 - 500);
  EXPECT_GT(window_bounds.y(), 100 + 50 - 10);  // -10 to roughly compensate for
                                                // arrow overlap.

  // Test bubble not fitting on right.
  frame.bubble_border()->set_arrow(BubbleBorder::TOP_LEFT);
  window_bounds = frame.GetUpdatedWindowBounds(
      gfx::Rect(900, 100, 50, 50),  // |anchor_rect|
      gfx::Size(500, 500),          // |client_size|
      true);                        // |adjust_if_offscreen|
  EXPECT_EQ(BubbleBorder::TOP_RIGHT, frame.bubble_border()->arrow());
  EXPECT_LT(window_bounds.x(), 900 + 50 - 500);
  EXPECT_GT(window_bounds.y(), 100 + 50 - 10);  // -10 to roughly compensate for
                                                // arrow overlap.

  // Test bubble not fitting on bottom and right.
  frame.bubble_border()->set_arrow(BubbleBorder::TOP_LEFT);
  window_bounds = frame.GetUpdatedWindowBounds(
      gfx::Rect(900, 900, 50, 50),  // |anchor_rect|
      gfx::Size(500, 500),          // |client_size|
      true);                        // |adjust_if_offscreen|
  EXPECT_EQ(BubbleBorder::BOTTOM_RIGHT, frame.bubble_border()->arrow());
  EXPECT_LT(window_bounds.x(), 900 + 50 - 500);
  EXPECT_LT(window_bounds.y(), 900 - 500 - 15);  // -15 to roughly compensate
                                                 // for arrow height.

  // Test bubble not fitting at the bottom.
  frame.bubble_border()->set_arrow(BubbleBorder::TOP_LEFT);
  window_bounds = frame.GetUpdatedWindowBounds(
      gfx::Rect(100, 900, 50, 50),  // |anchor_rect|
      gfx::Size(500, 500),          // |client_size|
      true);                        // |adjust_if_offscreen|
  EXPECT_EQ(BubbleBorder::BOTTOM_LEFT, frame.bubble_border()->arrow());
  // The window should be right aligned with the anchor_rect.
  EXPECT_LT(window_bounds.x(), 900 + 50 - 500);
  EXPECT_LT(window_bounds.y(), 900 - 500 - 15);  // -15 to roughly compensate
                                                 // for arrow height.

  // Test bubble not fitting at the bottom and left.
  frame.bubble_border()->set_arrow(BubbleBorder::TOP_RIGHT);
  window_bounds = frame.GetUpdatedWindowBounds(
      gfx::Rect(100, 900, 50, 50),  // |anchor_rect|
      gfx::Size(500, 500),          // |client_size|
      true);                        // |adjust_if_offscreen|
  EXPECT_EQ(BubbleBorder::BOTTOM_LEFT, frame.bubble_border()->arrow());
  // The window should be right aligned with the anchor_rect.
  EXPECT_LT(window_bounds.x(), 900 + 50 - 500);
  EXPECT_LT(window_bounds.y(), 900 - 500 - 15);  // -15 to roughly compensate
                                                 // for arrow height.
}

// Tests that the arrow is not moved when the info-bubble does not fit the
// screen but moving it would make matter worse.
TEST_F(BubbleFrameViewTest, GetUpdatedWindowBoundsMirroringFails) {
  TestBubbleFrameView frame;
  frame.bubble_border()->set_arrow(BubbleBorder::TOP_LEFT);
  gfx::Rect window_bounds = frame.GetUpdatedWindowBounds(
      gfx::Rect(400, 100, 50, 50),  // |anchor_rect|
      gfx::Size(500, 700),          // |client_size|
      true);                        // |adjust_if_offscreen|
  EXPECT_EQ(BubbleBorder::TOP_LEFT, frame.bubble_border()->arrow());
}

TEST_F(BubbleFrameViewTest, TestMirroringForCenteredArrow) {
  TestBubbleFrameView frame;

  // Test bubble not fitting above the anchor.
  frame.bubble_border()->set_arrow(BubbleBorder::BOTTOM_CENTER);
  gfx::Rect window_bounds = frame.GetUpdatedWindowBounds(
      gfx::Rect(100, 100, 50, 50),  // |anchor_rect|
      gfx::Size(500, 700),          // |client_size|
      true);                        // |adjust_if_offscreen|
  EXPECT_EQ(BubbleBorder::TOP_CENTER, frame.bubble_border()->arrow());

  // Test bubble not fitting below the anchor.
  frame.bubble_border()->set_arrow(BubbleBorder::TOP_CENTER);
  window_bounds = frame.GetUpdatedWindowBounds(
      gfx::Rect(300, 800, 50, 50),  // |anchor_rect|
      gfx::Size(500, 200),          // |client_size|
      true);                        // |adjust_if_offscreen|
  EXPECT_EQ(BubbleBorder::BOTTOM_CENTER, frame.bubble_border()->arrow());

  // Test bubble not fitting to the right of the anchor.
  frame.bubble_border()->set_arrow(BubbleBorder::LEFT_CENTER);
  window_bounds = frame.GetUpdatedWindowBounds(
      gfx::Rect(800, 300, 50, 50),  // |anchor_rect|
      gfx::Size(200, 500),          // |client_size|
      true);                        // |adjust_if_offscreen|
  EXPECT_EQ(BubbleBorder::RIGHT_CENTER, frame.bubble_border()->arrow());

  // Test bubble not fitting to the left of the anchor.
  frame.bubble_border()->set_arrow(BubbleBorder::RIGHT_CENTER);
  window_bounds = frame.GetUpdatedWindowBounds(
      gfx::Rect(100, 300, 50, 50),  // |anchor_rect|
      gfx::Size(500, 500),          // |client_size|
      true);                        // |adjust_if_offscreen|
  EXPECT_EQ(BubbleBorder::LEFT_CENTER, frame.bubble_border()->arrow());
}

// Test that the arrow will not be mirrored when |adjust_if_offscreen| is false.
TEST_F(BubbleFrameViewTest, GetUpdatedWindowBoundsDontTryMirror) {
  TestBubbleFrameView frame;
  frame.bubble_border()->set_arrow(BubbleBorder::TOP_RIGHT);
  gfx::Rect window_bounds = frame.GetUpdatedWindowBounds(
      gfx::Rect(100, 900, 50, 50),  // |anchor_rect|
      gfx::Size(500, 500),          // |client_size|
      false);                       // |adjust_if_offscreen|
  EXPECT_EQ(BubbleBorder::TOP_RIGHT, frame.bubble_border()->arrow());
  // The coordinates should be pointing to anchor_rect from TOP_RIGHT.
  EXPECT_LT(window_bounds.x(), 100 + 50 - 500);
  EXPECT_GT(window_bounds.y(), 900 + 50 - 10);  // -10 to roughly compensate for
                                                // arrow overlap.
}

// Test that the center arrow is moved as needed to fit the screen.
TEST_F(BubbleFrameViewTest, GetUpdatedWindowBoundsCenterArrows) {
  TestBubbleFrameView frame;
  gfx::Rect window_bounds;

  // Test that the bubble displays normally when it fits.
  frame.bubble_border()->set_arrow(BubbleBorder::TOP_CENTER);
  window_bounds = frame.GetUpdatedWindowBounds(
      gfx::Rect(500, 100, 50, 50),  // |anchor_rect|
      gfx::Size(500, 500),          // |client_size|
      true);                        // |adjust_if_offscreen|
  EXPECT_EQ(BubbleBorder::TOP_CENTER, frame.bubble_border()->arrow());
  EXPECT_EQ(window_bounds.x() + window_bounds.width() / 2, 525);

  frame.bubble_border()->set_arrow(BubbleBorder::BOTTOM_CENTER);
  window_bounds = frame.GetUpdatedWindowBounds(
      gfx::Rect(500, 900, 50, 50),  // |anchor_rect|
      gfx::Size(500, 500),          // |client_size|
      true);                        // |adjust_if_offscreen|
  EXPECT_EQ(BubbleBorder::BOTTOM_CENTER, frame.bubble_border()->arrow());
  EXPECT_EQ(window_bounds.x() + window_bounds.width() / 2, 525);

  frame.bubble_border()->set_arrow(BubbleBorder::LEFT_CENTER);
  window_bounds = frame.GetUpdatedWindowBounds(
      gfx::Rect(100, 400, 50, 50),  // |anchor_rect|
      gfx::Size(500, 500),          // |client_size|
      true);                        // |adjust_if_offscreen|
  EXPECT_EQ(BubbleBorder::LEFT_CENTER, frame.bubble_border()->arrow());
  EXPECT_EQ(window_bounds.y() + window_bounds.height() / 2, 425);

  frame.bubble_border()->set_arrow(BubbleBorder::RIGHT_CENTER);
  window_bounds = frame.GetUpdatedWindowBounds(
      gfx::Rect(900, 400, 50, 50),  // |anchor_rect|
      gfx::Size(500, 500),          // |client_size|
      true);                        // |adjust_if_offscreen|
  EXPECT_EQ(BubbleBorder::RIGHT_CENTER, frame.bubble_border()->arrow());
  EXPECT_EQ(window_bounds.y() + window_bounds.height() / 2, 425);

  // Test bubble not fitting left screen edge.
  frame.bubble_border()->set_arrow(BubbleBorder::TOP_CENTER);
  window_bounds = frame.GetUpdatedWindowBounds(
      gfx::Rect(100, 100, 50, 50),  // |anchor_rect|
      gfx::Size(500, 500),          // |client_size|
      true);                        // |adjust_if_offscreen|
  EXPECT_EQ(BubbleBorder::TOP_CENTER, frame.bubble_border()->arrow());
  EXPECT_EQ(window_bounds.x(), 0);
  EXPECT_EQ(window_bounds.x() +
            frame.bubble_border()->GetArrowOffset(window_bounds.size()), 125);

  frame.bubble_border()->set_arrow(BubbleBorder::BOTTOM_CENTER);
  window_bounds = frame.GetUpdatedWindowBounds(
      gfx::Rect(100, 900, 50, 50),  // |anchor_rect|
      gfx::Size(500, 500),          // |client_size|
      true);                        // |adjust_if_offscreen|
  EXPECT_EQ(BubbleBorder::BOTTOM_CENTER, frame.bubble_border()->arrow());
  EXPECT_EQ(window_bounds.x(), 0);
  EXPECT_EQ(window_bounds.x() +
            frame.bubble_border()->GetArrowOffset(window_bounds.size()), 125);

  // Test bubble not fitting right screen edge.
  frame.bubble_border()->set_arrow(BubbleBorder::TOP_CENTER);
  window_bounds = frame.GetUpdatedWindowBounds(
      gfx::Rect(900, 100, 50, 50),  // |anchor_rect|
      gfx::Size(500, 500),          // |client_size|
      true);                        // |adjust_if_offscreen|
  EXPECT_EQ(BubbleBorder::TOP_CENTER, frame.bubble_border()->arrow());
  EXPECT_EQ(window_bounds.right(), 1000);
  EXPECT_EQ(window_bounds.x() +
            frame.bubble_border()->GetArrowOffset(window_bounds.size()), 925);

  frame.bubble_border()->set_arrow(BubbleBorder::BOTTOM_CENTER);
  window_bounds = frame.GetUpdatedWindowBounds(
      gfx::Rect(900, 900, 50, 50),  // |anchor_rect|
      gfx::Size(500, 500),          // |client_size|
      true);                        // |adjust_if_offscreen|
  EXPECT_EQ(BubbleBorder::BOTTOM_CENTER, frame.bubble_border()->arrow());
  EXPECT_EQ(window_bounds.right(), 1000);
  EXPECT_EQ(window_bounds.x() +
            frame.bubble_border()->GetArrowOffset(window_bounds.size()), 925);

  // Test bubble not fitting top screen edge.
  frame.bubble_border()->set_arrow(BubbleBorder::LEFT_CENTER);
  window_bounds = frame.GetUpdatedWindowBounds(
      gfx::Rect(100, 100, 50, 50),  // |anchor_rect|
      gfx::Size(500, 500),          // |client_size|
      true);                        // |adjust_if_offscreen|
  EXPECT_EQ(BubbleBorder::LEFT_CENTER, frame.bubble_border()->arrow());
  EXPECT_EQ(window_bounds.y(), 0);
  EXPECT_EQ(window_bounds.y() +
            frame.bubble_border()->GetArrowOffset(window_bounds.size()), 125);

  frame.bubble_border()->set_arrow(BubbleBorder::RIGHT_CENTER);
  window_bounds = frame.GetUpdatedWindowBounds(
      gfx::Rect(900, 100, 50, 50),  // |anchor_rect|
      gfx::Size(500, 500),          // |client_size|
      true);                        // |adjust_if_offscreen|
  EXPECT_EQ(BubbleBorder::RIGHT_CENTER, frame.bubble_border()->arrow());
  EXPECT_EQ(window_bounds.y(), 0);
  EXPECT_EQ(window_bounds.y() +
            frame.bubble_border()->GetArrowOffset(window_bounds.size()), 125);

  // Test bubble not fitting bottom screen edge.
  frame.bubble_border()->set_arrow(BubbleBorder::LEFT_CENTER);
  window_bounds = frame.GetUpdatedWindowBounds(
      gfx::Rect(100, 900, 50, 50),  // |anchor_rect|
      gfx::Size(500, 500),          // |client_size|
      true);                        // |adjust_if_offscreen|
  EXPECT_EQ(BubbleBorder::LEFT_CENTER, frame.bubble_border()->arrow());
  EXPECT_EQ(window_bounds.bottom(), 1000);
  EXPECT_EQ(window_bounds.y() +
            frame.bubble_border()->GetArrowOffset(window_bounds.size()), 925);

  frame.bubble_border()->set_arrow(BubbleBorder::RIGHT_CENTER);
  window_bounds = frame.GetUpdatedWindowBounds(
      gfx::Rect(900, 900, 50, 50),  // |anchor_rect|
      gfx::Size(500, 500),          // |client_size|
      true);                        // |adjust_if_offscreen|
  EXPECT_EQ(BubbleBorder::RIGHT_CENTER, frame.bubble_border()->arrow());
  EXPECT_EQ(window_bounds.bottom(), 1000);
  EXPECT_EQ(window_bounds.y() +
            frame.bubble_border()->GetArrowOffset(window_bounds.size()), 925);
}

}  // namespace views
