// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ui/views/cocoa/bridged_native_widget.h"

#import <Cocoa/Cocoa.h>

#import "base/mac/mac_util.h"
#import "base/mac/sdk_forward_declarations.h"
#import "ui/base/test/nswindow_fullscreen_notification_waiter.h"
#include "ui/base/hit_test.h"
#import "ui/base/test/windowed_nsnotification_observer.h"
#import "ui/events/test/cocoa_test_event_utils.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/window/native_frame_view.h"

namespace views {
namespace test {

class BridgedNativeWidgetUITest : public test::WidgetTest {
 public:
  BridgedNativeWidgetUITest() {}

  // testing::Test:
  void SetUp() override {
    WidgetTest::SetUp();
    Widget::InitParams init_params =
        CreateParams(Widget::InitParams::TYPE_WINDOW);
    init_params.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
    widget_.reset(new Widget);
    widget_->Init(init_params);
  }

  void TearDown() override {
    // Ensures any compositor is removed before ViewsTestBase tears down the
    // ContextFactory.
    widget_.reset();
    WidgetTest::TearDown();
  }

  NSWindow* test_window() {
    return widget_->GetNativeWindow();
  }

 protected:
  scoped_ptr<Widget> widget_;
};

// Tests for correct fullscreen tracking, regardless of whether it is initiated
// by the Widget code or elsewhere (e.g. by the user).
TEST_F(BridgedNativeWidgetUITest, FullscreenSynchronousState) {
  EXPECT_FALSE(widget_->IsFullscreen());
  if (base::mac::IsOSSnowLeopard())
    return;

  // Allow user-initiated fullscreen changes on the Window.
  [test_window()
      setCollectionBehavior:[test_window() collectionBehavior] |
                            NSWindowCollectionBehaviorFullScreenPrimary];

  base::scoped_nsobject<NSWindowFullscreenNotificationWaiter> waiter(
      [[NSWindowFullscreenNotificationWaiter alloc]
          initWithWindow:test_window()]);
  const gfx::Rect restored_bounds = widget_->GetRestoredBounds();

  // First show the widget. A user shouldn't be able to initiate fullscreen
  // unless the window is visible in the first place.
  widget_->Show();

  // Simulate a user-initiated fullscreen. Note trying to to this again before
  // spinning a runloop will cause Cocoa to emit text to stdio and ignore it.
  [test_window() toggleFullScreen:nil];
  EXPECT_TRUE(widget_->IsFullscreen());
  EXPECT_EQ(restored_bounds, widget_->GetRestoredBounds());

  // Note there's now an animation running. While that's happening, toggling the
  // state should work as expected, but do "nothing".
  widget_->SetFullscreen(false);
  EXPECT_FALSE(widget_->IsFullscreen());
  EXPECT_EQ(restored_bounds, widget_->GetRestoredBounds());
  widget_->SetFullscreen(false);  // Same request - should no-op.
  EXPECT_FALSE(widget_->IsFullscreen());
  EXPECT_EQ(restored_bounds, widget_->GetRestoredBounds());

  widget_->SetFullscreen(true);
  EXPECT_TRUE(widget_->IsFullscreen());
  EXPECT_EQ(restored_bounds, widget_->GetRestoredBounds());

  // Always finish out of fullscreen. Otherwise there are 4 NSWindow objects
  // that Cocoa creates which don't close themselves and will be seen by the Mac
  // test harness on teardown. Note that the test harness will be waiting until
  // all animations complete, since these temporary animation windows will not
  // be removed from the window list until they do.
  widget_->SetFullscreen(false);
  EXPECT_EQ(restored_bounds, widget_->GetRestoredBounds());

  // Now we must wait for the notifications. Since, if the widget is torn down,
  // the NSWindowDelegate is removed, and the pending request to take out of
  // fullscreen is lost. Since a message loop has not yet spun up in this test
  // we can reliably say there will be one enter and one exit, despite all the
  // toggling above.
  [waiter waitForEnterCount:1 exitCount:1];
  EXPECT_EQ(restored_bounds, widget_->GetRestoredBounds());
}

// Test fullscreen without overlapping calls and without changing collection
// behavior on the test window.
TEST_F(BridgedNativeWidgetUITest, FullscreenEnterAndExit) {
  base::scoped_nsobject<NSWindowFullscreenNotificationWaiter> waiter(
      [[NSWindowFullscreenNotificationWaiter alloc]
          initWithWindow:test_window()]);

  EXPECT_FALSE(widget_->IsFullscreen());
  const gfx::Rect restored_bounds = widget_->GetRestoredBounds();
  EXPECT_FALSE(restored_bounds.IsEmpty());

  // Ensure this works without having to change collection behavior as for the
  // test above. Also check that making a hidden widget fullscreen shows it.
  EXPECT_FALSE(widget_->IsVisible());
  widget_->SetFullscreen(true);
  EXPECT_TRUE(widget_->IsVisible());
  if (base::mac::IsOSSnowLeopard()) {
    // On Snow Leopard, SetFullscreen() isn't implemented. But shouldn't crash.
    EXPECT_FALSE(widget_->IsFullscreen());
    return;
  }

  EXPECT_TRUE(widget_->IsFullscreen());
  EXPECT_EQ(restored_bounds, widget_->GetRestoredBounds());

  // Should be zero until the runloop spins.
  EXPECT_EQ(0, [waiter enterCount]);
  [waiter waitForEnterCount:1 exitCount:0];

  // Verify it hasn't exceeded.
  EXPECT_EQ(1, [waiter enterCount]);
  EXPECT_EQ(0, [waiter exitCount]);
  EXPECT_TRUE(widget_->IsFullscreen());
  EXPECT_EQ(restored_bounds, widget_->GetRestoredBounds());

  widget_->SetFullscreen(false);
  EXPECT_FALSE(widget_->IsFullscreen());
  EXPECT_EQ(restored_bounds, widget_->GetRestoredBounds());

  [waiter waitForEnterCount:1 exitCount:1];
  EXPECT_EQ(1, [waiter enterCount]);
  EXPECT_EQ(1, [waiter exitCount]);
  EXPECT_EQ(restored_bounds, widget_->GetRestoredBounds());
}

// Test that Widget::Restore exits fullscreen.
TEST_F(BridgedNativeWidgetUITest, FullscreenRestore) {
  if (base::mac::IsOSSnowLeopard())
    return;

  base::scoped_nsobject<NSWindowFullscreenNotificationWaiter> waiter(
      [[NSWindowFullscreenNotificationWaiter alloc]
          initWithWindow:test_window()]);

  EXPECT_FALSE(widget_->IsFullscreen());
  const gfx::Rect restored_bounds = widget_->GetRestoredBounds();
  EXPECT_FALSE(restored_bounds.IsEmpty());

  widget_->SetFullscreen(true);
  EXPECT_TRUE(widget_->IsFullscreen());
  [waiter waitForEnterCount:1 exitCount:0];

  widget_->Restore();
  EXPECT_FALSE(widget_->IsFullscreen());
  [waiter waitForEnterCount:1 exitCount:1];
  EXPECT_EQ(restored_bounds, widget_->GetRestoredBounds());
}

namespace {

// This is used to wait for reposted events to be seen. We can't just use
// RunPendingMessages() because CGEventPost might not be synchronous.
class HitTestBridgedNativeWidget : public BridgedNativeWidget {
 public:
  explicit HitTestBridgedNativeWidget(NativeWidgetMac* widget)
      : BridgedNativeWidget(widget) {}

