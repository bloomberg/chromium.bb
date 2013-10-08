// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/run_loop.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/dialog_delegate.h"

#if defined(USE_AURA)
#include "ui/aura/client/activation_client.h"
#include "ui/aura/client/focus_client.h"
#include "ui/aura/env.h"
#include "ui/aura/root_window.h"
#if !defined(OS_CHROMEOS)
#include "ui/views/widget/desktop_aura/desktop_native_widget_aura.h"
#endif
#endif

#if defined(OS_WIN)
#include "ui/views/win/hwnd_util.h"
#endif

namespace views {
namespace test {

namespace {

// A View that closes the Widget and exits the current message-loop when it
// receives a mouse-release event.
class ExitLoopOnRelease : public View {
 public:
  ExitLoopOnRelease() {}
  virtual ~ExitLoopOnRelease() {}

 private:
  // Overridden from View:
  virtual void OnMouseReleased(const ui::MouseEvent& event) OVERRIDE {
    GetWidget()->Close();
    base::MessageLoop::current()->QuitNow();
  }

  DISALLOW_COPY_AND_ASSIGN(ExitLoopOnRelease);
};

// A view that does a capture on gesture-begin events.
class GestureCaptureView : public View {
 public:
  GestureCaptureView() {}
  virtual ~GestureCaptureView() {}

 private:
  // Overridden from View:
  virtual void OnGestureEvent(ui::GestureEvent* event) OVERRIDE {
    if (event->type() == ui::ET_GESTURE_BEGIN) {
      GetWidget()->SetCapture(this);
      event->StopPropagation();
    }
  }

  DISALLOW_COPY_AND_ASSIGN(GestureCaptureView);
};

// A view that always processes all mouse events.
class MouseView : public View {
 public:
  MouseView()
      : View(),
        entered_(0),
        exited_(0),
        pressed_(0) {
  }
  virtual ~MouseView() {}

  virtual bool OnMousePressed(const ui::MouseEvent& event) OVERRIDE {
    pressed_++;
    return true;
  }

  virtual void OnMouseEntered(const ui::MouseEvent& event) OVERRIDE {
    entered_++;
  }

  virtual void OnMouseExited(const ui::MouseEvent& event) OVERRIDE {
    exited_++;
  }

  // Return the number of OnMouseEntered calls and reset the counter.
  int EnteredCalls() {
    int i = entered_;
    entered_ = 0;
    return i;
  }

  // Return the number of OnMouseExited calls and reset the counter.
  int ExitedCalls() {
    int i = exited_;
    exited_ = 0;
    return i;
  }

  int pressed() const { return pressed_; }

 private:
  int entered_;
  int exited_;

  int pressed_;

  DISALLOW_COPY_AND_ASSIGN(MouseView);
};

// A View that shows a different widget, sets capture on that widget, and
// initiates a nested message-loop when it receives a mouse-press event.
class NestedLoopCaptureView : public View {
 public:
  explicit NestedLoopCaptureView(Widget* widget) : widget_(widget) {}
  virtual ~NestedLoopCaptureView() {}

 private:
  // Overridden from View:
  virtual bool OnMousePressed(const ui::MouseEvent& event) OVERRIDE {
    // Start a nested loop.
    widget_->Show();
    widget_->SetCapture(widget_->GetContentsView());
    EXPECT_TRUE(widget_->HasCapture());

    base::MessageLoopForUI* loop = base::MessageLoopForUI::current();
    base::MessageLoop::ScopedNestableTaskAllower allow(loop);

    base::RunLoop run_loop;
#if defined(USE_AURA)
    run_loop.set_dispatcher(aura::Env::GetInstance()->GetDispatcher());
#endif
    run_loop.Run();
    return true;
  }

  Widget* widget_;

