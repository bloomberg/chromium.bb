// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/macros.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "ui/base/ime/input_method.h"
#include "ui/base/ime/text_input_client.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/ui_base_paths.h"
#include "ui/base/ui_base_switches.h"
#include "ui/events/event_processor.h"
#include "ui/events/event_utils.h"
#include "ui/events/test/event_generator.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gl/test/gl_surface_test_support.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/controls/textfield/textfield_test_api.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/test/focus_manager_test.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/touchui/touch_selection_controller_impl.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/dialog_delegate.h"
#include "ui/wm/public/activation_client.h"

#if defined(OS_WIN)
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/views/widget/desktop_aura/desktop_native_widget_aura.h"
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
  ~ExitLoopOnRelease() override {}

 private:
  // Overridden from View:
  void OnMouseReleased(const ui::MouseEvent& event) override {
    GetWidget()->Close();
    base::MessageLoop::current()->QuitNow();
  }

  DISALLOW_COPY_AND_ASSIGN(ExitLoopOnRelease);
};

// A view that does a capture on ui::ET_GESTURE_TAP_DOWN events.
class GestureCaptureView : public View {
 public:
  GestureCaptureView() {}
  ~GestureCaptureView() override {}

 private:
  // Overridden from View:
  void OnGestureEvent(ui::GestureEvent* event) override {
    if (event->type() == ui::ET_GESTURE_TAP_DOWN) {
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
  ~MouseView() override {}

  bool OnMousePressed(const ui::MouseEvent& event) override {
    pressed_++;
    return true;
  }

  void OnMouseEntered(const ui::MouseEvent& event) override { entered_++; }

  void OnMouseExited(const ui::MouseEvent& event) override { exited_++; }

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
  ~NestedLoopCaptureView() override {}

 private:
  // Overridden from View:
  bool OnMousePressed(const ui::MouseEvent& event) override {
    // Start a nested loop.
    widget_->Show();
    widget_->SetCapture(widget_->GetContentsView());
    EXPECT_TRUE(widget_->HasCapture());

    base::MessageLoopForUI* loop = base::MessageLoopForUI::current();
    base::MessageLoop::ScopedNestableTaskAllower allow(loop);

    base::RunLoop run_loop;
    run_loop.Run();
    return true;
  }

  Widget* widget_;

  DISALLOW_COPY_AND_ASSIGN(NestedLoopCaptureView);
};

// Spins a run loop until a Widget's active state matches a desired state.
class WidgetActivationWaiter : public WidgetObserver {
 public:
  WidgetActivationWaiter(Widget* widget, bool active) : observed_(false) {
#if defined(OS_WIN)
    // On Windows, a HWND can receive a WM_ACTIVATE message without the value
    // of ::GetActiveWindow() updating to reflect that change. This can cause
    // the active window reported by IsActive() to get out of sync. Usually this
    // happens after a call to HWNDMessageHandler::Deactivate() which works by
    // activating some other window, which might be in another application.
    // Doing this can trigger the native OS activation-blocker, causing the
    // taskbar icon to flash instead. But since activation of native widgets on
    // Windows is synchronous, we never have to wait anyway, so it's safe to
    // return here.
    if (active == widget->IsActive()) {
      observed_ = true;
      return;
    }
#endif
    // Always expect a change for tests using this.
    EXPECT_NE(active, widget->IsActive());
    widget->AddObserver(this);
  }

  void Wait() {
    if (!observed_)
      run_loop_.Run();
  }

  void OnWidgetActivationChanged(Widget* widget, bool active) override {
    observed_ = true;
    widget->RemoveObserver(this);
    if (run_loop_.running())
      run_loop_.Quit();
  }

 private:
  base::RunLoop run_loop_;
  bool observed_;

  DISALLOW_COPY_AND_ASSIGN(WidgetActivationWaiter);
};

ui::WindowShowState GetWidgetShowState(const Widget* widget) {
  // Use IsMaximized/IsMinimized/IsFullScreen instead of GetWindowPlacement
  // because the former is implemented on all platforms but the latter is not.
  return widget->IsFullscreen() ? ui::SHOW_STATE_FULLSCREEN :
      widget->IsMaximized() ? ui::SHOW_STATE_MAXIMIZED :
      widget->IsMinimized() ? ui::SHOW_STATE_MINIMIZED :
      widget->IsActive() ? ui::SHOW_STATE_NORMAL :
                           ui::SHOW_STATE_INACTIVE;
}

// Give the OS an opportunity to process messages for an activation change, when
// there is actually no change expected (e.g. ShowInactive()).
void RunPendingMessagesForActiveStatusChange() {
#if defined(OS_MACOSX)
  // On Mac, a single spin is *usually* enough. It isn't when a widget is shown
  // and made active in two steps, so tests should follow up with a ShowSync()
  // or ActivateSync to ensure a consistent state.
  base::RunLoop().RunUntilIdle();
#endif
  // TODO(tapted): Check for desktop aura widgets.
}

// Activate a widget, and wait for it to become active. On non-desktop Aura
// this is just an activation. For other widgets, it means activating and then
// spinning the run loop until the OS has activated the window.
void ActivateSync(Widget* widget) {
  WidgetActivationWaiter waiter(widget, true);
  widget->Activate();
  waiter.Wait();
}

// Like for ActivateSync(), wait for a widget to become active, but Show() the
// widget rather than calling Activate().
void ShowSync(Widget* widget) {
  WidgetActivationWaiter waiter(widget, true);
  widget->Show();
  waiter.Wait();
}

void DeactivateSync(Widget* widget) {
#if defined(OS_MACOSX)
  // Deactivation of a window isn't a concept on Mac: If an application is
  // active and it has any activatable windows, then one of them is always
  // active. But we can simulate deactivation (e.g. as if another application
  // became active) by temporarily making |widget| non-activatable, then
  // activating (and closing) a temporary widget.
  widget->widget_delegate()->set_can_activate(false);
  Widget* stealer = new Widget;
  stealer->Init(Widget::InitParams(Widget::InitParams::TYPE_WINDOW));
  ShowSync(stealer);
  stealer->CloseNow();
  widget->widget_delegate()->set_can_activate(true);
#else
  WidgetActivationWaiter waiter(widget, false);
  widget->Deactivate();
  waiter.Wait();
#endif
}

#if defined(OS_WIN)
void ActivatePlatformWindow(Widget* widget) {
  ::SetActiveWindow(
      widget->GetNativeWindow()->GetHost()->GetAcceleratedWidget());
}
#endif

// Calls ShowInactive() on a Widget, and spins a run loop. The goal is to give
// the OS a chance to activate a widget. However, for this case, the test
// doesn't expect that to happen, so there is nothing to wait for.
void ShowInactiveSync(Widget* widget) {
  widget->ShowInactive();
  RunPendingMessagesForActiveStatusChange();
}

}  // namespace

class WidgetTestInteractive : public WidgetTest {
 public:
  WidgetTestInteractive() {}
  ~WidgetTestInteractive() override {}