  // BridgedNativeWidget:
  bool ShouldRepostPendingLeftMouseDown(NSPoint location_in_window) override {
    bool draggable_before = [ns_view() mouseDownCanMoveWindow];
    bool should_repost = BridgedNativeWidget::ShouldRepostPendingLeftMouseDown(
        location_in_window);
    bool draggable_after = [ns_view() mouseDownCanMoveWindow];

    if (run_loop_ && draggable_before && !draggable_after)
      run_loop_->Quit();

    return should_repost;
  }

  void WaitForIsDraggableChange() {
    base::RunLoop run_loop;
    run_loop_ = &run_loop;
    run_loop.Run();
    run_loop_ = nullptr;
  }

 private:
  base::RunLoop* run_loop_;

  DISALLOW_COPY_AND_ASSIGN(HitTestBridgedNativeWidget);
};

// This is used to return a customized result to NonClientHitTest.
class HitTestNonClientFrameView : public NativeFrameView {
 public:
  explicit HitTestNonClientFrameView(Widget* widget)
      : NativeFrameView(widget), hit_test_result_(HTNOWHERE) {}

  // NonClientFrameView overrides:
  int NonClientHitTest(const gfx::Point& point) override {
    return hit_test_result_;
  }

  void set_hit_test_result(int component) { hit_test_result_ = component; }

