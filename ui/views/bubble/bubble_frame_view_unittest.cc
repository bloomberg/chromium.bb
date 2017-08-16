// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/bubble/bubble_frame_view.h"

#include <memory>

#include "base/macros.h"
#include "build/build_config.h"
#include "ui/base/material_design/material_design_controller.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/bubble/bubble_dialog_delegate.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/test/test_layout_provider.h"
#include "ui/views/test/test_views.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

namespace views {

typedef ViewsTestBase BubbleFrameViewTest;

namespace {

bool UseMd() {
  return ui::MaterialDesignController::IsSecondaryUiMaterial();
}

const BubbleBorder::Arrow kArrow = BubbleBorder::TOP_LEFT;
const SkColor kColor = SK_ColorRED;
const int kMargin = 6;
const int kMinimumClientWidth = 100;
const int kMinimumClientHeight = 200;
const int kMaximumClientWidth = 300;
const int kMaximumClientHeight = 300;
const int kPreferredClientWidth = 150;
const int kPreferredClientHeight = 250;

// These account for non-client areas like the title bar, footnote etc. However
// these do not take the bubble border into consideration.
const int kExpectedAdditionalWidth = 12;
const int kExpectedAdditionalHeight = 12;

class TestBubbleFrameViewWidgetDelegate : public WidgetDelegate {
 public:
  TestBubbleFrameViewWidgetDelegate(Widget* widget) : widget_(widget) {}

  ~TestBubbleFrameViewWidgetDelegate() override {}

  // WidgetDelegate overrides:
  Widget* GetWidget() override { return widget_; }
  const Widget* GetWidget() const override { return widget_; }

  View* GetContentsView() override {
    if (!contents_view_) {
      StaticSizedView* contents_view = new StaticSizedView(
          gfx::Size(kPreferredClientWidth, kPreferredClientHeight));
      contents_view->set_minimum_size(
          gfx::Size(kMinimumClientWidth, kMinimumClientHeight));
      contents_view->set_maximum_size(
          gfx::Size(kMaximumClientWidth, kMaximumClientHeight));
      contents_view_ = contents_view;
    }
    return contents_view_;
  }

  bool ShouldShowCloseButton() const override { return should_show_close_; }
  void SetShouldShowCloseButton(bool should_show_close) {
    should_show_close_ = should_show_close;
  }

 private:
  Widget* widget_;
  View* contents_view_ = nullptr;  // Owned by |widget_|.
  bool should_show_close_ = false;
};

class TestBubbleFrameView : public BubbleFrameView {
 public:
  TestBubbleFrameView(ViewsTestBase* test_base)
      : BubbleFrameView(gfx::Insets(), gfx::Insets(kMargin)),
        test_base_(test_base),
        available_bounds_(gfx::Rect(0, 0, 1000, 1000)) {
    SetBubbleBorder(std::unique_ptr<BubbleBorder>(
        new BubbleBorder(kArrow, BubbleBorder::NO_SHADOW, kColor)));
  }
  ~TestBubbleFrameView() override {}

  // View overrides:
  const Widget* GetWidget() const override {
    if (!widget_) {
      widget_.reset(new Widget);
      widget_delegate_.reset(
          new TestBubbleFrameViewWidgetDelegate(widget_.get()));
      Widget::InitParams params =
          test_base_->CreateParams(Widget::InitParams::TYPE_BUBBLE);
      params.delegate = widget_delegate_.get();
      params.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
      widget_->Init(params);
    }

    return widget_.get();
  }

  // BubbleFrameView overrides:
  gfx::Rect GetAvailableScreenBounds(const gfx::Rect& rect) const override {
    return available_bounds_;
  }

  TestBubbleFrameViewWidgetDelegate* widget_delegate() {
    return widget_delegate_.get();
  }

 private:
  ViewsTestBase* test_base_;

  gfx::Rect available_bounds_;

  // Widget returned by GetWidget(). Only created if GetWidget() is called.
  mutable std::unique_ptr<TestBubbleFrameViewWidgetDelegate> widget_delegate_;
  mutable std::unique_ptr<Widget> widget_;