  void SetUp() override {
    gfx::GLSurfaceTestSupport::InitializeOneOff();
    ui::RegisterPathProvider();
    base::FilePath ui_test_pak_path;
    ASSERT_TRUE(PathService::Get(ui::UI_TEST_PAK, &ui_test_pak_path));
    ui::ResourceBundle::InitSharedInstanceWithPakPath(ui_test_pak_path);
    WidgetTest::SetUp();
  }

 protected:
#if defined(USE_AURA)
  static void ShowQuickMenuImmediately(
      TouchSelectionControllerImpl* controller) {
    DCHECK(controller);
    if (controller->quick_menu_timer_.IsRunning()) {
      controller->quick_menu_timer_.Stop();
      controller->QuickMenuTimerFired();
    }
  }
#endif  // defined (USE_AURA)

  Widget* CreateWidget() {
    Widget* widget = CreateNativeDesktopWidget();
    widget->SetBounds(gfx::Rect(0, 0, 200, 200));
    return widget;
  }
};

#if defined(OS_WIN)
// Tests whether activation and focus change works correctly in Windows.
// We test the following:-
// 1. If the active aura window is correctly set when a top level widget is
//    created.
// 2. If the active aura window in widget 1 created above, is set to NULL when
//    another top level widget is created and focused.
// 3. On focusing the native platform window for widget 1, the active aura
//    window for widget 1 should be set and that for widget 2 should reset.
// TODO(ananta): Discuss with erg on how to write this test for linux x11 aura.
TEST_F(WidgetTestInteractive, DesktopNativeWidgetAuraActivationAndFocusTest) {
  // Create widget 1 and expect the active window to be its window.
  View* focusable_view1 = new View;
  focusable_view1->SetFocusable(true);
  Widget* widget1 = CreateWidget();
  widget1->GetContentsView()->AddChildView(focusable_view1);
  widget1->Show();
  aura::Window* root_window1 = widget1->GetNativeView()->GetRootWindow();
  focusable_view1->RequestFocus();

  EXPECT_TRUE(root_window1 != NULL);
  aura::client::ActivationClient* activation_client1 =
      aura::client::GetActivationClient(root_window1);
  EXPECT_TRUE(activation_client1 != NULL);
  EXPECT_EQ(activation_client1->GetActiveWindow(), widget1->GetNativeView());

  // Create widget 2 and expect the active window to be its window.
  View* focusable_view2 = new View;
  Widget* widget2 = CreateWidget();
  widget1->GetContentsView()->AddChildView(focusable_view2);
  widget2->Show();
  aura::Window* root_window2 = widget2->GetNativeView()->GetRootWindow();
  focusable_view2->RequestFocus();
  ActivatePlatformWindow(widget2);

  aura::client::ActivationClient* activation_client2 =
      aura::client::GetActivationClient(root_window2);
  EXPECT_TRUE(activation_client2 != NULL);
  EXPECT_EQ(activation_client2->GetActiveWindow(), widget2->GetNativeView());
  EXPECT_EQ(activation_client1->GetActiveWindow(),
            reinterpret_cast<aura::Window*>(NULL));

  // Now set focus back to widget 1 and expect the active window to be its
  // window.
  focusable_view1->RequestFocus();
  ActivatePlatformWindow(widget1);
  EXPECT_EQ(activation_client2->GetActiveWindow(),
            reinterpret_cast<aura::Window*>(NULL));
  EXPECT_EQ(activation_client1->GetActiveWindow(), widget1->GetNativeView());

  widget2->CloseNow();
  widget1->CloseNow();
}
#endif  // defined(OS_WIN)

TEST_F(WidgetTestInteractive, CaptureAutoReset) {
  Widget* toplevel = CreateTopLevelFramelessPlatformWidget();
  View* container = new View;
  toplevel->SetContentsView(container);

  EXPECT_FALSE(toplevel->HasCapture());
  toplevel->SetCapture(NULL);
  EXPECT_TRUE(toplevel->HasCapture());

  // By default, mouse release removes capture.
  gfx::Point click_location(45, 15);
  ui::MouseEvent release(ui::ET_MOUSE_RELEASED, click_location, click_location,
                         ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON,
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

TEST_F(WidgetTestInteractive, ResetCaptureOnGestureEnd) {
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
  ui::GestureEvent tap_down(15,
                            15,
                            0,
                            base::TimeDelta(),
                            ui::GestureEventDetails(ui::ET_GESTURE_TAP_DOWN));
  ui::GestureEvent end(15,
                       15,
                       0,
                       base::TimeDelta(),
                       ui::GestureEventDetails(ui::ET_GESTURE_END));
  toplevel->OnGestureEvent(&tap_down);

  // Now try to click on |mouse|. Since |gesture| will have capture, |mouse|
  // will not receive the event.
  gfx::Point click_location(45, 15);

  ui::MouseEvent press(ui::ET_MOUSE_PRESSED, click_location, click_location,
                       ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON,
                       ui::EF_LEFT_MOUSE_BUTTON);
  ui::MouseEvent release(ui::ET_MOUSE_RELEASED, click_location, click_location,
                         ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON,
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
TEST_F(WidgetTestInteractive, DisableCaptureWidgetFromMousePress) {
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
  base::MessageLoop::current()->PostTask(
      FROM_HERE, base::Bind(&Widget::OnMouseEvent, base::Unretained(second),
                            base::Owned(new ui::MouseEvent(
                                ui::ET_MOUSE_RELEASED, location, location,
                                ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON,
                                ui::EF_LEFT_MOUSE_BUTTON))));
  ui::MouseEvent press(ui::ET_MOUSE_PRESSED, location, location,
                       ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON,
                       ui::EF_LEFT_MOUSE_BUTTON);
  first->OnMouseEvent(&press);
  EXPECT_FALSE(first->HasCapture());
  first->Close();
  RunPendingMessages();
}

// Tests some grab/ungrab events.
// TODO(estade): can this be enabled now that this is an interactive ui test?
TEST_F(WidgetTestInteractive, DISABLED_GrabUngrab) {
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
  ui::MouseEvent pressed(ui::ET_MOUSE_PRESSED, p1, p1, ui::EventTimeForNow(),
                         ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON);
  toplevel->OnMouseEvent(&pressed);

  EXPECT_TRUE(toplevel->HasCapture());
  EXPECT_TRUE(child1->HasCapture());
  EXPECT_FALSE(child2->HasCapture());

  ui::MouseEvent released(ui::ET_MOUSE_RELEASED, p1, p1, ui::EventTimeForNow(),
                          ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON);
  toplevel->OnMouseEvent(&released);

  EXPECT_FALSE(toplevel->HasCapture());
  EXPECT_FALSE(child1->HasCapture());
  EXPECT_FALSE(child2->HasCapture());

  RunPendingMessages();

  // Click on child2
  gfx::Point p2(315, 45);
  ui::MouseEvent pressed2(ui::ET_MOUSE_PRESSED, p2, p2, ui::EventTimeForNow(),
                          ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON);
  toplevel->OnMouseEvent(&pressed2);
  EXPECT_TRUE(pressed2.handled());
  EXPECT_TRUE(toplevel->HasCapture());
  EXPECT_TRUE(child2->HasCapture());
  EXPECT_FALSE(child1->HasCapture());

  ui::MouseEvent released2(ui::ET_MOUSE_RELEASED, p2, p2, ui::EventTimeForNow(),
                           ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON);
  toplevel->OnMouseEvent(&released2);
  EXPECT_FALSE(toplevel->HasCapture());
  EXPECT_FALSE(child1->HasCapture());
  EXPECT_FALSE(child2->HasCapture());

  toplevel->CloseNow();
}

// Tests mouse move outside of the window into the "resize controller" and back
// will still generate an OnMouseEntered and OnMouseExited event..
TEST_F(WidgetTestInteractive, CheckResizeControllerEvents) {
  Widget* toplevel = CreateTopLevelPlatformWidget();

  toplevel->SetBounds(gfx::Rect(0, 0, 100, 100));

  MouseView* view = new MouseView();
  view->SetBounds(90, 90, 10, 10);
  toplevel->GetRootView()->AddChildView(view);

  toplevel->Show();
  RunPendingMessages();

  // Move to an outside position.
  gfx::Point p1(200, 200);
  ui::MouseEvent moved_out(ui::ET_MOUSE_MOVED, p1, p1, ui::EventTimeForNow(),
                           ui::EF_NONE, ui::EF_NONE);
  toplevel->OnMouseEvent(&moved_out);
  EXPECT_EQ(0, view->EnteredCalls());
  EXPECT_EQ(0, view->ExitedCalls());

  // Move onto the active view.
  gfx::Point p2(95, 95);
  ui::MouseEvent moved_over(ui::ET_MOUSE_MOVED, p2, p2, ui::EventTimeForNow(),
                            ui::EF_NONE, ui::EF_NONE);
  toplevel->OnMouseEvent(&moved_over);
  EXPECT_EQ(1, view->EnteredCalls());
  EXPECT_EQ(0, view->ExitedCalls());

  // Move onto the outer resizing border.
  gfx::Point p3(102, 95);
  ui::MouseEvent moved_resizer(ui::ET_MOUSE_MOVED, p3, p3,
                               ui::EventTimeForNow(), ui::EF_NONE, ui::EF_NONE);
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

// Test view focus restoration when a widget is deactivated and re-activated.
TEST_F(WidgetTestInteractive, ViewFocusOnWidgetActivationChanges) {
  Widget* widget1 = CreateTopLevelPlatformWidget();
  View* view1 = new View;
  view1->SetFocusable(true);
  widget1->GetContentsView()->AddChildView(view1);

  Widget* widget2 = CreateTopLevelPlatformWidget();
  View* view2a = new View;
  View* view2b = new View;
  view2a->SetFocusable(true);
  view2b->SetFocusable(true);
  widget2->GetContentsView()->AddChildView(view2a);
  widget2->GetContentsView()->AddChildView(view2b);

  ShowSync(widget1);
  EXPECT_TRUE(widget1->IsActive());
  view1->RequestFocus();
  EXPECT_EQ(view1, widget1->GetFocusManager()->GetFocusedView());

  ShowSync(widget2);
  EXPECT_TRUE(widget2->IsActive());
  EXPECT_FALSE(widget1->IsActive());
  EXPECT_EQ(NULL, widget1->GetFocusManager()->GetFocusedView());
  view2a->RequestFocus();
  EXPECT_EQ(view2a, widget2->GetFocusManager()->GetFocusedView());
  view2b->RequestFocus();
  EXPECT_EQ(view2b, widget2->GetFocusManager()->GetFocusedView());

  ActivateSync(widget1);
  EXPECT_TRUE(widget1->IsActive());
  EXPECT_EQ(view1, widget1->GetFocusManager()->GetFocusedView());
  EXPECT_FALSE(widget2->IsActive());
  EXPECT_EQ(NULL, widget2->GetFocusManager()->GetFocusedView());

  ActivateSync(widget2);
  EXPECT_TRUE(widget2->IsActive());
  EXPECT_EQ(view2b, widget2->GetFocusManager()->GetFocusedView());
  EXPECT_FALSE(widget1->IsActive());
  EXPECT_EQ(NULL, widget1->GetFocusManager()->GetFocusedView());

  widget1->CloseNow();
  widget2->CloseNow();
}

#if defined(OS_WIN)

// Test view focus retention when a widget's HWND is disabled and re-enabled.
TEST_F(WidgetTestInteractive, ViewFocusOnHWNDEnabledChanges) {
  Widget* widget = CreateTopLevelFramelessPlatformWidget();
  widget->SetContentsView(new View);
  for (size_t i = 0; i < 2; ++i) {
    widget->GetContentsView()->AddChildView(new View);
    widget->GetContentsView()->child_at(i)->SetFocusable(true);
  }

  widget->Show();
  const HWND hwnd = HWNDForWidget(widget);
  EXPECT_TRUE(::IsWindow(hwnd));
  EXPECT_TRUE(::IsWindowEnabled(hwnd));
  EXPECT_EQ(hwnd, ::GetActiveWindow());

  for (int i = 0; i < widget->GetContentsView()->child_count(); ++i) {
    SCOPED_TRACE(base::StringPrintf("Child view %d", i));
    View* view = widget->GetContentsView()->child_at(i);

    view->RequestFocus();
    EXPECT_EQ(view, widget->GetFocusManager()->GetFocusedView());
    EXPECT_FALSE(::EnableWindow(hwnd, FALSE));
    EXPECT_FALSE(::IsWindowEnabled(hwnd));

    // Oddly, disabling the HWND leaves it active with the focus unchanged.
    EXPECT_EQ(hwnd, ::GetActiveWindow());
    EXPECT_TRUE(widget->IsActive());
    EXPECT_EQ(view, widget->GetFocusManager()->GetFocusedView());

    EXPECT_TRUE(::EnableWindow(hwnd, TRUE));
    EXPECT_TRUE(::IsWindowEnabled(hwnd));
    EXPECT_EQ(hwnd, ::GetActiveWindow());
    EXPECT_TRUE(widget->IsActive());
    EXPECT_EQ(view, widget->GetFocusManager()->GetFocusedView());
  }

  widget->CloseNow();
}

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

  ~WidgetActivationTest() override {}

  void OnNativeWidgetActivationChanged(bool active) override {
    active_ = active;
  }

  bool active() const { return active_; }

 private:
  bool active_;

  DISALLOW_COPY_AND_ASSIGN(WidgetActivationTest);
};

// Tests whether the widget only becomes active when the underlying window
// is really active.
TEST_F(WidgetTestInteractive, WidgetNotActivatedOnFakeActivationMessages) {
  WidgetActivationTest widget1;
  Widget::InitParams init_params =
      CreateParams(Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  init_params.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  init_params.native_widget = new DesktopNativeWidgetAura(&widget1);
  init_params.bounds = gfx::Rect(0, 0, 200, 200);
  widget1.Init(init_params);
  widget1.Show();
  EXPECT_EQ(true, widget1.active());

  WidgetActivationTest widget2;
  init_params.native_widget = new DesktopNativeWidgetAura(&widget2);
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
#endif  // defined(OS_WIN)

#if !defined(OS_CHROMEOS)
// Provides functionality to create a window modal dialog.
class ModalDialogDelegate : public DialogDelegateView {
 public:
  explicit ModalDialogDelegate(ui::ModalType type) : type_(type) {}
  ~ModalDialogDelegate() override {}

  // WidgetDelegate overrides.
  ui::ModalType GetModalType() const override { return type_; }

 private:
  ui::ModalType type_;

  DISALLOW_COPY_AND_ASSIGN(ModalDialogDelegate);
};

// Tests whether the focused window is set correctly when a modal window is
// created and destroyed. When it is destroyed it should focus the owner window.
TEST_F(WidgetTestInteractive, WindowModalWindowDestroyedActivationTest) {
  TestWidgetFocusChangeListener focus_listener;
  WidgetFocusManager::GetInstance()->AddFocusChangeListener(&focus_listener);
  const std::vector<gfx::NativeView>& focus_changes =
      focus_listener.focus_changes();

  // Create a top level widget.
  Widget top_level_widget;
  Widget::InitParams init_params =
      CreateParams(Widget::InitParams::TYPE_WINDOW);
  init_params.show_state = ui::SHOW_STATE_NORMAL;
  gfx::Rect initial_bounds(0, 0, 500, 500);
  init_params.bounds = initial_bounds;
  init_params.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  init_params.native_widget =
      new PlatformDesktopNativeWidget(&top_level_widget);
  top_level_widget.Init(init_params);
  ShowSync(&top_level_widget);

  gfx::NativeView top_level_native_view = top_level_widget.GetNativeView();
  ASSERT_FALSE(focus_listener.focus_changes().empty());
  EXPECT_EQ(1u, focus_changes.size());
  EXPECT_EQ(top_level_native_view, focus_changes[0]);

  // Create a modal dialog.
  // This instance will be destroyed when the dialog is destroyed.
  ModalDialogDelegate* dialog_delegate =
      new ModalDialogDelegate(ui::MODAL_TYPE_WINDOW);

  Widget* modal_dialog_widget = views::DialogDelegate::CreateDialogWidget(
      dialog_delegate, NULL, top_level_widget.GetNativeView());
  modal_dialog_widget->SetBounds(gfx::Rect(100, 100, 200, 200));

  // Note the dialog widget doesn't need a ShowSync. Since it is modal, it gains
  // active status synchronously, even on Mac.
  modal_dialog_widget->Show();

  gfx::NativeView modal_native_view = modal_dialog_widget->GetNativeView();
  EXPECT_EQ(3u, focus_changes.size());
  EXPECT_EQ(nullptr, focus_changes[1]);
  EXPECT_EQ(modal_native_view, focus_changes[2]);

#if defined(OS_MACOSX)
  // Window modal dialogs on Mac are "sheets", which animate to close before
  // activating their parent widget.
  WidgetActivationWaiter waiter(&top_level_widget, true);
  modal_dialog_widget->Close();
  waiter.Wait();
#else
  modal_dialog_widget->CloseNow();
#endif

  EXPECT_EQ(5u, focus_changes.size());
  EXPECT_EQ(nullptr, focus_changes[3]);
  EXPECT_EQ(top_level_native_view, focus_changes[4]);

  top_level_widget.CloseNow();
  WidgetFocusManager::GetInstance()->RemoveFocusChangeListener(&focus_listener);
}

// Disabled on Mac. Desktop Mac doesn't have system modal windows since Carbon
// was deprecated. It does have application modal windows, but only Ash requests
// those.
#if defined(OS_MACOSX) && !defined(USE_AURA)
#define MAYBE_SystemModalWindowReleasesCapture \
    DISABLED_SystemModalWindowReleasesCapture
#else
#define MAYBE_SystemModalWindowReleasesCapture SystemModalWindowReleasesCapture
#endif

// Test that when opening a system-modal window, capture is released.
TEST_F(WidgetTestInteractive, MAYBE_SystemModalWindowReleasesCapture) {
  TestWidgetFocusChangeListener focus_listener;
  WidgetFocusManager::GetInstance()->AddFocusChangeListener(&focus_listener);

  // Create a top level widget.
  Widget top_level_widget;
  Widget::InitParams init_params =
      CreateParams(Widget::InitParams::TYPE_WINDOW);
  init_params.show_state = ui::SHOW_STATE_NORMAL;
  gfx::Rect initial_bounds(0, 0, 500, 500);
  init_params.bounds = initial_bounds;
  init_params.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  init_params.native_widget =
      new PlatformDesktopNativeWidget(&top_level_widget);
  top_level_widget.Init(init_params);
  ShowSync(&top_level_widget);

  ASSERT_FALSE(focus_listener.focus_changes().empty());
  EXPECT_EQ(top_level_widget.GetNativeView(),
            focus_listener.focus_changes().back());

  EXPECT_FALSE(top_level_widget.HasCapture());
  top_level_widget.SetCapture(NULL);
  EXPECT_TRUE(top_level_widget.HasCapture());

  // Create a modal dialog.
  ModalDialogDelegate* dialog_delegate =
      new ModalDialogDelegate(ui::MODAL_TYPE_SYSTEM);

  Widget* modal_dialog_widget = views::DialogDelegate::CreateDialogWidget(
      dialog_delegate, NULL, top_level_widget.GetNativeView());
  modal_dialog_widget->SetBounds(gfx::Rect(100, 100, 200, 200));
  ShowSync(modal_dialog_widget);

  EXPECT_FALSE(top_level_widget.HasCapture());

  modal_dialog_widget->CloseNow();
  top_level_widget.CloseNow();
  WidgetFocusManager::GetInstance()->RemoveFocusChangeListener(&focus_listener);
}

#endif  // !defined(OS_CHROMEOS)

TEST_F(WidgetTestInteractive, CanActivateFlagIsHonored) {
  Widget widget;
  Widget::InitParams init_params =
      CreateParams(Widget::InitParams::TYPE_WINDOW);
  init_params.bounds = gfx::Rect(0, 0, 200, 200);
  init_params.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  init_params.activatable = Widget::InitParams::ACTIVATABLE_NO;
#if !defined(OS_CHROMEOS)
  init_params.native_widget = new PlatformDesktopNativeWidget(&widget);
#endif  // !defined(OS_CHROMEOS)
  widget.Init(init_params);

  widget.Show();
  EXPECT_FALSE(widget.IsActive());
}

#if defined(USE_AURA)
// Test that touch selection quick menu is not activated when opened.
TEST_F(WidgetTestInteractive, TouchSelectionQuickMenuIsNotActivated) {
#if defined(OS_WIN)
  views_delegate()->set_use_desktop_native_widgets(true);
#endif  // !defined(OS_WIN)

  Widget* widget = CreateWidget();

  Textfield* textfield = new Textfield;
  textfield->SetBounds(0, 0, 200, 20);
  textfield->SetText(base::ASCIIToUTF16("some text"));
  widget->GetRootView()->AddChildView(textfield);

  widget->Show();
  textfield->RequestFocus();
  textfield->SelectAll(true);
  TextfieldTestApi textfield_test_api(textfield);

  RunPendingMessages();

  ui::test::EventGenerator generator(widget->GetNativeWindow());
  generator.GestureTapAt(gfx::Point(10, 10));
  ShowQuickMenuImmediately(static_cast<TouchSelectionControllerImpl*>(
      textfield_test_api.touch_selection_controller()));

  EXPECT_TRUE(textfield->HasFocus());
  EXPECT_TRUE(widget->IsActive());
  EXPECT_TRUE(ui::TouchSelectionMenuRunner::GetInstance()->IsRunning());
  widget->CloseNow();
}
#endif  // defined(USE_AURA)

TEST_F(WidgetTestInteractive, DisableViewDoesNotActivateWidget) {
#if defined(OS_WIN)
  views_delegate()->set_use_desktop_native_widgets(true);
#endif  // !defined(OS_WIN)

  // Create first widget and view, activate the widget, and focus the view.
  Widget widget1;
  Widget::InitParams params1 = CreateParams(Widget::InitParams::TYPE_POPUP);
  params1.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params1.activatable = Widget::InitParams::ACTIVATABLE_YES;
  widget1.Init(params1);

  View* view1 = new View();
  view1->SetFocusable(true);
  widget1.GetRootView()->AddChildView(view1);

  ActivateSync(&widget1);

  FocusManager* focus_manager1 = widget1.GetFocusManager();
  ASSERT_TRUE(focus_manager1);
  focus_manager1->SetFocusedView(view1);
  EXPECT_EQ(view1, focus_manager1->GetFocusedView());

  // Create second widget and view, activate the widget, and focus the view.
  Widget widget2;
  Widget::InitParams params2 = CreateParams(Widget::InitParams::TYPE_POPUP);
  params2.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params2.activatable = Widget::InitParams::ACTIVATABLE_YES;
  widget2.Init(params2);

  View* view2 = new View();
  view2->SetFocusable(true);
  widget2.GetRootView()->AddChildView(view2);

  ActivateSync(&widget2);
  EXPECT_TRUE(widget2.IsActive());
  EXPECT_FALSE(widget1.IsActive());

  FocusManager* focus_manager2 = widget2.GetFocusManager();
  ASSERT_TRUE(focus_manager2);
  focus_manager2->SetFocusedView(view2);
  EXPECT_EQ(view2, focus_manager2->GetFocusedView());

  // Disable the first view and make sure it loses focus, but its widget is not
  // activated.
  view1->SetEnabled(false);
  EXPECT_NE(view1, focus_manager1->GetFocusedView());
  EXPECT_FALSE(widget1.IsActive());
  EXPECT_TRUE(widget2.IsActive());
}

TEST_F(WidgetTestInteractive, ShowCreatesActiveWindow) {
  Widget* widget = CreateTopLevelPlatformWidget();

  ShowSync(widget);
  EXPECT_EQ(GetWidgetShowState(widget), ui::SHOW_STATE_NORMAL);

  widget->CloseNow();
}

TEST_F(WidgetTestInteractive, ShowInactive) {
  Widget* widget = CreateTopLevelPlatformWidget();

  ShowInactiveSync(widget);
  EXPECT_EQ(GetWidgetShowState(widget), ui::SHOW_STATE_INACTIVE);

  widget->CloseNow();
}

TEST_F(WidgetTestInteractive, InactiveBeforeShow) {
  Widget* widget = CreateTopLevelPlatformWidget();

  EXPECT_FALSE(widget->IsActive());
  EXPECT_FALSE(widget->IsVisible());

  ShowSync(widget);

  EXPECT_TRUE(widget->IsActive());
  EXPECT_TRUE(widget->IsVisible());

  widget->CloseNow();
}

TEST_F(WidgetTestInteractive, ShowInactiveAfterShow) {
  // Create 2 widgets to ensure window layering does not change.
  Widget* widget = CreateTopLevelPlatformWidget();
  Widget* widget2 = CreateTopLevelPlatformWidget();

  ShowSync(widget2);
  EXPECT_FALSE(widget->IsActive());
  EXPECT_TRUE(widget2->IsVisible());
  EXPECT_TRUE(widget2->IsActive());

  ShowSync(widget);
  EXPECT_TRUE(widget->IsActive());
  EXPECT_FALSE(widget2->IsActive());

  ShowInactiveSync(widget);
  EXPECT_TRUE(widget->IsActive());
  EXPECT_FALSE(widget2->IsActive());
  EXPECT_EQ(GetWidgetShowState(widget), ui::SHOW_STATE_NORMAL);

  widget2->CloseNow();
  widget->CloseNow();
}

TEST_F(WidgetTestInteractive, ShowAfterShowInactive) {
  Widget* widget = CreateTopLevelPlatformWidget();
  widget->SetBounds(gfx::Rect(100, 100, 100, 100));

  ShowInactiveSync(widget);
  ShowSync(widget);
  EXPECT_EQ(GetWidgetShowState(widget), ui::SHOW_STATE_NORMAL);

  widget->CloseNow();
}

#if !defined(OS_CHROMEOS)
TEST_F(WidgetTestInteractive, InactiveWidgetDoesNotGrabActivation) {
  Widget* widget = CreateTopLevelPlatformWidget();
  ShowSync(widget);
  EXPECT_EQ(GetWidgetShowState(widget), ui::SHOW_STATE_NORMAL);

  Widget widget2;
  Widget::InitParams params = CreateParams(Widget::InitParams::TYPE_POPUP);
  params.native_widget = new PlatformDesktopNativeWidget(&widget2);
  params.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  widget2.Init(params);
  widget2.Show();
  RunPendingMessagesForActiveStatusChange();

  EXPECT_EQ(GetWidgetShowState(&widget2), ui::SHOW_STATE_INACTIVE);
  EXPECT_EQ(GetWidgetShowState(widget), ui::SHOW_STATE_NORMAL);

  widget->CloseNow();
  widget2.CloseNow();
}
#endif  // !defined(OS_CHROMEOS)

// ExitFullscreenRestoreState doesn't use DesktopAura widgets. On Mac, there are
// currently only Desktop widgets and fullscreen changes have to coordinate with
// the OS. See BridgedNativeWidgetUITest for native Mac fullscreen tests.
// Maximize on mac is also (intentionally) a no-op.
#if defined(OS_MACOSX) && !defined(USE_AURA)
#define MAYBE_ExitFullscreenRestoreState DISABLED_ExitFullscreenRestoreState
#else
#define MAYBE_ExitFullscreenRestoreState ExitFullscreenRestoreState
#endif

// Test that window state is not changed after getting out of full screen.
TEST_F(WidgetTestInteractive, MAYBE_ExitFullscreenRestoreState) {
  Widget* toplevel = CreateTopLevelPlatformWidget();

  toplevel->Show();
  RunPendingMessages();

  // This should be a normal state window.
  EXPECT_EQ(ui::SHOW_STATE_NORMAL, GetWidgetShowState(toplevel));

  toplevel->SetFullscreen(true);
  EXPECT_EQ(ui::SHOW_STATE_FULLSCREEN, GetWidgetShowState(toplevel));
  toplevel->SetFullscreen(false);
  EXPECT_NE(ui::SHOW_STATE_FULLSCREEN, GetWidgetShowState(toplevel));

  // And it should still be in normal state after getting out of full screen.
  EXPECT_EQ(ui::SHOW_STATE_NORMAL, GetWidgetShowState(toplevel));

  // Now, make it maximized.
  toplevel->Maximize();
  EXPECT_EQ(ui::SHOW_STATE_MAXIMIZED, GetWidgetShowState(toplevel));

  toplevel->SetFullscreen(true);
  EXPECT_EQ(ui::SHOW_STATE_FULLSCREEN, GetWidgetShowState(toplevel));
  toplevel->SetFullscreen(false);
  EXPECT_NE(ui::SHOW_STATE_FULLSCREEN, GetWidgetShowState(toplevel));

  // And it stays maximized after getting out of full screen.
  EXPECT_EQ(ui::SHOW_STATE_MAXIMIZED, GetWidgetShowState(toplevel));

  // Clean up.
  toplevel->Close();
  RunPendingMessages();
}

namespace {

// Used to veirfy OnMouseCaptureLost() has been invoked.
class CaptureLostTrackingWidget : public Widget {
 public:
  CaptureLostTrackingWidget() : got_capture_lost_(false) {}
  ~CaptureLostTrackingWidget() override {}

  bool GetAndClearGotCaptureLost() {
    bool value = got_capture_lost_;
    got_capture_lost_ = false;
    return value;
  }

  // Widget:
  void OnMouseCaptureLost() override {
    got_capture_lost_ = true;
    Widget::OnMouseCaptureLost();
  }

 private:
  bool got_capture_lost_;

  DISALLOW_COPY_AND_ASSIGN(CaptureLostTrackingWidget);
};

}  // namespace

class WidgetCaptureTest : public ViewsTestBase {
 public:
  WidgetCaptureTest() {
  }

  ~WidgetCaptureTest() override {}

  void SetUp() override {
    gfx::GLSurfaceTestSupport::InitializeOneOff();
    ui::RegisterPathProvider();
    base::FilePath ui_test_pak_path;
    ASSERT_TRUE(PathService::Get(ui::UI_TEST_PAK, &ui_test_pak_path));
    ui::ResourceBundle::InitSharedInstanceWithPakPath(ui_test_pak_path);
    ViewsTestBase::SetUp();
  }

  // Verifies Widget::SetCapture() results in updating native capture along with
  // invoking the right Widget function.
  void TestCapture(bool use_desktop_native_widget) {
    CaptureLostTrackingWidget widget1;
    Widget::InitParams params1 =
        CreateParams(views::Widget::InitParams::TYPE_WINDOW);
    params1.native_widget = CreateNativeWidget(use_desktop_native_widget,
                                               &widget1);
    params1.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
    widget1.Init(params1);
    widget1.Show();

    CaptureLostTrackingWidget widget2;
    Widget::InitParams params2 =
        CreateParams(views::Widget::InitParams::TYPE_WINDOW);
    params2.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
    params2.native_widget = CreateNativeWidget(use_desktop_native_widget,
                                               &widget2);
    widget2.Init(params2);
    widget2.Show();

    // Set capture to widget2 and verity it gets it.
    widget2.SetCapture(widget2.GetRootView());
    EXPECT_FALSE(widget1.HasCapture());
    EXPECT_TRUE(widget2.HasCapture());
    EXPECT_FALSE(widget1.GetAndClearGotCaptureLost());
    EXPECT_FALSE(widget2.GetAndClearGotCaptureLost());

    // Set capture to widget1 and verify it gets it.
    widget1.SetCapture(widget1.GetRootView());
    EXPECT_TRUE(widget1.HasCapture());
    EXPECT_FALSE(widget2.HasCapture());
    EXPECT_FALSE(widget1.GetAndClearGotCaptureLost());
    EXPECT_TRUE(widget2.GetAndClearGotCaptureLost());

    // Release and verify no one has it.
    widget1.ReleaseCapture();
    EXPECT_FALSE(widget1.HasCapture());
    EXPECT_FALSE(widget2.HasCapture());
    EXPECT_TRUE(widget1.GetAndClearGotCaptureLost());
    EXPECT_FALSE(widget2.GetAndClearGotCaptureLost());
  }

  NativeWidget* CreateNativeWidget(bool create_desktop_native_widget,
                                   Widget* widget) {
#if !defined(OS_CHROMEOS)
    if (create_desktop_native_widget)
      return new PlatformDesktopNativeWidget(widget);
#endif
    return NULL;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(WidgetCaptureTest);
};

// See description in TestCapture().
TEST_F(WidgetCaptureTest, Capture) {
  TestCapture(false);
}

#if !defined(OS_CHROMEOS)
// See description in TestCapture(). Creates DesktopNativeWidget.
TEST_F(WidgetCaptureTest, CaptureDesktopNativeWidget) {
  TestCapture(true);
}
#endif

// Test that no state is set if capture fails.
TEST_F(WidgetCaptureTest, FailedCaptureRequestIsNoop) {
  Widget widget;
  Widget::InitParams params =
      CreateParams(Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  params.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.bounds = gfx::Rect(400, 400);
  widget.Init(params);

  MouseView* mouse_view1 = new MouseView;
  MouseView* mouse_view2 = new MouseView;
  View* contents_view = new View;
  contents_view->AddChildView(mouse_view1);
  contents_view->AddChildView(mouse_view2);
  widget.SetContentsView(contents_view);

  mouse_view1->SetBounds(0, 0, 200, 400);
  mouse_view2->SetBounds(200, 0, 200, 400);

  // Setting capture should fail because |widget| is not visible.
  widget.SetCapture(mouse_view1);
  EXPECT_FALSE(widget.HasCapture());

  widget.Show();
  ui::test::EventGenerator generator(GetContext(), widget.GetNativeWindow());
  generator.set_current_location(gfx::Point(300, 10));
  generator.PressLeftButton();

  EXPECT_FALSE(mouse_view1->pressed());
  EXPECT_TRUE(mouse_view2->pressed());
}

// Regression test for http://crbug.com/382421 (Linux-Aura issue).
// TODO(pkotwicz): Make test pass on CrOS and Windows.
// TODO(tapted): Investigate for toolkit-views on Mac http;//crbug.com/441064.
#if defined(OS_CHROMEOS) || defined(OS_WIN) || defined(OS_MACOSX)
#define MAYBE_MouseExitOnCaptureGrab DISABLED_MouseExitOnCaptureGrab
#else
#define MAYBE_MouseExitOnCaptureGrab MouseExitOnCaptureGrab
#endif

// Test that a synthetic mouse exit is sent to the widget which was handling
// mouse events when a different widget grabs capture.
TEST_F(WidgetCaptureTest, MAYBE_MouseExitOnCaptureGrab) {
  Widget widget1;
  Widget::InitParams params1 =
      CreateParams(Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  params1.native_widget = CreateNativeWidget(true, &widget1);
  params1.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  widget1.Init(params1);
  MouseView* mouse_view1 = new MouseView;
  widget1.SetContentsView(mouse_view1);
  widget1.Show();
  widget1.SetBounds(gfx::Rect(300, 300));

  Widget widget2;
  Widget::InitParams params2 =
      CreateParams(Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  params2.native_widget = CreateNativeWidget(true, &widget2);
  params2.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  widget2.Init(params2);
  widget2.Show();
  widget2.SetBounds(gfx::Rect(400, 0, 300, 300));

  ui::test::EventGenerator generator(widget1.GetNativeWindow());
  generator.set_current_location(gfx::Point(100, 100));
  generator.MoveMouseBy(0, 0);

  EXPECT_EQ(1, mouse_view1->EnteredCalls());
  EXPECT_EQ(0, mouse_view1->ExitedCalls());

  widget2.SetCapture(NULL);
  EXPECT_EQ(0, mouse_view1->EnteredCalls());
  // Grabbing native capture on Windows generates a ui::ET_MOUSE_EXITED event
  // in addition to the one generated by Chrome.
  EXPECT_LT(0, mouse_view1->ExitedCalls());
}

namespace {

// Widget observer which grabs capture when the widget is activated.
class CaptureOnActivationObserver : public WidgetObserver {
 public:
  CaptureOnActivationObserver() : activation_observed_(false) {}
  ~CaptureOnActivationObserver() override {}

  // WidgetObserver:
  void OnWidgetActivationChanged(Widget* widget, bool active) override {
    if (active) {
      widget->SetCapture(nullptr);
      activation_observed_ = true;
    }
  }

  bool activation_observed() const { return activation_observed_; }

 private:
  bool activation_observed_;

  DISALLOW_COPY_AND_ASSIGN(CaptureOnActivationObserver);
};

}  // namespace

// Test that setting capture on widget activation of a non-toplevel widget
// (e.g. a bubble on Linux) succeeds.
TEST_F(WidgetCaptureTest, SetCaptureToNonToplevel) {
  Widget toplevel;
  Widget::InitParams toplevel_params =
      CreateParams(Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  toplevel_params.native_widget = CreateNativeWidget(true, &toplevel);
  toplevel_params.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  toplevel.Init(toplevel_params);
  toplevel.Show();

  Widget* child = new Widget;
  Widget::InitParams child_params =
      CreateParams(Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  child_params.parent = toplevel.GetNativeView();
  child_params.context = toplevel.GetNativeWindow();
  child->Init(child_params);

  CaptureOnActivationObserver observer;
  child->AddObserver(&observer);
  child->Show();

#if defined(OS_MACOSX) && !defined(USE_AURA)
  // On Mac, activation is asynchronous. A single trip to the runloop should be
  // sufficient. On Aura platforms, note that since the child widget isn't top-
  // level, the aura window manager gets asked whether the widget is active, not
  // the OS.
  base::RunLoop().RunUntilIdle();
#endif

  EXPECT_TRUE(observer.activation_observed());
  EXPECT_TRUE(child->HasCapture());
}


#if defined(OS_WIN)
namespace {

// Used to verify OnMouseEvent() has been invoked.
class MouseEventTrackingWidget : public Widget {
 public:
  MouseEventTrackingWidget() : got_mouse_event_(false) {}
  ~MouseEventTrackingWidget() override {}

  bool GetAndClearGotMouseEvent() {
    bool value = got_mouse_event_;
    got_mouse_event_ = false;
    return value;
  }

  // Widget:
  void OnMouseEvent(ui::MouseEvent* event) override {
    got_mouse_event_ = true;
    Widget::OnMouseEvent(event);
  }

 private:
  bool got_mouse_event_;

  DISALLOW_COPY_AND_ASSIGN(MouseEventTrackingWidget);
};

}  // namespace

// Verifies if a mouse event is received on a widget that doesn't have capture
// on Windows that it is correctly processed by the widget that doesn't have
// capture. This behavior is not desired on OSes other than Windows.
TEST_F(WidgetCaptureTest, MouseEventDispatchedToRightWindow) {
  MouseEventTrackingWidget widget1;
  Widget::InitParams params1 =
      CreateParams(views::Widget::InitParams::TYPE_WINDOW);
  params1.native_widget = new DesktopNativeWidgetAura(&widget1);
  params1.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  widget1.Init(params1);
  widget1.Show();

  MouseEventTrackingWidget widget2;
  Widget::InitParams params2 =
      CreateParams(views::Widget::InitParams::TYPE_WINDOW);
  params2.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params2.native_widget = new DesktopNativeWidgetAura(&widget2);
  widget2.Init(params2);
  widget2.Show();

  // Set capture to widget2 and verity it gets it.
  widget2.SetCapture(widget2.GetRootView());
  EXPECT_FALSE(widget1.HasCapture());
  EXPECT_TRUE(widget2.HasCapture());

  widget1.GetAndClearGotMouseEvent();
  widget2.GetAndClearGotMouseEvent();
  // Send a mouse event to the RootWindow associated with |widget1|. Even though
  // |widget2| has capture, |widget1| should still get the event.
  ui::MouseEvent mouse_event(ui::ET_MOUSE_EXITED, gfx::Point(), gfx::Point(),
                             ui::EventTimeForNow(), ui::EF_NONE, ui::EF_NONE);
  ui::EventDispatchDetails details = widget1.GetNativeWindow()->
      GetHost()->event_processor()->OnEventFromSource(&mouse_event);
  ASSERT_FALSE(details.dispatcher_destroyed);
  EXPECT_TRUE(widget1.GetAndClearGotMouseEvent());
  EXPECT_FALSE(widget2.GetAndClearGotMouseEvent());
}
#endif  // defined(OS_WIN)

class WidgetInputMethodInteractiveTest : public WidgetTestInteractive {
 public:
  WidgetInputMethodInteractiveTest() {}

  // testing::Test:
  void SetUp() override {
    WidgetTestInteractive::SetUp();
#if defined(OS_WIN)
    // On Windows, Widget::Deactivate() works by activating the next topmost
    // window on the z-order stack. This only works if there is at least one
    // other window, so make sure that is the case.
    deactivate_widget_ = CreateWidget();
    deactivate_widget_->Show();
#endif
  }

  void TearDown() override {
    if (deactivate_widget_)
      deactivate_widget_->CloseNow();
    WidgetTestInteractive::TearDown();
  }

 private:
  Widget* deactivate_widget_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(WidgetInputMethodInteractiveTest);
};

// Test input method focus changes affected by top window activaction.
TEST_F(WidgetInputMethodInteractiveTest, Activation) {
  Widget* widget = CreateWidget();
  Textfield* textfield = new Textfield;
  widget->GetRootView()->AddChildView(textfield);
  textfield->RequestFocus();

  ShowSync(widget);

  EXPECT_EQ(ui::TEXT_INPUT_TYPE_TEXT,
            widget->GetInputMethod()->GetTextInputType());

  DeactivateSync(widget);

  EXPECT_EQ(ui::TEXT_INPUT_TYPE_NONE,
            widget->GetInputMethod()->GetTextInputType());
  widget->CloseNow();
}

// Test input method focus changes affected by focus changes within 1 window.
TEST_F(WidgetInputMethodInteractiveTest, OneWindow) {
  Widget* widget = CreateWidget();
  Textfield* textfield1 = new Textfield;
  Textfield* textfield2 = new Textfield;
  textfield2->SetTextInputType(ui::TEXT_INPUT_TYPE_PASSWORD);
  widget->GetRootView()->AddChildView(textfield1);
  widget->GetRootView()->AddChildView(textfield2);

  ShowSync(widget);

  textfield1->RequestFocus();
  EXPECT_EQ(ui::TEXT_INPUT_TYPE_TEXT,
            widget->GetInputMethod()->GetTextInputType());

  textfield2->RequestFocus();
  EXPECT_EQ(ui::TEXT_INPUT_TYPE_PASSWORD,
            widget->GetInputMethod()->GetTextInputType());

// Widget::Deactivate() doesn't work for CrOS, because it uses NWA instead of
// DNWA (which just activates the last active window) and involves the
// AuraTestHelper which sets the input method as DummyInputMethod.
#if !defined(OS_CHROMEOS)
  DeactivateSync(widget);
  EXPECT_EQ(ui::TEXT_INPUT_TYPE_NONE,
            widget->GetInputMethod()->GetTextInputType());

  ActivateSync(widget);
  EXPECT_EQ(ui::TEXT_INPUT_TYPE_PASSWORD,
            widget->GetInputMethod()->GetTextInputType());

  DeactivateSync(widget);
  textfield1->RequestFocus();
  ActivateSync(widget);
  EXPECT_TRUE(widget->IsActive());
  EXPECT_EQ(ui::TEXT_INPUT_TYPE_TEXT,
            widget->GetInputMethod()->GetTextInputType());
#endif
  widget->CloseNow();
}

// Test input method focus changes affected by focus changes cross 2 windows
// which shares the same top window.
TEST_F(WidgetInputMethodInteractiveTest, TwoWindows) {
  Widget* parent = CreateWidget();
  parent->SetBounds(gfx::Rect(100, 100, 100, 100));

  Widget* child = CreateChildNativeWidgetWithParent(parent);
  child->SetBounds(gfx::Rect(0, 0, 50, 50));
  child->Show();

  Textfield* textfield_parent = new Textfield;
  Textfield* textfield_child = new Textfield;
  textfield_parent->SetTextInputType(ui::TEXT_INPUT_TYPE_PASSWORD);
  parent->GetRootView()->AddChildView(textfield_parent);
  child->GetRootView()->AddChildView(textfield_child);
  ShowSync(parent);

  EXPECT_EQ(parent->GetInputMethod(), child->GetInputMethod());

  textfield_parent->RequestFocus();
  EXPECT_EQ(ui::TEXT_INPUT_TYPE_PASSWORD,
            parent->GetInputMethod()->GetTextInputType());

  textfield_child->RequestFocus();
  EXPECT_EQ(ui::TEXT_INPUT_TYPE_TEXT,
            parent->GetInputMethod()->GetTextInputType());

// Widget::Deactivate() doesn't work for CrOS, because it uses NWA instead of
// DNWA (which just activates the last active window) and involves the
// AuraTestHelper which sets the input method as DummyInputMethod.
#if !defined(OS_CHROMEOS)
  DeactivateSync(parent);
  EXPECT_EQ(ui::TEXT_INPUT_TYPE_NONE,
            parent->GetInputMethod()->GetTextInputType());

  ActivateSync(parent);
  EXPECT_EQ(ui::TEXT_INPUT_TYPE_TEXT,
            parent->GetInputMethod()->GetTextInputType());

  textfield_parent->RequestFocus();
  DeactivateSync(parent);
  EXPECT_EQ(ui::TEXT_INPUT_TYPE_NONE,
            parent->GetInputMethod()->GetTextInputType());

  ActivateSync(parent);
  EXPECT_EQ(ui::TEXT_INPUT_TYPE_PASSWORD,
            parent->GetInputMethod()->GetTextInputType());
#endif

  parent->CloseNow();
}

// Test input method focus changes affected by textfield's state changes.
TEST_F(WidgetInputMethodInteractiveTest, TextField) {
  Widget* widget = CreateWidget();
  Textfield* textfield = new Textfield;
  widget->GetRootView()->AddChildView(textfield);
  ShowSync(widget);
  EXPECT_EQ(ui::TEXT_INPUT_TYPE_NONE,
            widget->GetInputMethod()->GetTextInputType());

  textfield->SetTextInputType(ui::TEXT_INPUT_TYPE_PASSWORD);
  EXPECT_EQ(ui::TEXT_INPUT_TYPE_NONE,
            widget->GetInputMethod()->GetTextInputType());

  textfield->RequestFocus();
  EXPECT_EQ(ui::TEXT_INPUT_TYPE_PASSWORD,
            widget->GetInputMethod()->GetTextInputType());

  textfield->SetTextInputType(ui::TEXT_INPUT_TYPE_TEXT);
  EXPECT_EQ(ui::TEXT_INPUT_TYPE_TEXT,
            widget->GetInputMethod()->GetTextInputType());

  textfield->SetReadOnly(true);
  EXPECT_EQ(ui::TEXT_INPUT_TYPE_NONE,
            widget->GetInputMethod()->GetTextInputType());
  widget->CloseNow();
}

}  // namespace test
}  // namespace views