 private:
  int hit_test_result_;

  DISALLOW_COPY_AND_ASSIGN(HitTestNonClientFrameView);
};

void WaitForEvent(NSUInteger mask) {
  // Pointer because the handler block captures local variables by copying.
  base::RunLoop run_loop;
  base::RunLoop* run_loop_ref = &run_loop;
  id monitor = [NSEvent
      addLocalMonitorForEventsMatchingMask:mask
                                   handler:^NSEvent*(NSEvent* ns_event) {
                                     run_loop_ref->Quit();
                                     return ns_event;
                                   }];
  run_loop.Run();
  [NSEvent removeMonitor:monitor];
}

}  // namespace

// This is used to inject test versions of NativeFrameView and
// BridgedNativeWidget.
class HitTestNativeWidgetMac : public NativeWidgetMac {
 public:
  HitTestNativeWidgetMac(internal::NativeWidgetDelegate* delegate,
                         NativeFrameView* native_frame_view)
      : NativeWidgetMac(delegate), native_frame_view_(native_frame_view) {
    NativeWidgetMac::bridge_.reset(new HitTestBridgedNativeWidget(this));
  }

  HitTestBridgedNativeWidget* bridge() {
    return static_cast<HitTestBridgedNativeWidget*>(
        NativeWidgetMac::bridge_.get());
  }

  // internal::NativeWidgetPrivate:
  NonClientFrameView* CreateNonClientFrameView() override {
    return native_frame_view_;
  }

 private:
  // Owned by Widget.
  NativeFrameView* native_frame_view_;

