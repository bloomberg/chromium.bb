// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ui/views/widget/native_widget_mac.h"

#import <Cocoa/Cocoa.h>

#include "ui/views/test/test_widget_observer.h"
#include "ui/views/test/widget_test.h"

namespace views {
namespace test {

// Tests for parts of NativeWidgetMac not covered by BridgedNativeWidget, which
// need access to Cocoa APIs.
typedef WidgetTest NativeWidgetMacTest;

class WidgetChangeObserver : public TestWidgetObserver {
 public:
  WidgetChangeObserver(Widget* widget)
      : TestWidgetObserver(widget),
        gained_visible_count_(0),
        lost_visible_count_(0) {}

  int gained_visible_count() const { return gained_visible_count_; }
  int lost_visible_count() const { return lost_visible_count_; }

 private:
  // WidgetObserver:
  void OnWidgetVisibilityChanged(Widget* widget,
                                 bool visible) override {
    ++(visible ? gained_visible_count_ : lost_visible_count_);
  }

  int gained_visible_count_;
  int lost_visible_count_;

  DISALLOW_COPY_AND_ASSIGN(WidgetChangeObserver);
};

// Test visibility states triggered externally.
TEST_F(NativeWidgetMacTest, HideAndShowExternally) {
  Widget* widget = CreateTopLevelPlatformWidget();
  NSWindow* ns_window = widget->GetNativeWindow();
  WidgetChangeObserver observer(widget);

  // Should initially be hidden.
  EXPECT_FALSE(widget->IsVisible());
  EXPECT_FALSE([ns_window isVisible]);
  EXPECT_EQ(0, observer.gained_visible_count());
  EXPECT_EQ(0, observer.lost_visible_count());

  widget->Show();
  EXPECT_TRUE(widget->IsVisible());
  EXPECT_TRUE([ns_window isVisible]);
  EXPECT_EQ(1, observer.gained_visible_count());
  EXPECT_EQ(0, observer.lost_visible_count());

  widget->Hide();
  EXPECT_FALSE(widget->IsVisible());
  EXPECT_FALSE([ns_window isVisible]);
  EXPECT_EQ(1, observer.gained_visible_count());
  EXPECT_EQ(1, observer.lost_visible_count());

  widget->Show();
  EXPECT_TRUE(widget->IsVisible());
  EXPECT_TRUE([ns_window isVisible]);
  EXPECT_EQ(2, observer.gained_visible_count());
  EXPECT_EQ(1, observer.lost_visible_count());

  // Test when hiding individual windows.
  [ns_window orderOut:nil];
  EXPECT_FALSE(widget->IsVisible());
  EXPECT_FALSE([ns_window isVisible]);
  EXPECT_EQ(2, observer.gained_visible_count());
  EXPECT_EQ(2, observer.lost_visible_count());

  [ns_window orderFront:nil];
  EXPECT_TRUE(widget->IsVisible());
  EXPECT_TRUE([ns_window isVisible]);
  EXPECT_EQ(3, observer.gained_visible_count());
  EXPECT_EQ(2, observer.lost_visible_count());

  // Test when hiding the entire application. This doesn't send an orderOut:
  // to the NSWindow.
  [NSApp hide:nil];
  EXPECT_FALSE(widget->IsVisible());
  EXPECT_FALSE([ns_window isVisible]);
  EXPECT_EQ(3, observer.gained_visible_count());
  EXPECT_EQ(3, observer.lost_visible_count());

  [NSApp unhideWithoutActivation];
  EXPECT_TRUE(widget->IsVisible());
  EXPECT_TRUE([ns_window isVisible]);
  EXPECT_EQ(4, observer.gained_visible_count());
  EXPECT_EQ(3, observer.lost_visible_count());

  // Hide again to test unhiding with an activation.
  [NSApp hide:nil];
  EXPECT_EQ(4, observer.lost_visible_count());
  [NSApp unhide:nil];
  EXPECT_EQ(5, observer.gained_visible_count());

  // No change when closing.
  widget->CloseNow();
  EXPECT_EQ(4, observer.lost_visible_count());
  EXPECT_EQ(5, observer.gained_visible_count());
}

// A view that counts calls to OnPaint().
class PaintCountView : public View {
 public:
  PaintCountView() : paint_count_(0) {
    SetBounds(0, 0, 100, 100);
  }

  // View:
  void OnPaint(gfx::Canvas* canvas) override {
    EXPECT_TRUE(GetWidget()->IsVisible());
    ++paint_count_;
  }

  int paint_count() { return paint_count_; }

 private:
  int paint_count_;

