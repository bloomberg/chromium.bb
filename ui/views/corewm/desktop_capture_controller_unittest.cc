// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/corewm/capture_controller.h"

#include "base/logging.h"
#include "ui/aura/env.h"
#include "ui/aura/root_window.h"
#include "ui/aura/test/test_window_delegate.h"
#include "ui/events/event.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/view.h"
#include "ui/views/widget/desktop_aura/desktop_native_widget_aura.h"
#include "ui/views/widget/desktop_aura/desktop_screen_position_client.h"
#include "ui/views/widget/root_view.h"
#include "ui/views/widget/widget.h"

namespace views {

typedef ViewsTestBase DesktopCaptureControllerTest;

// NOTE: these tests do native capture, so they have to be in
// interactive_ui_tests.

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
TEST_F(DesktopCaptureControllerTest, CaptureWindowInputEventTest) {
  scoped_ptr<aura::client::ScreenPositionClient> desktop_position_client1;
  scoped_ptr<aura::client::ScreenPositionClient> desktop_position_client2;

  scoped_ptr<Widget> widget1(new Widget());
  Widget::InitParams params = CreateParams(Widget::InitParams::TYPE_POPUP);
  scoped_ptr<corewm::ScopedCaptureClient> scoped_capture_client(
      new corewm::ScopedCaptureClient(params.context->GetRootWindow()));
  aura::client::CaptureClient* capture_client =
      scoped_capture_client->capture_client();
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.bounds = gfx::Rect(50, 50, 650, 650);
  widget1->Init(params);
  internal::RootView* root1 =
      static_cast<internal::RootView*>(widget1->GetRootView());

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
  EXPECT_EQ(reinterpret_cast<aura::Window*>(0),
            capture_client->GetCaptureWindow());

  widget1->GetNativeView()->SetCapture();
  EXPECT_TRUE(widget1->GetNativeView()->HasCapture());
  EXPECT_FALSE(widget2->GetNativeView()->HasCapture());
  EXPECT_EQ(capture_client->GetCaptureWindow(), widget1->GetNativeView());

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
  EXPECT_EQ(capture_client->GetCaptureWindow(), widget2->GetNativeView());

  root2->DispatchGestureEvent(&g1);
  EXPECT_TRUE(v2->received_gesture_event());
  EXPECT_FALSE(v1->received_gesture_event());

  widget1->CloseNow();
  widget2->CloseNow();
  RunPendingMessages();
}

// crbug.com/295342
#if defined(OS_WIN)
#define MAYBE_TransferOnCapture DISABLED_TransferOnCapture
#else
#define MAYBE_TransferOnCapture TransferOnCapture
#endif
// Verifies aura::RootWindow is correctly notified on capture changes.
TEST_F(DesktopCaptureControllerTest, MAYBE_TransferOnCapture) {
  Widget widget1;
  Widget::InitParams params1 =
      CreateParams(views::Widget::InitParams::TYPE_WINDOW);
  params1.native_widget = new DesktopNativeWidgetAura(&widget1);
  params1.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  widget1.Init(params1);
  widget1.Show();

  Widget widget2;
  Widget::InitParams params2 =
      CreateParams(views::Widget::InitParams::TYPE_WINDOW);
  params2.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params2.native_widget = new DesktopNativeWidgetAura(&widget2);
  widget2.Init(params2);
  widget2.Show();

  // Set capture to |widget2|, then |widget1| and lastly release capture.
  aura::Env::GetInstance()->set_mouse_button_flags(1);
  widget2.SetCapture(widget2.GetRootView());
  widget1.SetCapture(widget1.GetRootView());
  widget1.ReleaseCapture();
  // Widget1's RootWindow should still have a non-NULL |mouse_moved_handler_|
  // (since it contains the view that had capture). OTOH Widget2 doesn't contain
  // the Window that had capture and so its |mouse_moved_handler_| should be
  // NULL.
  EXPECT_NE(static_cast<aura::Window*>(NULL),
            widget1.GetNativeView()->GetRootWindow()->mouse_moved_handler());
  EXPECT_EQ(static_cast<aura::Window*>(NULL),
            widget2.GetNativeView()->GetRootWindow()->mouse_moved_handler());
  aura::Env::GetInstance()->set_mouse_button_flags(0);
}

}  // namespace views
