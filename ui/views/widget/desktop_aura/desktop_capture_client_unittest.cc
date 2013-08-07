// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/widget/desktop_aura/desktop_capture_client.h"

#include "ui/aura/root_window.h"
#include "ui/aura/test/aura_test_base.h"
#include "ui/aura/test/test_screen.h"
#include "ui/aura/test/test_window_delegate.h"
#include "ui/base/events/event.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/view.h"
#include "ui/views/widget/desktop_aura/desktop_screen_position_client.h"
#include "ui/views/widget/root_view.h"
#include "ui/views/widget/widget.h"

namespace views {

typedef ViewsTestBase ViewTest;

class DesktopCaptureClientTest : public aura::test::AuraTestBase {
 public:
  virtual void SetUp() OVERRIDE {
    AuraTestBase::SetUp();
    desktop_capture_client_.reset(new DesktopCaptureClient(root_window()));

    second_root_.reset(new aura::RootWindow(
        aura::RootWindow::CreateParams(gfx::Rect(0, 0, 800, 600))));
    second_root_->Init();
    second_root_->Show();
    second_root_->SetHostSize(gfx::Size(800, 600));
    second_desktop_capture_client_.reset(
      new DesktopCaptureClient(second_root_.get()));

    desktop_position_client_.reset(new DesktopScreenPositionClient());
    aura::client::SetScreenPositionClient(root_window(),
                                          desktop_position_client_.get());

    second_desktop_position_client_.reset(new DesktopScreenPositionClient());
    aura::client::SetScreenPositionClient(
        second_root_.get(),
        second_desktop_position_client_.get());
  }

  virtual void TearDown() OVERRIDE {
    RunAllPendingInMessageLoop();

    second_desktop_position_client_.reset();
    second_desktop_capture_client_.reset();

    // Kill any active compositors before we hit the compositor shutdown paths.
    second_root_.reset();

    desktop_position_client_.reset();
    desktop_capture_client_.reset();

    AuraTestBase::TearDown();
  }

  scoped_ptr<DesktopCaptureClient> desktop_capture_client_;
  scoped_ptr<aura::RootWindow> second_root_;
  scoped_ptr<DesktopCaptureClient> second_desktop_capture_client_;
  scoped_ptr<aura::client::ScreenPositionClient> desktop_position_client_;
  scoped_ptr<aura::client::ScreenPositionClient>
      second_desktop_position_client_;
};

// Makes sure that internal details that are set on mouse down (such as
// mouse_pressed_handler()) are cleared when another root window takes capture.
TEST_F(DesktopCaptureClientTest, ResetMouseEventHandlerOnCapture) {
  // Create a window inside the RootWindow.
  scoped_ptr<aura::Window> w1(CreateNormalWindow(1, root_window(), NULL));

  // Make a synthesized mouse down event. Ensure that the RootWindow will
  // dispatch further mouse events to |w1|.
  ui::MouseEvent mouse_pressed_event(ui::ET_MOUSE_PRESSED, gfx::Point(5, 5),
                                     gfx::Point(5, 5), 0);
  root_window()->AsRootWindowHostDelegate()->OnHostMouseEvent(
      &mouse_pressed_event);
  EXPECT_EQ(w1.get(), root_window()->mouse_pressed_handler());

  // Build a window in the second RootWindow.
  scoped_ptr<aura::Window> w2(CreateNormalWindow(2, second_root_.get(), NULL));

  // The act of having the second window take capture should clear out mouse
  // pressed handler in the first RootWindow.
  w2->SetCapture();
  EXPECT_EQ(NULL, root_window()->mouse_pressed_handler());
}

// Makes sure that when one window gets capture, it forces the release on the
// other. This is needed has to be handled explicitly on Linux, and is a sanity
// check on Windows.
TEST_F(DesktopCaptureClientTest, ResetOtherWindowCaptureOnCapture) {
  // Create a window inside the RootWindow.
  scoped_ptr<aura::Window> w1(CreateNormalWindow(1, root_window(), NULL));
  w1->SetCapture();
  // Both capture clients should return the same capture window.
  EXPECT_EQ(w1.get(), desktop_capture_client_->GetCaptureWindow());
  EXPECT_EQ(w1.get(), second_desktop_capture_client_->GetCaptureWindow());

  // Build a window in the second RootWindow and give it capture. Both capture
  // clients should return the same capture window.
  scoped_ptr<aura::Window> w2(CreateNormalWindow(2, second_root_.get(), NULL));
  w2->SetCapture();
  EXPECT_EQ(w2.get(), desktop_capture_client_->GetCaptureWindow());
  EXPECT_EQ(w2.get(), second_desktop_capture_client_->GetCaptureWindow());
}

// This class provides functionality to verify whether the View instance
// received the gesture event.
class DesktopViewInputTest : public View {
 public:
  DesktopViewInputTest()
      : received_gesture_event_(false) {}