  DISALLOW_COPY_AND_ASSIGN(TestBubbleFrameView);
};

}  // namespace

TEST_F(BubbleFrameViewTest, GetBoundsForClientView) {
  TestBubbleFrameView frame(this);
  EXPECT_EQ(kArrow, frame.bubble_border()->arrow());
  EXPECT_EQ(kColor, frame.bubble_border()->background_color());

  int margin_x = frame.content_margins().left();
  int margin_y = frame.content_margins().top();
  gfx::Insets insets = frame.bubble_border()->GetInsets();
  EXPECT_EQ(insets.left() + margin_x, frame.GetBoundsForClientView().x());
  EXPECT_EQ(insets.top() + margin_y, frame.GetBoundsForClientView().y());
}

TEST_F(BubbleFrameViewTest, RemoveFootnoteView) {
  TestBubbleFrameView frame(this);
  EXPECT_EQ(nullptr, frame.footnote_container_);
  View* footnote_dummy_view = new StaticSizedView(gfx::Size(200, 200));
  frame.SetFootnoteView(footnote_dummy_view);
  EXPECT_EQ(footnote_dummy_view->parent(), frame.footnote_container_);
  View* container_view = footnote_dummy_view->parent();
  delete footnote_dummy_view;
  footnote_dummy_view = nullptr;
  EXPECT_FALSE(container_view->visible());
  EXPECT_EQ(nullptr, frame.footnote_container_);
}

TEST_F(BubbleFrameViewTest, GetBoundsForClientViewWithClose) {
  TestBubbleFrameView frame(this);
  // TestBubbleFrameView::GetWidget() is responsible for creating the widget and
  // widget delegate at first call, so it is called here for that side-effect.
  ignore_result(frame.GetWidget());
  frame.widget_delegate()->SetShouldShowCloseButton(true);
  frame.ResetWindowControls();
  EXPECT_EQ(kArrow, frame.bubble_border()->arrow());
  EXPECT_EQ(kColor, frame.bubble_border()->background_color());

  gfx::Insets frame_insets = frame.GetInsets();
  gfx::Insets border_insets = frame.bubble_border()->GetInsets();
  EXPECT_EQ(border_insets.left() + frame_insets.left(),
            frame.GetBoundsForClientView().x());
  EXPECT_EQ(border_insets.top() + frame_insets.top(),
            frame.GetBoundsForClientView().y());
}

TEST_F(BubbleFrameViewTest,
       FootnoteContainerViewShouldMatchVisibilityOfFirstChild) {
  TestBubbleFrameView frame(this);
  View* footnote_dummy_view = new StaticSizedView(gfx::Size(200, 200));
  footnote_dummy_view->SetVisible(false);
  frame.SetFootnoteView(footnote_dummy_view);
  View* footnote_container_view = footnote_dummy_view->parent();
  EXPECT_FALSE(footnote_container_view->visible());
  footnote_dummy_view->SetVisible(true);
  EXPECT_TRUE(footnote_container_view->visible());
  footnote_dummy_view->SetVisible(false);
  EXPECT_FALSE(footnote_container_view->visible());
}

// Tests that the arrow is mirrored as needed to better fit the screen.
TEST_F(BubbleFrameViewTest, GetUpdatedWindowBounds) {
  TestBubbleFrameView frame(this);
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
  TestBubbleFrameView frame(this);
  frame.bubble_border()->set_arrow(BubbleBorder::TOP_LEFT);
  frame.GetUpdatedWindowBounds(
      gfx::Rect(400, 100, 50, 50),  // |anchor_rect|
      gfx::Size(500, 700),          // |client_size|
      true);                        // |adjust_if_offscreen|
  EXPECT_EQ(BubbleBorder::TOP_LEFT, frame.bubble_border()->arrow());
}

TEST_F(BubbleFrameViewTest, TestMirroringForCenteredArrow) {
  TestBubbleFrameView frame(this);

  // Test bubble not fitting above the anchor.
  frame.bubble_border()->set_arrow(BubbleBorder::BOTTOM_CENTER);
  frame.GetUpdatedWindowBounds(
      gfx::Rect(100, 100, 50, 50),  // |anchor_rect|
      gfx::Size(500, 700),          // |client_size|
      true);                        // |adjust_if_offscreen|
  EXPECT_EQ(BubbleBorder::TOP_CENTER, frame.bubble_border()->arrow());

  // Test bubble not fitting below the anchor.
  frame.bubble_border()->set_arrow(BubbleBorder::TOP_CENTER);
  frame.GetUpdatedWindowBounds(
      gfx::Rect(300, 800, 50, 50),  // |anchor_rect|
      gfx::Size(500, 200),          // |client_size|
      true);                        // |adjust_if_offscreen|
  EXPECT_EQ(BubbleBorder::BOTTOM_CENTER, frame.bubble_border()->arrow());

  // Test bubble not fitting to the right of the anchor.
  frame.bubble_border()->set_arrow(BubbleBorder::LEFT_CENTER);
  frame.GetUpdatedWindowBounds(
      gfx::Rect(800, 300, 50, 50),  // |anchor_rect|
      gfx::Size(200, 500),          // |client_size|
      true);                        // |adjust_if_offscreen|
  EXPECT_EQ(BubbleBorder::RIGHT_CENTER, frame.bubble_border()->arrow());

  // Test bubble not fitting to the left of the anchor.
  frame.bubble_border()->set_arrow(BubbleBorder::RIGHT_CENTER);
  frame.GetUpdatedWindowBounds(
      gfx::Rect(100, 300, 50, 50),  // |anchor_rect|
      gfx::Size(500, 500),          // |client_size|
      true);                        // |adjust_if_offscreen|
  EXPECT_EQ(BubbleBorder::LEFT_CENTER, frame.bubble_border()->arrow());
}

// Test that the arrow will not be mirrored when |adjust_if_offscreen| is false.
TEST_F(BubbleFrameViewTest, GetUpdatedWindowBoundsDontTryMirror) {
  TestBubbleFrameView frame(this);
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
  TestBubbleFrameView frame(this);
  gfx::Rect window_bounds;
  // Bubbles have a thicker shadow on the bottom in MD.
  // Match definition of kLargeShadowVerticalOffset in bubble_border.cc.
  const int kLargeShadowVerticalOffset = UseMd() ? 2 : 0;

  // Some of these tests may go away once --secondary-ui-md becomes the
  // default. Under Material Design mode, the BubbleBorder doesn't support all
  // "arrow" positions. If this changes, then the tests should be updated or
  // added for MD mode.

  // Test that the bubble displays normally when it fits.
  if (!UseMd()) {  // TOP_CENTER isn't supported by the bubble_border() in MD.
    frame.bubble_border()->set_arrow(BubbleBorder::TOP_CENTER);
    window_bounds = frame.GetUpdatedWindowBounds(
        gfx::Rect(500, 100, 50, 50),  // |anchor_rect|
        gfx::Size(500, 500),          // |client_size|
        true);                        // |adjust_if_offscreen|
    EXPECT_EQ(BubbleBorder::TOP_CENTER, frame.bubble_border()->arrow());
    EXPECT_EQ(window_bounds.x() + window_bounds.width() / 2, 525);
  }

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
  EXPECT_EQ(window_bounds.y() + window_bounds.height() / 2,
            425 + kLargeShadowVerticalOffset);

  frame.bubble_border()->set_arrow(BubbleBorder::RIGHT_CENTER);
  window_bounds = frame.GetUpdatedWindowBounds(
      gfx::Rect(900, 400, 50, 50),  // |anchor_rect|
      gfx::Size(500, 500),          // |client_size|
      true);                        // |adjust_if_offscreen|
  EXPECT_EQ(BubbleBorder::RIGHT_CENTER, frame.bubble_border()->arrow());
  EXPECT_EQ(window_bounds.y() + window_bounds.height() / 2,
            425 + kLargeShadowVerticalOffset);

  // Test bubble not fitting left screen edge.
  if (!UseMd()) {  // TOP_CENTER isn't supported by the bubble_border() in MD.
    frame.bubble_border()->set_arrow(BubbleBorder::TOP_CENTER);
    window_bounds = frame.GetUpdatedWindowBounds(
        gfx::Rect(100, 100, 50, 50),  // |anchor_rect|
        gfx::Size(500, 500),          // |client_size|
        true);                        // |adjust_if_offscreen|
    EXPECT_EQ(BubbleBorder::TOP_CENTER, frame.bubble_border()->arrow());
    EXPECT_EQ(window_bounds.x(), 0);
    EXPECT_EQ(window_bounds.x() +
                  frame.bubble_border()->GetArrowOffset(window_bounds.size()),
              125);
  }

  frame.bubble_border()->set_arrow(BubbleBorder::BOTTOM_CENTER);
  window_bounds = frame.GetUpdatedWindowBounds(
      gfx::Rect(100, 900, 50, 50),  // |anchor_rect|
      gfx::Size(500, 500),          // |client_size|
      true);                        // |adjust_if_offscreen|
  EXPECT_EQ(BubbleBorder::BOTTOM_CENTER, frame.bubble_border()->arrow());
  EXPECT_EQ(window_bounds.x(), 0);
  if (!UseMd()) {  // There is no arrow offset in MD mode.
    EXPECT_EQ(window_bounds.x() +
                  frame.bubble_border()->GetArrowOffset(window_bounds.size()),
              125);
  }

  // Test bubble not fitting right screen edge.
  if (!UseMd()) {  // TOP_CENTER isn't supported by the bubble_border() in MD.
    frame.bubble_border()->set_arrow(BubbleBorder::TOP_CENTER);
    window_bounds = frame.GetUpdatedWindowBounds(
        gfx::Rect(900, 100, 50, 50),  // |anchor_rect|
        gfx::Size(500, 500),          // |client_size|
        true);                        // |adjust_if_offscreen|
    EXPECT_EQ(BubbleBorder::TOP_CENTER, frame.bubble_border()->arrow());
    EXPECT_EQ(window_bounds.right(), 1000);
    EXPECT_EQ(window_bounds.x() +
                  frame.bubble_border()->GetArrowOffset(window_bounds.size()),
              925);
  }

  frame.bubble_border()->set_arrow(BubbleBorder::BOTTOM_CENTER);
  window_bounds = frame.GetUpdatedWindowBounds(
      gfx::Rect(900, 900, 50, 50),  // |anchor_rect|
      gfx::Size(500, 500),          // |client_size|
      true);                        // |adjust_if_offscreen|
  EXPECT_EQ(BubbleBorder::BOTTOM_CENTER, frame.bubble_border()->arrow());
  EXPECT_EQ(window_bounds.right(), 1000);
  if (!UseMd()) {  // There is no arrow offset in MD mode.
    EXPECT_EQ(window_bounds.x() +
                  frame.bubble_border()->GetArrowOffset(window_bounds.size()),
              925);
  }

  // Test bubble not fitting top screen edge.
  if (!UseMd()) {  // Moving the bubble by setting the arrow offset doesn't work
                   // in MD mode since there is no arrow displayed.
    frame.bubble_border()->set_arrow(BubbleBorder::LEFT_CENTER);
    window_bounds = frame.GetUpdatedWindowBounds(
        gfx::Rect(100, 100, 50, 50),  // |anchor_rect|
        gfx::Size(500, 500),          // |client_size|
        true);                        // |adjust_if_offscreen|
    EXPECT_EQ(BubbleBorder::LEFT_CENTER, frame.bubble_border()->arrow());
    EXPECT_EQ(window_bounds.y(), 0);
    EXPECT_EQ(window_bounds.y() +
                  frame.bubble_border()->GetArrowOffset(window_bounds.size()),
              125);

    frame.bubble_border()->set_arrow(BubbleBorder::RIGHT_CENTER);
    window_bounds = frame.GetUpdatedWindowBounds(
        gfx::Rect(900, 100, 50, 50),  // |anchor_rect|
        gfx::Size(500, 500),          // |client_size|
        true);                        // |adjust_if_offscreen|
    EXPECT_EQ(BubbleBorder::RIGHT_CENTER, frame.bubble_border()->arrow());
    EXPECT_EQ(window_bounds.y(), 0);
    EXPECT_EQ(window_bounds.y() +
                  frame.bubble_border()->GetArrowOffset(window_bounds.size()),
              125);

    // Test bubble not fitting bottom screen edge.
    frame.bubble_border()->set_arrow(BubbleBorder::LEFT_CENTER);
    window_bounds = frame.GetUpdatedWindowBounds(
        gfx::Rect(100, 900, 50, 50),  // |anchor_rect|
        gfx::Size(500, 500),          // |client_size|
        true);                        // |adjust_if_offscreen|
    EXPECT_EQ(BubbleBorder::LEFT_CENTER, frame.bubble_border()->arrow());
    EXPECT_EQ(window_bounds.bottom(), 1000);
    EXPECT_EQ(window_bounds.y() +
                  frame.bubble_border()->GetArrowOffset(window_bounds.size()),
              925);

    frame.bubble_border()->set_arrow(BubbleBorder::RIGHT_CENTER);
    window_bounds = frame.GetUpdatedWindowBounds(
        gfx::Rect(900, 900, 50, 50),  // |anchor_rect|
        gfx::Size(500, 500),          // |client_size|
        true);                        // |adjust_if_offscreen|
    EXPECT_EQ(BubbleBorder::RIGHT_CENTER, frame.bubble_border()->arrow());
    EXPECT_EQ(window_bounds.bottom(), 1000);
    EXPECT_EQ(window_bounds.y() +
                  frame.bubble_border()->GetArrowOffset(window_bounds.size()),
              925);
  }
}

TEST_F(BubbleFrameViewTest, GetPreferredSize) {
  // Test border/insets.
  TestBubbleFrameView frame(this);
  gfx::Rect preferred_rect(frame.GetPreferredSize());
  // Expect that a border has been added to the preferred size.
  preferred_rect.Inset(frame.bubble_border()->GetInsets());

  gfx::Size expected_size(kPreferredClientWidth + kExpectedAdditionalWidth,
                          kPreferredClientHeight + kExpectedAdditionalHeight);
  EXPECT_EQ(expected_size, preferred_rect.size());
}

TEST_F(BubbleFrameViewTest, GetPreferredSizeWithFootnote) {
  // Test footnote view: adding a footnote should increase the preferred size,
  // but only when the footnote is visible.
  TestBubbleFrameView frame(this);

  constexpr int kFootnoteHeight = 20;
  const gfx::Size no_footnote_size = frame.GetPreferredSize();
  View* footnote = new StaticSizedView(gfx::Size(10, kFootnoteHeight));
  footnote->SetVisible(false);
  frame.SetFootnoteView(footnote);
  EXPECT_EQ(no_footnote_size, frame.GetPreferredSize());  // No change.

  footnote->SetVisible(true);
  gfx::Size with_footnote_size = no_footnote_size;
  constexpr int kFootnoteTopBorderThickness = 1;
  with_footnote_size.Enlarge(0, kFootnoteHeight + kFootnoteTopBorderThickness +
                                    frame.content_margins().height());
  EXPECT_EQ(with_footnote_size, frame.GetPreferredSize());

  footnote->SetVisible(false);
  EXPECT_EQ(no_footnote_size, frame.GetPreferredSize());
}

TEST_F(BubbleFrameViewTest, GetMinimumSize) {
  TestBubbleFrameView frame(this);
  gfx::Rect minimum_rect(frame.GetMinimumSize());
  // Expect that a border has been added to the minimum size.
  minimum_rect.Inset(frame.bubble_border()->GetInsets());

  gfx::Size expected_size(kMinimumClientWidth + kExpectedAdditionalWidth,
                          kMinimumClientHeight + kExpectedAdditionalHeight);
  EXPECT_EQ(expected_size, minimum_rect.size());
}

TEST_F(BubbleFrameViewTest, GetMaximumSize) {
  TestBubbleFrameView frame(this);
  gfx::Rect maximum_rect(frame.GetMaximumSize());
#if defined(OS_WIN)
  // On Windows, GetMaximumSize causes problems with DWM, so it should just be 0
  // (unlimited). See http://crbug.com/506206.
  EXPECT_EQ(gfx::Size(), maximum_rect.size());
#else
  maximum_rect.Inset(frame.bubble_border()->GetInsets());

  // Should ignore the contents view's maximum size and use the preferred size.
  gfx::Size expected_size(kPreferredClientWidth + kExpectedAdditionalWidth,
                          kPreferredClientHeight + kExpectedAdditionalHeight);
  EXPECT_EQ(expected_size, maximum_rect.size());
#endif
}

namespace {

class TestBubbleDialogDelegateView : public BubbleDialogDelegateView {
 public:
  TestBubbleDialogDelegateView()
      : BubbleDialogDelegateView(nullptr, BubbleBorder::NONE) {
    set_shadow(BubbleBorder::NO_ASSETS);
    SetAnchorRect(gfx::Rect());
  }
  ~TestBubbleDialogDelegateView() override {}

