// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ui/views/widget/native_widget_mac.h"

#import <Cocoa/Cocoa.h>

#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#import "testing/gtest_mac.h"
#import "ui/events/test/cocoa_test_event_utils.h"
#include "ui/events/test/event_generator.h"
#import "ui/gfx/mac/coordinate_conversion.h"
#include "ui/views/controls/label.h"
#include "ui/views/native_cursor.h"
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
  // When the activation policy is NSApplicationActivationPolicyRegular, the
  // calls via NSApp are asynchronous, and the run loop needs to be flushed.
  // With NSApplicationActivationPolicyProhibited, the following RunUntilIdle
  // calls are superfluous, but don't hurt.
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(widget->IsVisible());
  EXPECT_FALSE([ns_window isVisible]);
  EXPECT_EQ(3, observer.gained_visible_count());
  EXPECT_EQ(3, observer.lost_visible_count());

  [NSApp unhideWithoutActivation];
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(widget->IsVisible());
  EXPECT_TRUE([ns_window isVisible]);
  EXPECT_EQ(4, observer.gained_visible_count());
  EXPECT_EQ(3, observer.lost_visible_count());

  // Hide again to test unhiding with an activation.
  [NSApp hide:nil];
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(4, observer.lost_visible_count());
  [NSApp unhide:nil];
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(5, observer.gained_visible_count());

  // Hide again to test makeKeyAndOrderFront:.
  [ns_window orderOut:nil];
  EXPECT_FALSE(widget->IsVisible());
  EXPECT_FALSE([ns_window isVisible]);
  EXPECT_EQ(5, observer.gained_visible_count());
  EXPECT_EQ(5, observer.lost_visible_count());

  [ns_window makeKeyAndOrderFront:nil];
  EXPECT_TRUE(widget->IsVisible());
  EXPECT_TRUE([ns_window isVisible]);
  EXPECT_EQ(6, observer.gained_visible_count());
  EXPECT_EQ(5, observer.lost_visible_count());

  // No change when closing.
  widget->CloseNow();
  EXPECT_EQ(5, observer.lost_visible_count());
  EXPECT_EQ(6, observer.gained_visible_count());
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

// Simple view for the SetCursor test that overrides View::GetCursor().
class CursorView : public View {
 public:
  CursorView(int x, NSCursor* cursor) : cursor_(cursor) {
    SetBounds(x, 0, 100, 300);
  }

  // View:
  gfx::NativeCursor GetCursor(const ui::MouseEvent& event) override {
    return cursor_;
  }

 private:
  NSCursor* cursor_;

  DISALLOW_COPY_AND_ASSIGN(CursorView);
};

// Test for Widget::SetCursor(). There is no Widget::GetCursor(), so this uses
// -[NSCursor currentCursor] to validate expectations. Note that currentCursor
// is just "the top cursor on the application's cursor stack.", which is why it
// is safe to use this in a non-interactive UI test with the EventGenerator.
TEST_F(NativeWidgetMacTest, SetCursor) {
  NSCursor* arrow = [NSCursor arrowCursor];
  NSCursor* hand = GetNativeHandCursor();
  NSCursor* ibeam = GetNativeIBeamCursor();

  Widget* widget = CreateTopLevelPlatformWidget();
  widget->SetBounds(gfx::Rect(0, 0, 300, 300));
  widget->GetContentsView()->AddChildView(new CursorView(0, hand));
  widget->GetContentsView()->AddChildView(new CursorView(100, ibeam));
  widget->Show();

  // Events used to simulate tracking rectangle updates. These are not passed to
  // toolkit-views, so it only matters whether they are inside or outside the
  // content area.
  NSEvent* event_in_content = cocoa_test_event_utils::MouseEventAtPoint(
      NSMakePoint(100, 100), NSMouseMoved, 0);
  NSEvent* event_out_of_content = cocoa_test_event_utils::MouseEventAtPoint(
      NSMakePoint(-50, -50), NSMouseMoved, 0);

  EXPECT_NE(arrow, hand);
  EXPECT_NE(arrow, ibeam);

  // At the start of the test, the cursor stack should be empty.
  EXPECT_FALSE([NSCursor currentCursor]);

  // Use an event generator to ask views code to set the cursor. However, note
  // that this does not cause Cocoa to generate tracking rectangle updates.
  ui::test::EventGenerator event_generator(GetContext(),
                                           widget->GetNativeWindow());

  // Move the mouse over the first view, then simulate a tracking rectangle
  // update.
  event_generator.MoveMouseTo(gfx::Point(50, 50));
  [widget->GetNativeWindow() cursorUpdate:event_in_content];
  EXPECT_EQ(hand, [NSCursor currentCursor]);

  // A tracking rectangle update not in the content area should forward to
  // the native NSWindow implementation, which sets the arrow cursor.
  [widget->GetNativeWindow() cursorUpdate:event_out_of_content];
  EXPECT_EQ(arrow, [NSCursor currentCursor]);

  // Now move to the second view.
  event_generator.MoveMouseTo(gfx::Point(150, 50));
  [widget->GetNativeWindow() cursorUpdate:event_in_content];
  EXPECT_EQ(ibeam, [NSCursor currentCursor]);

  // Moving to the third view (but remaining in the content area) should also
  // forward to the native NSWindow implementation.
  event_generator.MoveMouseTo(gfx::Point(250, 50));
  [widget->GetNativeWindow() cursorUpdate:event_in_content];
  EXPECT_EQ(arrow, [NSCursor currentCursor]);

  widget->CloseNow();
}

// Tests that an accessibility request from the system makes its way through to
// a views::Label filling the window.
TEST_F(NativeWidgetMacTest, AccessibilityIntegration) {
  Widget* widget = CreateTopLevelPlatformWidget();
  gfx::Rect screen_rect(50, 50, 100, 100);
  widget->SetBounds(screen_rect);

  const base::string16 test_string = base::ASCIIToUTF16("Green");
  views::Label* label = new views::Label(test_string);
  label->SetBounds(0, 0, 100, 100);
  widget->GetContentsView()->AddChildView(label);
  widget->Show();

  // Accessibility hit tests come in Cocoa screen coordinates.
  NSRect nsrect = gfx::ScreenRectToNSRect(screen_rect);
  NSPoint midpoint = NSMakePoint(NSMidX(nsrect), NSMidY(nsrect));

  id hit = [widget->GetNativeWindow() accessibilityHitTest:midpoint];
  id title = [hit accessibilityAttributeValue:NSAccessibilityTitleAttribute];
  EXPECT_NSEQ(title, @"Green");
}

}  // namespace test
}  // namespace views