  DISALLOW_COPY_AND_ASSIGN(PaintCountView);
};

// Test minimized states triggered externally, implied visibility and restored
// bounds whilst minimized.
TEST_F(NativeWidgetMacTest, MiniaturizeExternally) {
  Widget* widget = new Widget;
  Widget::InitParams init_params(Widget::InitParams::TYPE_WINDOW);
  // Don't add a layer, so that calls to paint can be observed synchronously.
  init_params.layer_type = aura::WINDOW_LAYER_NONE;
  widget->Init(init_params);

  PaintCountView* view = new PaintCountView();
  widget->GetContentsView()->AddChildView(view);
  NSWindow* ns_window = widget->GetNativeWindow();
  WidgetChangeObserver observer(widget);

  widget->SetBounds(gfx::Rect(100, 100, 300, 300));

  EXPECT_TRUE(view->IsDrawn());
  EXPECT_EQ(0, view->paint_count());
  widget->Show();

  EXPECT_EQ(1, observer.gained_visible_count());
  EXPECT_EQ(0, observer.lost_visible_count());
  const gfx::Rect restored_bounds = widget->GetRestoredBounds();
  EXPECT_FALSE(restored_bounds.IsEmpty());
  EXPECT_FALSE(widget->IsMinimized());
  EXPECT_TRUE(widget->IsVisible());

  // Showing should paint.
  EXPECT_EQ(1, view->paint_count());

  // First try performMiniaturize:, which requires a minimize button. Note that
  // Cocoa just blocks the UI thread during the animation, so no need to do
  // anything fancy to wait for it finish.
  [ns_window performMiniaturize:nil];

  EXPECT_TRUE(widget->IsMinimized());
  EXPECT_FALSE(widget->IsVisible());  // Minimizing also makes things invisible.
  EXPECT_EQ(1, observer.gained_visible_count());
  EXPECT_EQ(1, observer.lost_visible_count());
  EXPECT_EQ(restored_bounds, widget->GetRestoredBounds());

  // No repaint when minimizing. But note that this is partly due to not calling
  // [NSView setNeedsDisplay:YES] on the content view. The superview, which is
  // an NSThemeFrame, would repaint |view| if we had, because the miniaturize
  // button is highlighted for performMiniaturize.
  EXPECT_EQ(1, view->paint_count());

  [ns_window deminiaturize:nil];

  EXPECT_FALSE(widget->IsMinimized());
  EXPECT_TRUE(widget->IsVisible());
  EXPECT_EQ(2, observer.gained_visible_count());
  EXPECT_EQ(1, observer.lost_visible_count());
  EXPECT_EQ(restored_bounds, widget->GetRestoredBounds());

  EXPECT_EQ(2, view->paint_count());  // A single paint when deminiaturizing.
  EXPECT_FALSE([ns_window isMiniaturized]);

  widget->Minimize();

  EXPECT_TRUE(widget->IsMinimized());
  EXPECT_TRUE([ns_window isMiniaturized]);
  EXPECT_EQ(2, observer.gained_visible_count());
  EXPECT_EQ(2, observer.lost_visible_count());
  EXPECT_EQ(restored_bounds, widget->GetRestoredBounds());
  EXPECT_EQ(2, view->paint_count());  // No paint when miniaturizing.

  widget->Restore();  // If miniaturized, should deminiaturize.

  EXPECT_FALSE(widget->IsMinimized());
  EXPECT_FALSE([ns_window isMiniaturized]);
  EXPECT_EQ(3, observer.gained_visible_count());
  EXPECT_EQ(2, observer.lost_visible_count());
  EXPECT_EQ(restored_bounds, widget->GetRestoredBounds());
  EXPECT_EQ(3, view->paint_count());

  widget->Restore();  // If not miniaturized, does nothing.

  EXPECT_FALSE(widget->IsMinimized());
  EXPECT_FALSE([ns_window isMiniaturized]);
  EXPECT_EQ(3, observer.gained_visible_count());
  EXPECT_EQ(2, observer.lost_visible_count());
  EXPECT_EQ(restored_bounds, widget->GetRestoredBounds());
  EXPECT_EQ(3, view->paint_count());

  widget->CloseNow();

  // Create a widget without a minimize button.
  widget = CreateTopLevelFramelessPlatformWidget();
  ns_window = widget->GetNativeWindow();
  widget->SetBounds(gfx::Rect(100, 100, 300, 300));
  widget->Show();
  EXPECT_FALSE(widget->IsMinimized());

  // This should fail, since performMiniaturize: requires a minimize button.
  [ns_window performMiniaturize:nil];
  EXPECT_FALSE(widget->IsMinimized());

  // But this should work.
  widget->Minimize();
  EXPECT_TRUE(widget->IsMinimized());

  // Test closing while minimized.
  widget->CloseNow();
}

}  // namespace test
}  // namespace views