  void ChangeTitle(const base::string16& title) {
    title_ = title;
    // Note UpdateWindowTitle() always does a layout, which will be invalid if
    // the Widget needs to change size. But also SizeToContents() _only_ does a
    // layout if the size is actually changing.
    GetWidget()->UpdateWindowTitle();
    SizeToContents();
  }

  void set_override_snap(bool value) { override_snap_ = value; }

  // BubbleDialogDelegateView:
  using BubbleDialogDelegateView::SetAnchorView;
  using BubbleDialogDelegateView::SizeToContents;
  base::string16 GetWindowTitle() const override { return title_; }
  bool ShouldShowWindowTitle() const override { return !title_.empty(); }

  void DeleteDelegate() override {
    // This delegate is owned by the test case itself, so it should not delete
    // itself here.
  }
  int GetDialogButtons() const override { return ui::DIALOG_BUTTON_OK; }
  bool ShouldSnapFrameWidth() const override {
    return override_snap_.value_or(
        BubbleDialogDelegateView::ShouldSnapFrameWidth());
  }
  gfx::Size GetMinimumSize() const override { return gfx::Size(); }
  gfx::Size CalculatePreferredSize() const override {
    return gfx::Size(200, 200);
  }

 private:
  base::string16 title_;
  base::Optional<bool> override_snap_;