  DISALLOW_COPY_AND_ASSIGN(HitTestNativeWidgetMac);
};

TEST_F(BridgedNativeWidgetUITest, HitTest) {
  Widget widget;
  HitTestNonClientFrameView* frame_view =
      new HitTestNonClientFrameView(&widget);
  test::HitTestNativeWidgetMac* native_widget =
      new test::HitTestNativeWidgetMac(&widget, frame_view);
  Widget::InitParams init_params =
      CreateParams(Widget::InitParams::TYPE_WINDOW);
  init_params.native_widget = native_widget;
  init_params.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  init_params.bounds = gfx::Rect(100, 200, 400, 300);
  widget.Init(init_params);
  widget.Show();

  // Dragging the window should work.
  frame_view->set_hit_test_result(HTCAPTION);
  {
    EXPECT_EQ(100, [widget.GetNativeWindow() frame].origin.x);

    NSEvent* mouse_down = cocoa_test_event_utils::LeftMouseDownAtPointInWindow(
        NSMakePoint(10, 10), widget.GetNativeWindow());
    CGEventPost(kCGSessionEventTap, [mouse_down CGEvent]);
    native_widget->bridge()->WaitForIsDraggableChange();

    base::scoped_nsobject<WindowedNSNotificationObserver> ns_observer(
        [[WindowedNSNotificationObserver alloc]
            initForNotification:NSWindowDidMoveNotification]);
    NSEvent* mouse_drag = cocoa_test_event_utils::MouseEventAtPointInWindow(
        NSMakePoint(110, 110), NSLeftMouseDragged, widget.GetNativeWindow(), 0);
    CGEventPost(kCGSessionEventTap, [mouse_drag CGEvent]);
    [ns_observer wait];
    EXPECT_EQ(200, [widget.GetNativeWindow() frame].origin.x);

    NSEvent* mouse_up = cocoa_test_event_utils::MouseEventAtPointInWindow(
        NSMakePoint(10, 10), NSLeftMouseUp, widget.GetNativeWindow(), 0);
    CGEventPost(kCGSessionEventTap, [mouse_up CGEvent]);
    WaitForEvent(NSLeftMouseUpMask);
    EXPECT_EQ(200, [widget.GetNativeWindow() frame].origin.x);
  }

  // Mouse-downs on the window controls should not be intercepted.
  {
    EXPECT_EQ(200, [widget.GetNativeWindow() frame].origin.x);

    base::scoped_nsobject<WindowedNSNotificationObserver> ns_observer(
        [[WindowedNSNotificationObserver alloc]
            initForNotification:NSWindowDidMiniaturizeNotification]);

    // Position this on the minimize button.
    NSEvent* mouse_down = cocoa_test_event_utils::LeftMouseDownAtPointInWindow(
        NSMakePoint(30, 290), widget.GetNativeWindow());
    CGEventPost(kCGSessionEventTap, [mouse_down CGEvent]);

    NSEvent* mouse_up = cocoa_test_event_utils::MouseEventAtPointInWindow(
        NSMakePoint(30, 290), NSLeftMouseUp, widget.GetNativeWindow(), 0);
    EXPECT_FALSE([widget.GetNativeWindow() isMiniaturized]);
    CGEventPost(kCGSessionEventTap, [mouse_up CGEvent]);
    [ns_observer wait];
    EXPECT_TRUE([widget.GetNativeWindow() isMiniaturized]);
    [widget.GetNativeWindow() deminiaturize:nil];

    // Position unchanged.
    EXPECT_EQ(200, [widget.GetNativeWindow() frame].origin.x);
  }

  // Non-draggable areas should do nothing.
  frame_view->set_hit_test_result(HTCLIENT);
  {
    EXPECT_EQ(200, [widget.GetNativeWindow() frame].origin.x);

    NSEvent* mouse_down = cocoa_test_event_utils::LeftMouseDownAtPointInWindow(
        NSMakePoint(10, 10), widget.GetNativeWindow());
    CGEventPost(kCGSessionEventTap, [mouse_down CGEvent]);
    WaitForEvent(NSLeftMouseDownMask);

    NSEvent* mouse_drag = cocoa_test_event_utils::MouseEventAtPointInWindow(
        NSMakePoint(110, 110), NSLeftMouseDragged, widget.GetNativeWindow(), 0);
    CGEventPost(kCGSessionEventTap, [mouse_drag CGEvent]);
    WaitForEvent(NSLeftMouseDraggedMask);
    EXPECT_EQ(200, [widget.GetNativeWindow() frame].origin.x);

    NSEvent* mouse_up = cocoa_test_event_utils::MouseEventAtPointInWindow(
        NSMakePoint(110, 110), NSLeftMouseUp, widget.GetNativeWindow(), 0);
    CGEventPost(kCGSessionEventTap, [mouse_up CGEvent]);
    WaitForEvent(NSLeftMouseUpMask);
    EXPECT_EQ(200, [widget.GetNativeWindow() frame].origin.x);
  }
}

}  // namespace test
}  // namespace views