  virtual void OnGestureEvent(ui::GestureEvent* event) OVERRIDE {
    received_gesture_event_ = true;
    return View::OnGestureEvent(event);
  }

  // Resets state maintained by this class.
  void Reset() {
    received_gesture_event_ = false;
  }

  bool received_gesture_event() const { return received_gesture_event_; }

 private:
  bool received_gesture_event_;

  DISALLOW_COPY_AND_ASSIGN(DesktopViewInputTest);
};

// Tests aura::Window capture and whether gesture events are sent to the window
// which has capture.
// The test case creates two visible widgets and sets capture to the underlying
// aura::Windows one by one. It then sends a gesture event and validates whether
// the window which had capture receives the gesture.
TEST_F(ViewTest, CaptureWindowInputEventTest) {
  scoped_ptr<DesktopCaptureClient> desktop_capture_client1;
  scoped_ptr<DesktopCaptureClient> desktop_capture_client2;
  scoped_ptr<aura::client::ScreenPositionClient> desktop_position_client1;
  scoped_ptr<aura::client::ScreenPositionClient> desktop_position_client2;

  scoped_ptr<Widget> widget1(new Widget());
  Widget::InitParams params = CreateParams(Widget::InitParams::TYPE_POPUP);
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.bounds = gfx::Rect(50, 50, 650, 650);
  widget1->Init(params);
  internal::RootView* root1 =
      static_cast<internal::RootView*>(widget1->GetRootView());

  desktop_capture_client1.reset(new DesktopCaptureClient(
      widget1->GetNativeView()->GetRootWindow()));
  desktop_position_client1.reset(new DesktopScreenPositionClient());
  aura::client::SetScreenPositionClient(
      widget1->GetNativeView()->GetRootWindow(),
      desktop_position_client1.get());

  DesktopViewInputTest* v1 = new DesktopViewInputTest();
  v1->SetBoundsRect(gfx::Rect(0, 0, 300, 300));
  root1->AddChildView(v1);
  widget1->Show();

  scoped_ptr<Widget> widget2(new Widget());

  params = CreateParams(Widget::InitParams::TYPE_POPUP);
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.bounds = gfx::Rect(50, 50, 650, 650);
  widget2->Init(params);

  internal::RootView* root2 =
      static_cast<internal::RootView*>(widget2->GetRootView());
  desktop_capture_client2.reset(new DesktopCaptureClient(
      widget2->GetNativeView()->GetRootWindow()));
  desktop_position_client2.reset(new DesktopScreenPositionClient());
  aura::client::SetScreenPositionClient(
      widget2->GetNativeView()->GetRootWindow(),
      desktop_position_client2.get());

  DesktopViewInputTest* v2 = new DesktopViewInputTest();
  v2->SetBoundsRect(gfx::Rect(0, 0, 300, 300));
  root2->AddChildView(v2);
  widget2->Show();

  EXPECT_FALSE(widget1->GetNativeView()->HasCapture());
  EXPECT_FALSE(widget2->GetNativeView()->HasCapture());
  EXPECT_EQ(desktop_capture_client1->GetCaptureWindow(),
            reinterpret_cast<aura::Window*>(0));
  EXPECT_EQ(desktop_capture_client2->GetCaptureWindow(),
            reinterpret_cast<aura::Window*>(0));

  widget1->GetNativeView()->SetCapture();
  EXPECT_TRUE(widget1->GetNativeView()->HasCapture());
  EXPECT_FALSE(widget2->GetNativeView()->HasCapture());
  EXPECT_EQ(desktop_capture_client1->GetCaptureWindow(),
            widget1->GetNativeView());
  EXPECT_EQ(desktop_capture_client2->GetCaptureWindow(),
            widget1->GetNativeView());

  ui::GestureEvent g1(ui::ET_GESTURE_LONG_PRESS, 80, 80, 0,
                      base::TimeDelta(),
                      ui::GestureEventDetails(ui::ET_GESTURE_LONG_PRESS,
                                              0.0f, 0.0f), 0);
  root1->DispatchGestureEvent(&g1);
  EXPECT_TRUE(v1->received_gesture_event());
  EXPECT_FALSE(v2->received_gesture_event());
  v1->Reset();
  v2->Reset();

  widget2->GetNativeView()->SetCapture();

  EXPECT_FALSE(widget1->GetNativeView()->HasCapture());
  EXPECT_TRUE(widget2->GetNativeView()->HasCapture());
  EXPECT_EQ(desktop_capture_client1->GetCaptureWindow(),
            widget2->GetNativeView());
  EXPECT_EQ(desktop_capture_client2->GetCaptureWindow(),
            widget2->GetNativeView());

  root2->DispatchGestureEvent(&g1);
  EXPECT_TRUE(v2->received_gesture_event());
  EXPECT_FALSE(v1->received_gesture_event());

  widget1->CloseNow();
  widget2->CloseNow();
  RunPendingMessages();
}

}  // namespace views