  DISALLOW_COPY_AND_ASSIGN(TestBubbleDialogDelegateView);
};

class TestAnchor {
 public:
  explicit TestAnchor(Widget::InitParams params) {
    params.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
    widget_.Init(params);
    widget_.Show();
  }

  Widget& widget() { return widget_; }

 private:
  Widget widget_;

  DISALLOW_COPY_AND_ASSIGN(TestAnchor);
};

}  // namespace

// This test ensures that if the installed LayoutProvider snaps dialog widths,
// BubbleFrameView correctly sizes itself to that width.
TEST_F(BubbleFrameViewTest, WidthSnaps) {
  test::TestLayoutProvider provider;
  TestBubbleDialogDelegateView delegate;
  TestAnchor anchor(CreateParams(Widget::InitParams::TYPE_WINDOW));

  delegate.SetAnchorView(anchor.widget().GetContentsView());
  delegate.set_margins(gfx::Insets());

  Widget* w0 = BubbleDialogDelegateView::CreateBubble(&delegate);
  w0->Show();
  EXPECT_EQ(delegate.GetPreferredSize().width(),
            w0->GetWindowBoundsInScreen().width());
  w0->CloseNow();

  constexpr int kTestWidth = 300;
  provider.SetSnappedDialogWidth(kTestWidth);

  // The Widget's snapped width should exactly match the width returned by the
  // LayoutProvider.
  Widget* w1 = BubbleDialogDelegateView::CreateBubble(&delegate);
  w1->Show();
  EXPECT_EQ(kTestWidth, w1->GetWindowBoundsInScreen().width());
  w1->CloseNow();

  // If the DialogDelegate asks not to snap, it should not snap.
  delegate.set_override_snap(false);
  Widget* w2 = BubbleDialogDelegateView::CreateBubble(&delegate);
  w2->Show();
  EXPECT_EQ(delegate.GetPreferredSize().width(),
            w2->GetWindowBoundsInScreen().width());
  w2->CloseNow();
}

// Tests edge cases when the frame's title view starts to wrap text. This is to
// ensure that the calculations BubbleFrameView does to determine the Widget
// size for a given client view are consistent with the eventual size that the
// client view takes after Layout().
TEST_F(BubbleFrameViewTest, LayoutEdgeCases) {
  test::TestLayoutProvider provider;
  TestBubbleDialogDelegateView delegate;
  TestAnchor anchor(CreateParams(Widget::InitParams::TYPE_WINDOW));
  delegate.SetAnchorView(anchor.widget().GetContentsView());

  Widget* bubble = BubbleDialogDelegateView::CreateBubble(&delegate);
  bubble->Show();

  // Even though the bubble has default margins, the dialog view should have
  // been given its preferred size.
  EXPECT_FALSE(delegate.margins().IsEmpty());
  EXPECT_EQ(delegate.size(), delegate.GetPreferredSize());

  // Starting with a short title.
  base::string16 title(1, 'i');
  delegate.ChangeTitle(title);
  const int min_bubble_height = bubble->GetWindowBoundsInScreen().height();
  EXPECT_LT(delegate.GetPreferredSize().height(), min_bubble_height);

  // Grow the title incrementally until word wrap is required. There should
  // never be a point where the BubbleFrameView over- or under-estimates the
  // size required for the title. If it did, it would cause SizeToContents() to
  // Widget size requiring the subsequent Layout() to fill the remaining client
  // area with something other than |delegate|'s preferred size.
  while (bubble->GetWindowBoundsInScreen().height() == min_bubble_height) {
    title += ' ';
    title += 'i';
    delegate.ChangeTitle(title);
    EXPECT_EQ(delegate.GetPreferredSize(), delegate.size()) << title;
  }

  // Sanity check that something interesting happened. The bubble should have
  // grown by "a line" for the wrapped title, and the title should have reached
  // a length that would have likely caused word wrap. A typical result would be
  // a +17-20 change in height and title length of 53 characters.
  const int two_line_height = bubble->GetWindowBoundsInScreen().height();
  EXPECT_LT(12, two_line_height - min_bubble_height);
  EXPECT_GT(25, two_line_height - min_bubble_height);
  EXPECT_LT(30u, title.size());
  EXPECT_GT(80u, title.size());

  // Now add dialog snapping.
  provider.SetSnappedDialogWidth(300);
  delegate.SizeToContents();

  // Height should go back to |min_bubble_height| since the window is wider:
  // word wrapping should no longer happen.
  EXPECT_EQ(min_bubble_height, bubble->GetWindowBoundsInScreen().height());
  EXPECT_EQ(300, bubble->GetWindowBoundsInScreen().width());

  // Now we are allowed to diverge from the client view width, but not height.
  EXPECT_EQ(delegate.GetPreferredSize().height(), delegate.height());
  EXPECT_LT(delegate.GetPreferredSize().width(), delegate.width());
  EXPECT_GT(300, delegate.width());  // Greater, since there are margins.

  const gfx::Size snapped_size = delegate.size();
  const size_t old_title_size = title.size();

  // Grow the title again with width snapping until word wrapping occurs.
  while (bubble->GetWindowBoundsInScreen().height() == min_bubble_height) {
    title += ' ';
    title += 'i';
    delegate.ChangeTitle(title);
    EXPECT_EQ(snapped_size, delegate.size()) << title;
  }
  // Change to the height should have been the same as before. Title should
  // have grown about 50%.
  EXPECT_EQ(two_line_height, bubble->GetWindowBoundsInScreen().height());
  EXPECT_LT(15u, title.size() - old_title_size);
  EXPECT_GT(40u, title.size() - old_title_size);

  // When |anchor| goes out of scope it should take |bubble| with it.
}

}  // namespace views