  DISALLOW_COPY_AND_ASSIGN(NestedLoopCaptureView);
};

}  // namespace

#if defined(OS_WIN) && defined(USE_AURA)
// Tests whether activation and focus change works correctly in Windows AURA.
// We test the following:-
// 1. If the active aura window is correctly set when a top level widget is
//    created.
// 2. If the active aura window in widget 1 created above, is set to NULL when
//    another top level widget is created and focused.
// 3. On focusing the native platform window for widget 1, the active aura
//    window for widget 1 should be set and that for widget 2 should reset.
// TODO(ananta)
// Discuss with erg on how to write this test for linux x11 aura.
TEST_F(WidgetTest, DesktopNativeWidgetAuraActivationAndFocusTest) {
  // Create widget 1 and expect the active window to be its window.
  View* contents_view1 = new View;
  contents_view1->set_focusable(true);
  Widget widget1;
  Widget::InitParams init_params =
      CreateParams(Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  init_params.bounds = gfx::Rect(0, 0, 200, 200);
  init_params.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  init_params.native_widget = new DesktopNativeWidgetAura(&widget1);
  widget1.Init(init_params);
  widget1.SetContentsView(contents_view1);
  widget1.Show();
  aura::RootWindow* root_window1= widget1.GetNativeView()->GetRootWindow();
  contents_view1->RequestFocus();

  EXPECT_TRUE(root_window1 != NULL);
  aura::client::ActivationClient* activation_client1 =
      aura::client::GetActivationClient(root_window1);
  EXPECT_TRUE(activation_client1 != NULL);
  EXPECT_EQ(activation_client1->GetActiveWindow(), widget1.GetNativeView());

  // Create widget 2 and expect the active window to be its window.
  View* contents_view2 = new View;
  Widget widget2;
  Widget::InitParams init_params2 =
      CreateParams(Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  init_params2.bounds = gfx::Rect(0, 0, 200, 200);
  init_params2.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  init_params2.native_widget = new DesktopNativeWidgetAura(&widget2);
  widget2.Init(init_params2);
  widget2.SetContentsView(contents_view2);
  widget2.Show();
  aura::RootWindow* root_window2 = widget2.GetNativeView()->GetRootWindow();
  contents_view2->RequestFocus();
  ::SetActiveWindow(root_window2->GetAcceleratedWidget());

  aura::client::ActivationClient* activation_client2 =
      aura::client::GetActivationClient(root_window2);
  EXPECT_TRUE(activation_client2 != NULL);
  EXPECT_EQ(activation_client2->GetActiveWindow(), widget2.GetNativeView());
  EXPECT_EQ(activation_client1->GetActiveWindow(),
            reinterpret_cast<aura::Window*>(NULL));

  // Now set focus back to widget 1 and expect the active window to be its
  // window.
  contents_view1->RequestFocus();
  ::SetActiveWindow(root_window1->GetAcceleratedWidget());
  EXPECT_EQ(activation_client2->GetActiveWindow(),
            reinterpret_cast<aura::Window*>(NULL));
  EXPECT_EQ(activation_client1->GetActiveWindow(), widget1.GetNativeView());
}
#endif

TEST_F(WidgetTest, CaptureAutoReset) {
  Widget* toplevel = CreateTopLevelFramelessPlatformWidget();
  View* container = new View;
  toplevel->SetContentsView(container);

  EXPECT_FALSE(toplevel->HasCapture());
  toplevel->SetCapture(NULL);
  EXPECT_TRUE(toplevel->HasCapture());

  // By default, mouse release removes capture.
  gfx::Point click_location(45, 15);
  ui::MouseEvent release(ui::ET_MOUSE_RELEASED, click_location, click_location,
      ui::EF_LEFT_MOUSE_BUTTON);
  toplevel->OnMouseEvent(&release);
  EXPECT_FALSE(toplevel->HasCapture());

  // Now a mouse release shouldn't remove capture.
  toplevel->set_auto_release_capture(false);
  toplevel->SetCapture(NULL);
  EXPECT_TRUE(toplevel->HasCapture());
  toplevel->OnMouseEvent(&release);
  EXPECT_TRUE(toplevel->HasCapture());
  toplevel->ReleaseCapture();
  EXPECT_FALSE(toplevel->HasCapture());

  toplevel->Close();
  RunPendingMessages();
}

TEST_F(WidgetTest, ResetCaptureOnGestureEnd) {
  Widget* toplevel = CreateTopLevelFramelessPlatformWidget();
  View* container = new View;
  toplevel->SetContentsView(container);

  View* gesture = new GestureCaptureView;
  gesture->SetBounds(0, 0, 30, 30);
  container->AddChildView(gesture);

  MouseView* mouse = new MouseView;
  mouse->SetBounds(30, 0, 30, 30);
  container->AddChildView(mouse);

  toplevel->SetSize(gfx::Size(100, 100));
  toplevel->Show();

  // Start a gesture on |gesture|.
  ui::GestureEvent begin(ui::ET_GESTURE_BEGIN,
      15, 15, 0, base::TimeDelta(),
      ui::GestureEventDetails(ui::ET_GESTURE_BEGIN, 0, 0), 1);
  ui::GestureEvent end(ui::ET_GESTURE_END,
      15, 15, 0, base::TimeDelta(),
      ui::GestureEventDetails(ui::ET_GESTURE_END, 0, 0), 1);
  toplevel->OnGestureEvent(&begin);

  // Now try to click on |mouse|. Since |gesture| will have capture, |mouse|
  // will not receive the event.
  gfx::Point click_location(45, 15);

  ui::MouseEvent press(ui::ET_MOUSE_PRESSED, click_location, click_location,
      ui::EF_LEFT_MOUSE_BUTTON);
  ui::MouseEvent release(ui::ET_MOUSE_RELEASED, click_location, click_location,
      ui::EF_LEFT_MOUSE_BUTTON);

  EXPECT_TRUE(toplevel->HasCapture());

  toplevel->OnMouseEvent(&press);
  toplevel->OnMouseEvent(&release);
  EXPECT_EQ(0, mouse->pressed());

  EXPECT_FALSE(toplevel->HasCapture());

  // The end of the gesture should release the capture, and pressing on |mouse|
  // should now reach |mouse|.
  toplevel->OnGestureEvent(&end);
  toplevel->OnMouseEvent(&press);
  toplevel->OnMouseEvent(&release);
  EXPECT_EQ(1, mouse->pressed());

  toplevel->Close();
  RunPendingMessages();
}

// Checks that if a mouse-press triggers a capture on a different widget (which
// consumes the mouse-release event), then the target of the press does not have
// capture.
// Fails on chromium.webkit Windows bot, see crbug.com/264872.
#if defined(OS_WIN)
#define MAYBE_DisableCaptureWidgetFromMousePress\
    DISABLED_CaptureWidgetFromMousePress
#else
#define MAYBE_DisableCaptureWidgetFromMousePress\
    CaptureWidgetFromMousePress
#endif
TEST_F(WidgetTest, MAYBE_DisableCaptureWidgetFromMousePress) {
  // The test creates two widgets: |first| and |second|.
  // The View in |first| makes |second| visible, sets capture on it, and starts
  // a nested loop (like a menu does). The View in |second| terminates the
  // nested loop and closes the widget.
  // The test sends a mouse-press event to |first|, and posts a task to send a
  // release event to |second|, to make sure that the release event is
  // dispatched after the nested loop starts.

  Widget* first = CreateTopLevelFramelessPlatformWidget();
  Widget* second = CreateTopLevelFramelessPlatformWidget();

  View* container = new NestedLoopCaptureView(second);
  first->SetContentsView(container);

  second->SetContentsView(new ExitLoopOnRelease());

  first->SetSize(gfx::Size(100, 100));
  first->Show();

  gfx::Point location(20, 20);
  base::MessageLoop::current()->PostTask(FROM_HERE,
      base::Bind(&Widget::OnMouseEvent,
                 base::Unretained(second),
                 base::Owned(new ui::MouseEvent(ui::ET_MOUSE_RELEASED,
                                                location,
                                                location,
                                                ui::EF_LEFT_MOUSE_BUTTON))));
  ui::MouseEvent press(ui::ET_MOUSE_PRESSED, location, location,
                       ui::EF_LEFT_MOUSE_BUTTON);
  first->OnMouseEvent(&press);
  EXPECT_FALSE(first->HasCapture());
  first->Close();
  RunPendingMessages();
}

// Tests some grab/ungrab events.
// TODO(estade): can this be enabled now that this is an interactive ui test?
TEST_F(WidgetTest, DISABLED_GrabUngrab) {
  Widget* toplevel = CreateTopLevelPlatformWidget();
  Widget* child1 = CreateChildNativeWidgetWithParent(toplevel);
  Widget* child2 = CreateChildNativeWidgetWithParent(toplevel);

  toplevel->SetBounds(gfx::Rect(0, 0, 500, 500));

  child1->SetBounds(gfx::Rect(10, 10, 300, 300));
  View* view = new MouseView();
  view->SetBounds(0, 0, 300, 300);
  child1->GetRootView()->AddChildView(view);

  child2->SetBounds(gfx::Rect(200, 10, 200, 200));
  view = new MouseView();
  view->SetBounds(0, 0, 200, 200);
  child2->GetRootView()->AddChildView(view);

  toplevel->Show();
  RunPendingMessages();

  // Click on child1
  gfx::Point p1(45, 45);
  ui::MouseEvent pressed(ui::ET_MOUSE_PRESSED, p1, p1,
                         ui::EF_LEFT_MOUSE_BUTTON);
  toplevel->OnMouseEvent(&pressed);

  EXPECT_TRUE(toplevel->HasCapture());
  EXPECT_TRUE(child1->HasCapture());
  EXPECT_FALSE(child2->HasCapture());

  ui::MouseEvent released(ui::ET_MOUSE_RELEASED, p1, p1,
                          ui::EF_LEFT_MOUSE_BUTTON);
  toplevel->OnMouseEvent(&released);

  EXPECT_FALSE(toplevel->HasCapture());
  EXPECT_FALSE(child1->HasCapture());
  EXPECT_FALSE(child2->HasCapture());

  RunPendingMessages();

  // Click on child2
  gfx::Point p2(315, 45);
  ui::MouseEvent pressed2(ui::ET_MOUSE_PRESSED, p2, p2,
                          ui::EF_LEFT_MOUSE_BUTTON);
  toplevel->OnMouseEvent(&pressed2);
  EXPECT_TRUE(pressed2.handled());
  EXPECT_TRUE(toplevel->HasCapture());
  EXPECT_TRUE(child2->HasCapture());
  EXPECT_FALSE(child1->HasCapture());

  ui::MouseEvent released2(ui::ET_MOUSE_RELEASED, p2, p2,
                           ui::EF_LEFT_MOUSE_BUTTON);
  toplevel->OnMouseEvent(&released2);
  EXPECT_FALSE(toplevel->HasCapture());
  EXPECT_FALSE(child1->HasCapture());
  EXPECT_FALSE(child2->HasCapture());

  toplevel->CloseNow();
}

// Tests mouse move outside of the window into the "resize controller" and back
// will still generate an OnMouseEntered and OnMouseExited event..
TEST_F(WidgetTest, CheckResizeControllerEvents) {
  Widget* toplevel = CreateTopLevelPlatformWidget();

  toplevel->SetBounds(gfx::Rect(0, 0, 100, 100));

  MouseView* view = new MouseView();
  view->SetBounds(90, 90, 10, 10);
  toplevel->GetRootView()->AddChildView(view);

  toplevel->Show();
  RunPendingMessages();

  // Move to an outside position.
  gfx::Point p1(200, 200);
  ui::MouseEvent moved_out(ui::ET_MOUSE_MOVED, p1, p1, ui::EF_NONE);
  toplevel->OnMouseEvent(&moved_out);
  EXPECT_EQ(0, view->EnteredCalls());
  EXPECT_EQ(0, view->ExitedCalls());

  // Move onto the active view.
  gfx::Point p2(95, 95);
  ui::MouseEvent moved_over(ui::ET_MOUSE_MOVED, p2, p2, ui::EF_NONE);
  toplevel->OnMouseEvent(&moved_over);
  EXPECT_EQ(1, view->EnteredCalls());
  EXPECT_EQ(0, view->ExitedCalls());

  // Move onto the outer resizing border.
  gfx::Point p3(102, 95);
  ui::MouseEvent moved_resizer(ui::ET_MOUSE_MOVED, p3, p3, ui::EF_NONE);
  toplevel->OnMouseEvent(&moved_resizer);
  EXPECT_EQ(0, view->EnteredCalls());
  EXPECT_EQ(1, view->ExitedCalls());

  // Move onto the view again.
  toplevel->OnMouseEvent(&moved_over);
  EXPECT_EQ(1, view->EnteredCalls());
  EXPECT_EQ(0, view->ExitedCalls());

  RunPendingMessages();

  toplevel->CloseNow();
}

#if defined(OS_WIN)

// This class subclasses the Widget class to listen for activation change
// notifications and provides accessors to return information as to whether
// the widget is active. We need this to ensure that users of the widget
// class activate the widget only when the underlying window becomes really
// active. Previously we would activate the widget in the WM_NCACTIVATE
// message which is incorrect because APIs like FlashWindowEx flash the
// window caption by sending fake WM_NCACTIVATE messages.
class WidgetActivationTest : public Widget {
 public:
  WidgetActivationTest()
      : active_(false) {}

  virtual ~WidgetActivationTest() {}

  virtual void OnNativeWidgetActivationChanged(bool active) OVERRIDE {
    active_ = active;
  }

  bool active() const { return active_; }

 private:
  bool active_;

  DISALLOW_COPY_AND_ASSIGN(WidgetActivationTest);
};

// Tests whether the widget only becomes active when the underlying window
// is really active.
TEST_F(WidgetTest, WidgetNotActivatedOnFakeActivationMessages) {
  WidgetActivationTest widget1;
  Widget::InitParams init_params =
      CreateParams(Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  init_params.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
#if defined(USE_AURA)
  init_params.native_widget = new DesktopNativeWidgetAura(&widget1);
#endif
  init_params.bounds = gfx::Rect(0, 0, 200, 200);
  widget1.Init(init_params);
  widget1.Show();
  EXPECT_EQ(true, widget1.active());

  WidgetActivationTest widget2;
#if defined(USE_AURA)
  init_params.native_widget = new DesktopNativeWidgetAura(&widget2);
#endif
  widget2.Init(init_params);
  widget2.Show();
  EXPECT_EQ(true, widget2.active());
  EXPECT_EQ(false, widget1.active());

  HWND win32_native_window1 = HWNDForWidget(&widget1);
  EXPECT_TRUE(::IsWindow(win32_native_window1));

  ::SendMessage(win32_native_window1, WM_NCACTIVATE, 1, 0);
  EXPECT_EQ(false, widget1.active());
  EXPECT_EQ(true, widget2.active());

  ::SetActiveWindow(win32_native_window1);
  EXPECT_EQ(true, widget1.active());
  EXPECT_EQ(false, widget2.active());
}
#endif

#if defined(USE_AURA) && !defined(OS_CHROMEOS)
// Provides functionality to create a window modal dialog.
class ModalDialogDelegate : public DialogDelegateView {
 public:
  ModalDialogDelegate() {}
  virtual ~ModalDialogDelegate() {}

  // WidgetDelegate overrides.
  virtual ui::ModalType GetModalType() const OVERRIDE {
    return ui::MODAL_TYPE_WINDOW;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ModalDialogDelegate);
};

// Tests whether the focused window is set correctly when a modal window is
// created and destroyed. When it is destroyed it should focus the owner
// window.
TEST_F(WidgetTest, WindowModalWindowDestroyedActivationTest) {
  // Create a top level widget.
  Widget top_level_widget;
  Widget::InitParams init_params =
      CreateParams(Widget::InitParams::TYPE_WINDOW);
  init_params.show_state = ui::SHOW_STATE_NORMAL;
  gfx::Rect initial_bounds(0, 0, 500, 500);
  init_params.bounds = initial_bounds;
  init_params.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  init_params.native_widget = new DesktopNativeWidgetAura(&top_level_widget);
  top_level_widget.Init(init_params);
  top_level_widget.Show();

  aura::Window* top_level_window = top_level_widget.GetNativeWindow();
  EXPECT_EQ(top_level_window, aura::client::GetFocusClient(
                top_level_window)->GetFocusedWindow());

  // Create a modal dialog.
  // This instance will be destroyed when the dialog is destroyed.
  ModalDialogDelegate* dialog_delegate = new ModalDialogDelegate;

  Widget* modal_dialog_widget = views::DialogDelegate::CreateDialogWidget(
      dialog_delegate, NULL, top_level_widget.GetNativeWindow());
  modal_dialog_widget->SetBounds(gfx::Rect(100, 100, 200, 200));
  modal_dialog_widget->Show();
  aura::Window* dialog_window = modal_dialog_widget->GetNativeWindow();
  EXPECT_EQ(dialog_window, aura::client::GetFocusClient(
                top_level_window)->GetFocusedWindow());

  modal_dialog_widget->CloseNow();
  EXPECT_EQ(top_level_window, aura::client::GetFocusClient(
                top_level_window)->GetFocusedWindow());
  top_level_widget.CloseNow();
}
#endif

}  // namespace test
}  // namespace views
