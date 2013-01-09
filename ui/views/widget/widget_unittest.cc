// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/point.h"
#include "ui/views/bubble/bubble_delegate.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/test/test_views_delegate.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/views_delegate.h"
#include "ui/views/widget/native_widget_delegate.h"

#if defined(USE_AURA)
#include "ui/aura/window.h"
#include "ui/views/widget/native_widget_aura.h"
#if !defined(OS_CHROMEOS)
#include "ui/views/widget/desktop_aura/desktop_native_widget_aura.h"
#endif
#elif defined(OS_WIN)
#include "ui/views/widget/native_widget_win.h"
#endif

namespace views {
namespace {

// A generic typedef to pick up relevant NativeWidget implementations.
#if defined(USE_AURA)
typedef NativeWidgetAura NativeWidgetPlatform;
#elif defined(OS_WIN)
typedef NativeWidgetWin NativeWidgetPlatform;
#endif

// A widget that assumes mouse capture always works. It won't on Aura in
// testing, so we mock it.
#if defined(USE_AURA)
class NativeWidgetCapture : public NativeWidgetPlatform {
 public:
  explicit NativeWidgetCapture(internal::NativeWidgetDelegate* delegate)
      : NativeWidgetPlatform(delegate),
        mouse_capture_(false) {}
  virtual ~NativeWidgetCapture() {}

  virtual void SetCapture() OVERRIDE {
    mouse_capture_ = true;
  }
  virtual void ReleaseCapture() OVERRIDE {
    if (mouse_capture_)
      delegate()->OnMouseCaptureLost();
    mouse_capture_ = false;
  }
  virtual bool HasCapture() const OVERRIDE {
    return mouse_capture_;
  }

 private:
  bool mouse_capture_;

  DISALLOW_COPY_AND_ASSIGN(NativeWidgetCapture);
};
#endif

// A typedef that inserts our mock-capture NativeWidget implementation for
// relevant platforms.
#if defined(USE_AURA)
typedef NativeWidgetCapture NativeWidgetPlatformForTest;
#elif defined(OS_WIN)
typedef NativeWidgetWin NativeWidgetPlatformForTest;
#endif

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

// A view that keeps track of the events it receives, but consumes no events.
class EventCountView : public View {
 public:
  EventCountView() {}
  virtual ~EventCountView() {}

  int GetEventCount(ui::EventType type) {
    return event_count_[type];
  }

  void ResetCounts() {
    event_count_.clear();
  }

 protected:
  // Overridden from View:
  virtual bool OnMousePressed(const ui::MouseEvent& event) OVERRIDE {
    RecordEvent(event);
    return false;
  }
  virtual bool OnMouseDragged(const ui::MouseEvent& event) OVERRIDE {
    RecordEvent(event);
    return false;
  }
  virtual void OnMouseReleased(const ui::MouseEvent& event) OVERRIDE {
    RecordEvent(event);
  }
  virtual void OnMouseMoved(const ui::MouseEvent& event) OVERRIDE {
    RecordEvent(event);
  }
  virtual void OnMouseEntered(const ui::MouseEvent& event) OVERRIDE {
    RecordEvent(event);
  }
  virtual void OnMouseExited(const ui::MouseEvent& event) OVERRIDE {
    RecordEvent(event);
  }
  virtual bool OnKeyPressed(const ui::KeyEvent& event) OVERRIDE {
    RecordEvent(event);
    return false;
  }
  virtual bool OnKeyReleased(const ui::KeyEvent& event) OVERRIDE {
    RecordEvent(event);
    return false;
  }
  virtual bool OnMouseWheel(const ui::MouseWheelEvent& event) OVERRIDE {
    RecordEvent(event);
    return false;
  }

  // Overridden from ui::EventHandler:
  virtual void OnKeyEvent(ui::KeyEvent* event) OVERRIDE {
    RecordEvent(*event);
  }
  virtual void OnMouseEvent(ui::MouseEvent* event) OVERRIDE {
    RecordEvent(*event);
  }
  virtual void OnScrollEvent(ui::ScrollEvent* event) OVERRIDE {
    RecordEvent(*event);
  }
  virtual void OnTouchEvent(ui::TouchEvent* event) OVERRIDE {
    RecordEvent(*event);
  }
  virtual void OnGestureEvent(ui::GestureEvent* event) OVERRIDE {
    RecordEvent(*event);
  }

 private:
  void RecordEvent(const ui::Event& event) {
    ++event_count_[event.type()];
  }

  std::map<ui::EventType, int> event_count_;

  DISALLOW_COPY_AND_ASSIGN(EventCountView);
};

// A view that keeps track of the events it receives, and consumes all scroll
// gesture events.
class ScrollableEventCountView : public EventCountView {
 public:
  ScrollableEventCountView() {}
  virtual ~ScrollableEventCountView() {}

 private:
  // Overridden from ui::EventHandler:
  virtual void OnGestureEvent(ui::GestureEvent* event) OVERRIDE {
    EventCountView::OnGestureEvent(event);
    switch (event->type()) {
      case ui::ET_GESTURE_SCROLL_BEGIN:
      case ui::ET_GESTURE_SCROLL_UPDATE:
      case ui::ET_GESTURE_SCROLL_END:
      case ui::ET_SCROLL_FLING_START:
        event->SetHandled();
        break;
      default:
        break;
    }
  }

  DISALLOW_COPY_AND_ASSIGN(ScrollableEventCountView);
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

class WidgetTest : public ViewsTestBase {
 public:
  WidgetTest() {}
  virtual ~WidgetTest() {}

  NativeWidget* CreatePlatformNativeWidget(
      internal::NativeWidgetDelegate* delegate) {
    return new NativeWidgetPlatformForTest(delegate);
  }

  Widget* CreateTopLevelPlatformWidget() {
    Widget* toplevel = new Widget;
    Widget::InitParams toplevel_params =
        CreateParams(Widget::InitParams::TYPE_WINDOW);
    toplevel_params.native_widget = CreatePlatformNativeWidget(toplevel);
    toplevel->Init(toplevel_params);
    return toplevel;
  }

  Widget* CreateChildPlatformWidget(gfx::NativeView parent_native_view) {
    Widget* child = new Widget;
    Widget::InitParams child_params =
        CreateParams(Widget::InitParams::TYPE_CONTROL);
    child_params.native_widget = CreatePlatformNativeWidget(child);
    child_params.parent = parent_native_view;
    child->Init(child_params);
    child->SetContentsView(new View);
    return child;
  }

#if defined(OS_WIN) && !defined(USE_AURA)
  // On Windows, it is possible for us to have a child window that is
  // TYPE_POPUP.
  Widget* CreateChildPopupPlatformWidget(gfx::NativeView parent_native_view) {
    Widget* child = new Widget;
    Widget::InitParams child_params =
        CreateParams(Widget::InitParams::TYPE_POPUP);
    child_params.child = true;
    child_params.native_widget = CreatePlatformNativeWidget(child);
    child_params.parent = parent_native_view;
    child->Init(child_params);
    child->SetContentsView(new View);
    return child;
  }
#endif

  Widget* CreateTopLevelNativeWidget() {
    Widget* toplevel = new Widget;
    Widget::InitParams params = CreateParams(Widget::InitParams::TYPE_WINDOW);
    toplevel->Init(params);
    toplevel->SetContentsView(new View);
    return toplevel;
  }

  Widget* CreateChildNativeWidgetWithParent(Widget* parent) {
    Widget* child = new Widget;
    Widget::InitParams params = CreateParams(Widget::InitParams::TYPE_CONTROL);
    params.parent = parent->GetNativeView();
    child->Init(params);
    child->SetContentsView(new View);
    return child;
  }

  Widget* CreateChildNativeWidget() {
    return CreateChildNativeWidgetWithParent(NULL);
  }
};

bool WidgetHasMouseCapture(const Widget* widget) {
  return static_cast<const internal::NativeWidgetPrivate*>(widget->
      native_widget())->HasCapture();
}

ui::WindowShowState GetWidgetShowState(const Widget* widget) {
  // Use IsMaximized/IsMinimized/IsFullScreen instead of GetWindowPlacement
  // because the former is implemented on all platforms but the latter is not.
  return widget->IsFullscreen() ? ui::SHOW_STATE_FULLSCREEN :
      widget->IsMaximized() ? ui::SHOW_STATE_MAXIMIZED :
      widget->IsMinimized() ? ui::SHOW_STATE_MINIMIZED :
                              ui::SHOW_STATE_NORMAL;
}

TEST_F(WidgetTest, WidgetInitParams) {
  ASSERT_FALSE(views_delegate().UseTransparentWindows());

  // Widgets are not transparent by default.
  Widget::InitParams init1;
  EXPECT_FALSE(init1.transparent);

  // Non-window widgets are not transparent either.
  Widget::InitParams init2(Widget::InitParams::TYPE_MENU);
  EXPECT_FALSE(init2.transparent);

  // A ViewsDelegate can set windows transparent by default.
  views_delegate().SetUseTransparentWindows(true);
  Widget::InitParams init3;
  EXPECT_TRUE(init3.transparent);

  // Non-window widgets stay opaque.
  Widget::InitParams init4(Widget::InitParams::TYPE_MENU);
  EXPECT_FALSE(init4.transparent);
}

////////////////////////////////////////////////////////////////////////////////
// Widget::GetTopLevelWidget tests.

TEST_F(WidgetTest, GetTopLevelWidget_Native) {
  // Create a hierarchy of native widgets.
  Widget* toplevel = CreateTopLevelPlatformWidget();
  gfx::NativeView parent = toplevel->GetNativeView();
  Widget* child = CreateChildPlatformWidget(parent);

  EXPECT_EQ(toplevel, toplevel->GetTopLevelWidget());
  EXPECT_EQ(toplevel, child->GetTopLevelWidget());

  toplevel->CloseNow();
  // |child| should be automatically destroyed with |toplevel|.
}

// Tests some grab/ungrab events.
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

  EXPECT_TRUE(WidgetHasMouseCapture(toplevel));
  EXPECT_TRUE(WidgetHasMouseCapture(child1));
  EXPECT_FALSE(WidgetHasMouseCapture(child2));

  ui::MouseEvent released(ui::ET_MOUSE_RELEASED, p1, p1,
                          ui::EF_LEFT_MOUSE_BUTTON);
  toplevel->OnMouseEvent(&released);

  EXPECT_FALSE(WidgetHasMouseCapture(toplevel));
  EXPECT_FALSE(WidgetHasMouseCapture(child1));
  EXPECT_FALSE(WidgetHasMouseCapture(child2));

  RunPendingMessages();

  // Click on child2
  gfx::Point p2(315, 45);
  ui::MouseEvent pressed2(ui::ET_MOUSE_PRESSED, p2, p2,
                          ui::EF_LEFT_MOUSE_BUTTON);
  toplevel->OnMouseEvent(&pressed2);
  EXPECT_TRUE(pressed2.handled());
  EXPECT_TRUE(WidgetHasMouseCapture(toplevel));
  EXPECT_TRUE(WidgetHasMouseCapture(child2));
  EXPECT_FALSE(WidgetHasMouseCapture(child1));

  ui::MouseEvent released2(ui::ET_MOUSE_RELEASED, p2, p2,
                           ui::EF_LEFT_MOUSE_BUTTON);
  toplevel->OnMouseEvent(&released2);
  EXPECT_FALSE(WidgetHasMouseCapture(toplevel));
  EXPECT_FALSE(WidgetHasMouseCapture(child1));
  EXPECT_FALSE(WidgetHasMouseCapture(child2));

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

// Test if a focus manager and an inputmethod work without CHECK failure
// when window activation changes.
TEST_F(WidgetTest, ChangeActivation) {
  Widget* top1 = CreateTopLevelPlatformWidget();
  // CreateInputMethod before activated
  top1->GetInputMethod();
  top1->Show();
  RunPendingMessages();

  Widget* top2 = CreateTopLevelPlatformWidget();
  top2->Show();
  RunPendingMessages();

  top1->Activate();
  RunPendingMessages();

  // Create InputMethod after deactivated.
  top2->GetInputMethod();
  top2->Activate();
  RunPendingMessages();

  top1->Activate();
  RunPendingMessages();

  top1->CloseNow();
  top2->CloseNow();
}

// Tests visibility of child widgets.
TEST_F(WidgetTest, Visibility) {
  Widget* toplevel = CreateTopLevelPlatformWidget();
  gfx::NativeView parent = toplevel->GetNativeView();
  Widget* child = CreateChildPlatformWidget(parent);

  EXPECT_FALSE(toplevel->IsVisible());
  EXPECT_FALSE(child->IsVisible());

  child->Show();

  EXPECT_FALSE(toplevel->IsVisible());
  EXPECT_FALSE(child->IsVisible());

  toplevel->Show();

  EXPECT_TRUE(toplevel->IsVisible());
  EXPECT_TRUE(child->IsVisible());

  toplevel->CloseNow();
  // |child| should be automatically destroyed with |toplevel|.
}

#if defined(OS_WIN) && !defined(USE_AURA)
// On Windows, it is possible to have child window that are TYPE_POPUP.  Unlike
// regular child windows, these should be created as hidden and must be shown
// explicitly.
TEST_F(WidgetTest, Visibility_ChildPopup) {
  Widget* toplevel = CreateTopLevelPlatformWidget();
  Widget* child_popup = CreateChildPopupPlatformWidget(
      toplevel->GetNativeView());

  EXPECT_FALSE(toplevel->IsVisible());
  EXPECT_FALSE(child_popup->IsVisible());

  toplevel->Show();

  EXPECT_TRUE(toplevel->IsVisible());
  EXPECT_FALSE(child_popup->IsVisible());

  child_popup->Show();

  EXPECT_TRUE(child_popup->IsVisible());

  toplevel->CloseNow();
  // |child_popup| should be automatically destroyed with |toplevel|.
}
#endif

////////////////////////////////////////////////////////////////////////////////
// Widget ownership tests.
//
// Tests various permutations of Widget ownership specified in the
// InitParams::Ownership param.

// A WidgetTest that supplies a toplevel widget for NativeWidget to parent to.
class WidgetOwnershipTest : public WidgetTest {
 public:
  WidgetOwnershipTest() {}
  virtual ~WidgetOwnershipTest() {}

  virtual void SetUp() {
    WidgetTest::SetUp();
    desktop_widget_ = CreateTopLevelPlatformWidget();
  }

  virtual void TearDown() {
    desktop_widget_->CloseNow();
    WidgetTest::TearDown();
  }

 private:
  Widget* desktop_widget_;

  DISALLOW_COPY_AND_ASSIGN(WidgetOwnershipTest);
};

// A bag of state to monitor destructions.
struct OwnershipTestState {
  OwnershipTestState() : widget_deleted(false), native_widget_deleted(false) {}

  bool widget_deleted;
  bool native_widget_deleted;
};

// A platform NativeWidget subclass that updates a bag of state when it is
// destroyed.
class OwnershipTestNativeWidget : public NativeWidgetPlatform {
 public:
  OwnershipTestNativeWidget(internal::NativeWidgetDelegate* delegate,
                            OwnershipTestState* state)
      : NativeWidgetPlatform(delegate),
        state_(state) {
  }
  virtual ~OwnershipTestNativeWidget() {
    state_->native_widget_deleted = true;
  }

 private:
  OwnershipTestState* state_;

  DISALLOW_COPY_AND_ASSIGN(OwnershipTestNativeWidget);
};

// A views NativeWidget subclass that updates a bag of state when it is
// destroyed.
class OwnershipTestNativeWidgetPlatform : public NativeWidgetPlatformForTest {
 public:
  OwnershipTestNativeWidgetPlatform(internal::NativeWidgetDelegate* delegate,
                                    OwnershipTestState* state)
      : NativeWidgetPlatformForTest(delegate),
        state_(state) {
  }
  virtual ~OwnershipTestNativeWidgetPlatform() {
    state_->native_widget_deleted = true;
  }

 private:
  OwnershipTestState* state_;

  DISALLOW_COPY_AND_ASSIGN(OwnershipTestNativeWidgetPlatform);
};

// A Widget subclass that updates a bag of state when it is destroyed.
class OwnershipTestWidget : public Widget {
 public:
  explicit OwnershipTestWidget(OwnershipTestState* state) : state_(state) {}
  virtual ~OwnershipTestWidget() {
    state_->widget_deleted = true;
  }

 private:
  OwnershipTestState* state_;

  DISALLOW_COPY_AND_ASSIGN(OwnershipTestWidget);
};

// Widget owns its NativeWidget, part 1: NativeWidget is a platform-native
// widget.
TEST_F(WidgetOwnershipTest, Ownership_WidgetOwnsPlatformNativeWidget) {
  OwnershipTestState state;

  scoped_ptr<Widget> widget(new OwnershipTestWidget(&state));
  Widget::InitParams params = CreateParams(Widget::InitParams::TYPE_POPUP);
  params.native_widget =
      new OwnershipTestNativeWidgetPlatform(widget.get(), &state);
  params.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  widget->Init(params);

  // Now delete the Widget, which should delete the NativeWidget.
  widget.reset();

  EXPECT_TRUE(state.widget_deleted);
  EXPECT_TRUE(state.native_widget_deleted);

  // TODO(beng): write test for this ownership scenario and the NativeWidget
  //             being deleted out from under the Widget.
}

// Widget owns its NativeWidget, part 2: NativeWidget is a NativeWidget.
TEST_F(WidgetOwnershipTest, Ownership_WidgetOwnsViewsNativeWidget) {
  OwnershipTestState state;

  scoped_ptr<Widget> widget(new OwnershipTestWidget(&state));
  Widget::InitParams params = CreateParams(Widget::InitParams::TYPE_POPUP);
  params.native_widget =
      new OwnershipTestNativeWidgetPlatform(widget.get(), &state);
  params.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  widget->Init(params);

  // Now delete the Widget, which should delete the NativeWidget.
  widget.reset();

  EXPECT_TRUE(state.widget_deleted);
  EXPECT_TRUE(state.native_widget_deleted);

  // TODO(beng): write test for this ownership scenario and the NativeWidget
  //             being deleted out from under the Widget.
}

// Widget owns its NativeWidget, part 3: NativeWidget is a NativeWidget,
// destroy the parent view.
TEST_F(WidgetOwnershipTest,
       Ownership_WidgetOwnsViewsNativeWidget_DestroyParentView) {
  OwnershipTestState state;

  Widget* toplevel = CreateTopLevelPlatformWidget();

  scoped_ptr<Widget> widget(new OwnershipTestWidget(&state));
  Widget::InitParams params = CreateParams(Widget::InitParams::TYPE_POPUP);
  params.native_widget =
      new OwnershipTestNativeWidgetPlatform(widget.get(), &state);
  params.parent = toplevel->GetNativeView();
  params.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  widget->Init(params);

  // Now close the toplevel, which deletes the view hierarchy.
  toplevel->CloseNow();

  RunPendingMessages();

  // This shouldn't delete the widget because it shouldn't be deleted
  // from the native side.
  EXPECT_FALSE(state.widget_deleted);
  EXPECT_FALSE(state.native_widget_deleted);

  // Now delete it explicitly.
  widget.reset();

  EXPECT_TRUE(state.widget_deleted);
  EXPECT_TRUE(state.native_widget_deleted);
}

// NativeWidget owns its Widget, part 1: NativeWidget is a platform-native
// widget.
TEST_F(WidgetOwnershipTest, Ownership_PlatformNativeWidgetOwnsWidget) {
  OwnershipTestState state;

  Widget* widget = new OwnershipTestWidget(&state);
  Widget::InitParams params = CreateParams(Widget::InitParams::TYPE_POPUP);
  params.native_widget =
      new OwnershipTestNativeWidgetPlatform(widget, &state);
  widget->Init(params);

  // Now destroy the native widget.
  widget->CloseNow();

  EXPECT_TRUE(state.widget_deleted);
  EXPECT_TRUE(state.native_widget_deleted);
}

// NativeWidget owns its Widget, part 2: NativeWidget is a NativeWidget.
TEST_F(WidgetOwnershipTest, Ownership_ViewsNativeWidgetOwnsWidget) {
  OwnershipTestState state;

  Widget* toplevel = CreateTopLevelPlatformWidget();

  Widget* widget = new OwnershipTestWidget(&state);
  Widget::InitParams params = CreateParams(Widget::InitParams::TYPE_POPUP);
  params.native_widget =
      new OwnershipTestNativeWidgetPlatform(widget, &state);
  params.parent = toplevel->GetNativeView();
  widget->Init(params);

  // Now destroy the native widget. This is achieved by closing the toplevel.
  toplevel->CloseNow();

  // The NativeWidget won't be deleted until after a return to the message loop
  // so we have to run pending messages before testing the destruction status.
  RunPendingMessages();

  EXPECT_TRUE(state.widget_deleted);
  EXPECT_TRUE(state.native_widget_deleted);
}

// NativeWidget owns its Widget, part 3: NativeWidget is a platform-native
// widget, destroyed out from under it by the OS.
TEST_F(WidgetOwnershipTest,
       Ownership_PlatformNativeWidgetOwnsWidget_NativeDestroy) {
  OwnershipTestState state;

  Widget* widget = new OwnershipTestWidget(&state);
  Widget::InitParams params = CreateParams(Widget::InitParams::TYPE_POPUP);
  params.native_widget =
      new OwnershipTestNativeWidgetPlatform(widget, &state);
  widget->Init(params);

  // Now simulate a destroy of the platform native widget from the OS:
#if defined(USE_AURA)
  delete widget->GetNativeView();
#elif defined(OS_WIN)
  DestroyWindow(widget->GetNativeView());
#endif

  EXPECT_TRUE(state.widget_deleted);
  EXPECT_TRUE(state.native_widget_deleted);
}

// NativeWidget owns its Widget, part 4: NativeWidget is a NativeWidget,
// destroyed by the view hierarchy that contains it.
TEST_F(WidgetOwnershipTest,
       Ownership_ViewsNativeWidgetOwnsWidget_NativeDestroy) {
  OwnershipTestState state;

  Widget* toplevel = CreateTopLevelPlatformWidget();

  Widget* widget = new OwnershipTestWidget(&state);
  Widget::InitParams params = CreateParams(Widget::InitParams::TYPE_POPUP);
  params.native_widget =
      new OwnershipTestNativeWidgetPlatform(widget, &state);
  params.parent = toplevel->GetNativeView();
  widget->Init(params);

  // Destroy the widget (achieved by closing the toplevel).
  toplevel->CloseNow();

  // The NativeWidget won't be deleted until after a return to the message loop
  // so we have to run pending messages before testing the destruction status.
  RunPendingMessages();

  EXPECT_TRUE(state.widget_deleted);
  EXPECT_TRUE(state.native_widget_deleted);
}

// NativeWidget owns its Widget, part 5: NativeWidget is a NativeWidget,
// we close it directly.
TEST_F(WidgetOwnershipTest,
       Ownership_ViewsNativeWidgetOwnsWidget_Close) {
  OwnershipTestState state;

  Widget* toplevel = CreateTopLevelPlatformWidget();

  Widget* widget = new OwnershipTestWidget(&state);
  Widget::InitParams params = CreateParams(Widget::InitParams::TYPE_POPUP);
  params.native_widget =
      new OwnershipTestNativeWidgetPlatform(widget, &state);
  params.parent = toplevel->GetNativeView();
  widget->Init(params);

  // Destroy the widget.
  widget->Close();
  toplevel->CloseNow();

  // The NativeWidget won't be deleted until after a return to the message loop
  // so we have to run pending messages before testing the destruction status.
  RunPendingMessages();

  EXPECT_TRUE(state.widget_deleted);
  EXPECT_TRUE(state.native_widget_deleted);
}

////////////////////////////////////////////////////////////////////////////////
// Widget observer tests.
//

class WidgetObserverTest : public WidgetTest, public WidgetObserver {
 public:
  WidgetObserverTest()
      : active_(NULL),
        widget_closed_(NULL),
        widget_activated_(NULL),
        widget_shown_(NULL),
        widget_hidden_(NULL),
        widget_bounds_changed_(NULL) {
  }

  virtual ~WidgetObserverTest() {}

  // Overridden from WidgetObserver:
  virtual void OnWidgetClosing(Widget* widget) OVERRIDE {
    if (active_ == widget)
      active_ = NULL;
    widget_closed_ = widget;
  }

  virtual void OnWidgetActivationChanged(Widget* widget,
                                         bool active) OVERRIDE {
    if (active) {
      if (widget_activated_)
        widget_activated_->Deactivate();
      widget_activated_ = widget;
      active_ = widget;
    } else {
      if (widget_activated_ == widget)
        widget_activated_ = NULL;
      widget_deactivated_ = widget;
    }
  }

  virtual void OnWidgetVisibilityChanged(Widget* widget,
                                         bool visible) OVERRIDE {
    if (visible)
      widget_shown_ = widget;
    else
      widget_hidden_ = widget;
  }

  virtual void OnWidgetBoundsChanged(Widget* widget,
                                     const gfx::Rect& new_bounds) OVERRIDE {
    widget_bounds_changed_ = widget;
  }

  void reset() {
    active_ = NULL;
    widget_closed_ = NULL;
    widget_activated_ = NULL;
    widget_deactivated_ = NULL;
    widget_shown_ = NULL;
    widget_hidden_ = NULL;
    widget_bounds_changed_ = NULL;
  }

  Widget* NewWidget() {
    Widget* widget = CreateTopLevelNativeWidget();
    widget->AddObserver(this);
    return widget;
  }

  const Widget* active() const { return active_; }
  const Widget* widget_closed() const { return widget_closed_; }
  const Widget* widget_activated() const { return widget_activated_; }
  const Widget* widget_deactivated() const { return widget_deactivated_; }
  const Widget* widget_shown() const { return widget_shown_; }
  const Widget* widget_hidden() const { return widget_hidden_; }
  const Widget* widget_bounds_changed() const { return widget_bounds_changed_; }

 private:

  Widget* active_;

  Widget* widget_closed_;
  Widget* widget_activated_;
  Widget* widget_deactivated_;
  Widget* widget_shown_;
  Widget* widget_hidden_;
  Widget* widget_bounds_changed_;
};

TEST_F(WidgetObserverTest, DISABLED_ActivationChange) {
  Widget* toplevel = CreateTopLevelPlatformWidget();

  Widget* toplevel1 = NewWidget();
  Widget* toplevel2 = NewWidget();

  toplevel1->Show();
  toplevel2->Show();

  reset();

  toplevel1->Activate();

  RunPendingMessages();
  EXPECT_EQ(toplevel1, widget_activated());

  toplevel2->Activate();
  RunPendingMessages();
  EXPECT_EQ(toplevel1, widget_deactivated());
  EXPECT_EQ(toplevel2, widget_activated());
  EXPECT_EQ(toplevel2, active());

  toplevel->CloseNow();
}

TEST_F(WidgetObserverTest, DISABLED_VisibilityChange) {
  Widget* toplevel = CreateTopLevelPlatformWidget();

  Widget* child1 = NewWidget();
  Widget* child2 = NewWidget();

  toplevel->Show();
  child1->Show();
  child2->Show();

  reset();

  child1->Hide();
  EXPECT_EQ(child1, widget_hidden());

  child2->Hide();
  EXPECT_EQ(child2, widget_hidden());

  child1->Show();
  EXPECT_EQ(child1, widget_shown());

  child2->Show();
  EXPECT_EQ(child2, widget_shown());

  toplevel->CloseNow();
}

TEST_F(WidgetObserverTest, DestroyBubble) {
  Widget* anchor = CreateTopLevelPlatformWidget();
  View* view = new View;
  anchor->SetContentsView(view);
  anchor->Show();

  BubbleDelegateView* bubble_delegate =
      new BubbleDelegateView(view, BubbleBorder::NONE);
  Widget* bubble_widget(BubbleDelegateView::CreateBubble(bubble_delegate));
  bubble_widget->Show();
  bubble_widget->CloseNow();

  anchor->Hide();
  anchor->CloseNow();
}

TEST_F(WidgetObserverTest, WidgetBoundsChanged) {
  Widget* child1 = NewWidget();
  Widget* child2 = NewWidget();

  child1->OnNativeWidgetMove();
  EXPECT_EQ(child1, widget_bounds_changed());

  child2->OnNativeWidgetMove();
  EXPECT_EQ(child2, widget_bounds_changed());

  child1->OnNativeWidgetSizeChanged(gfx::Size());
  EXPECT_EQ(child1, widget_bounds_changed());

  child2->OnNativeWidgetSizeChanged(gfx::Size());
  EXPECT_EQ(child2, widget_bounds_changed());
}

#if !defined(USE_AURA) && defined(OS_WIN)
// Aura needs shell to maximize/fullscreen window.
// NativeWidgetGtk doesn't implement GetRestoredBounds.
TEST_F(WidgetTest, GetRestoredBounds) {
  Widget* toplevel = CreateTopLevelPlatformWidget();
  EXPECT_EQ(toplevel->GetWindowBoundsInScreen().ToString(),
            toplevel->GetRestoredBounds().ToString());
  toplevel->Show();
  toplevel->Maximize();
  RunPendingMessages();
  EXPECT_NE(toplevel->GetWindowBoundsInScreen().ToString(),
            toplevel->GetRestoredBounds().ToString());
  EXPECT_GT(toplevel->GetRestoredBounds().width(), 0);
  EXPECT_GT(toplevel->GetRestoredBounds().height(), 0);

  toplevel->Restore();
  RunPendingMessages();
  EXPECT_EQ(toplevel->GetWindowBoundsInScreen().ToString(),
            toplevel->GetRestoredBounds().ToString());

  toplevel->SetFullscreen(true);
  RunPendingMessages();
  EXPECT_NE(toplevel->GetWindowBoundsInScreen().ToString(),
            toplevel->GetRestoredBounds().ToString());
  EXPECT_GT(toplevel->GetRestoredBounds().width(), 0);
  EXPECT_GT(toplevel->GetRestoredBounds().height(), 0);
}
#endif

// Test that window state is not changed after getting out of full screen.
TEST_F(WidgetTest, ExitFullscreenRestoreState) {
  Widget* toplevel = CreateTopLevelPlatformWidget();

  toplevel->Show();
  RunPendingMessages();

  // This should be a normal state window.
  EXPECT_EQ(ui::SHOW_STATE_NORMAL, GetWidgetShowState(toplevel));

  toplevel->SetFullscreen(true);
  while (GetWidgetShowState(toplevel) != ui::SHOW_STATE_FULLSCREEN)
    RunPendingMessages();
  toplevel->SetFullscreen(false);
  while (GetWidgetShowState(toplevel) == ui::SHOW_STATE_FULLSCREEN)
    RunPendingMessages();

  // And it should still be in normal state after getting out of full screen.
  EXPECT_EQ(ui::SHOW_STATE_NORMAL, GetWidgetShowState(toplevel));

  // Now, make it maximized.
  toplevel->Maximize();
  while (GetWidgetShowState(toplevel) != ui::SHOW_STATE_MAXIMIZED)
    RunPendingMessages();

  toplevel->SetFullscreen(true);
  while (GetWidgetShowState(toplevel) != ui::SHOW_STATE_FULLSCREEN)
    RunPendingMessages();
  toplevel->SetFullscreen(false);
  while (GetWidgetShowState(toplevel) == ui::SHOW_STATE_FULLSCREEN)
    RunPendingMessages();

  // And it stays maximized after getting out of full screen.
  EXPECT_EQ(ui::SHOW_STATE_MAXIMIZED, GetWidgetShowState(toplevel));

  // Clean up.
  toplevel->Close();
  RunPendingMessages();
}

TEST_F(WidgetTest, ResetCaptureOnGestureEnd) {
  Widget* toplevel = CreateTopLevelPlatformWidget();
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

  toplevel->OnMouseEvent(&press);
  toplevel->OnMouseEvent(&release);
  EXPECT_EQ(0, mouse->pressed());

  // The end of the gesture should release the capture, and pressing on |mouse|
  // should now reach |mouse|.
  toplevel->OnGestureEvent(&end);
  toplevel->OnMouseEvent(&press);
  toplevel->OnMouseEvent(&release);
  EXPECT_EQ(1, mouse->pressed());

  toplevel->Close();
  RunPendingMessages();
}

#if defined(USE_AURA)
// The key-event propagation from Widget happens differently on aura and
// non-aura systems because of the difference in IME. So this test works only on
// aura.
TEST_F(WidgetTest, KeyboardInputEvent) {
  Widget* toplevel = CreateTopLevelPlatformWidget();
  View* container = new View;
  toplevel->SetContentsView(container);

  Textfield* textfield = new Textfield();
  textfield->SetText(ASCIIToUTF16("some text"));
  container->AddChildView(textfield);
  toplevel->Show();
  textfield->RequestFocus();

  // The press gets handled. The release doesn't have an effect.
  ui::KeyEvent backspace_p(ui::ET_KEY_PRESSED, ui::VKEY_DELETE, 0, false);
  toplevel->OnKeyEvent(&backspace_p);
  EXPECT_TRUE(backspace_p.stopped_propagation());
  ui::KeyEvent backspace_r(ui::ET_KEY_RELEASED, ui::VKEY_DELETE, 0, false);
  toplevel->OnKeyEvent(&backspace_r);
  EXPECT_FALSE(backspace_r.handled());

  toplevel->Close();
}

// Verifies bubbles result in a focus lost when shown.
TEST_F(WidgetTest, FocusChangesOnBubble) {
  // Create a widget, show and activate it and focus the contents view.
  View* contents_view = new View;
  contents_view->set_focusable(true);
  Widget widget;
  Widget::InitParams init_params =
      CreateParams(Widget::InitParams::TYPE_WINDOW);
  init_params.bounds = gfx::Rect(0, 0, 200, 200);
  init_params.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
#if !defined(OS_CHROMEOS)
  init_params.native_widget = new DesktopNativeWidgetAura(&widget);
#endif
  widget.Init(init_params);
  widget.SetContentsView(contents_view);
  widget.Show();
  widget.Activate();
  contents_view->RequestFocus();
  EXPECT_TRUE(contents_view->HasFocus());

  // Show a buble.
  BubbleDelegateView* bubble_delegate_view =
      new BubbleDelegateView(contents_view, BubbleBorder::TOP_LEFT);
  bubble_delegate_view->set_focusable(true);
  Widget* bubble_widget =
      BubbleDelegateView::CreateBubble(bubble_delegate_view);
  bubble_delegate_view->Show();
  bubble_delegate_view->RequestFocus();

  // |contents_view_| should no longer have focus.
  EXPECT_FALSE(contents_view->HasFocus());
  EXPECT_TRUE(bubble_delegate_view->HasFocus());

  bubble_widget->CloseNow();

  // Closing the bubble should result in focus going back to the contents view.
  EXPECT_TRUE(contents_view->HasFocus());
}

// Desktop native widget Aura tests are for non Chrome OS platforms.
#if !defined(OS_CHROMEOS)
// This class validates whether paints are received for a visible Widget.
// To achieve this it overrides the Show and Close methods on the Widget class
// and sets state whether subsequent paints are expected.
class DesktopAuraTestValidPaintWidget : public views::Widget {
 public:
  DesktopAuraTestValidPaintWidget()
    : expect_paint_(true),
      received_paint_while_hidden_(false) {
  }

  virtual ~DesktopAuraTestValidPaintWidget() {
  }

  virtual void Show() OVERRIDE {
    expect_paint_ = true;
    views::Widget::Show();
  }

  virtual void Close() OVERRIDE {
    expect_paint_ = false;
    views::Widget::Close();
  }

  void Hide() {
    expect_paint_ = false;
    views::Widget::Hide();
  }

  virtual void OnNativeWidgetPaint(gfx::Canvas* canvas) OVERRIDE {
    EXPECT_TRUE(expect_paint_);
    if (!expect_paint_)
      received_paint_while_hidden_ = true;
    views::Widget::OnNativeWidgetPaint(canvas);
  }

  bool received_paint_while_hidden() const {
    return received_paint_while_hidden_;
  }

 private:
  bool expect_paint_;
  bool received_paint_while_hidden_;
};

TEST_F(WidgetTest, DesktopNativeWidgetAuraNoPaintAfterCloseTest) {
  View* contents_view = new View;
  contents_view->set_focusable(true);
  DesktopAuraTestValidPaintWidget widget;
  Widget::InitParams init_params =
      CreateParams(Widget::InitParams::TYPE_WINDOW);
  init_params.bounds = gfx::Rect(0, 0, 200, 200);
  init_params.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  init_params.native_widget = new DesktopNativeWidgetAura(&widget);
  widget.Init(init_params);
  widget.SetContentsView(contents_view);
  widget.Show();
  widget.Activate();
  RunPendingMessages();
  widget.SchedulePaintInRect(init_params.bounds);
  widget.Close();
  RunPendingMessages();
  EXPECT_FALSE(widget.received_paint_while_hidden());
}

TEST_F(WidgetTest, DesktopNativeWidgetAuraNoPaintAfterHideTest) {
  View* contents_view = new View;
  contents_view->set_focusable(true);
  DesktopAuraTestValidPaintWidget widget;
  Widget::InitParams init_params =
      CreateParams(Widget::InitParams::TYPE_WINDOW);
  init_params.bounds = gfx::Rect(0, 0, 200, 200);
  init_params.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  init_params.native_widget = new DesktopNativeWidgetAura(&widget);
  widget.Init(init_params);
  widget.SetContentsView(contents_view);
  widget.Show();
  widget.Activate();
  RunPendingMessages();
  widget.SchedulePaintInRect(init_params.bounds);
  widget.Hide();
  RunPendingMessages();
  EXPECT_FALSE(widget.received_paint_while_hidden());
  widget.Close();
}

#endif  // !defined(OS_CHROMEOS)

// Tests that wheel events generted from scroll events are targetted to the
// views under the cursor when the focused view does not processed them.
TEST_F(WidgetTest, WheelEventsFromScrollEventTarget) {
  EventCountView* focused_view = new EventCountView;
  focused_view->set_focusable(true);

  EventCountView* cursor_view = new EventCountView;

  focused_view->SetBounds(0, 0, 50, 40);
  cursor_view->SetBounds(60, 0, 50, 40);

  Widget* widget = CreateTopLevelPlatformWidget();
  widget->GetRootView()->AddChildView(focused_view);
  widget->GetRootView()->AddChildView(cursor_view);

  focused_view->RequestFocus();
  EXPECT_TRUE(focused_view->HasFocus());

  // Generate a scroll event on the cursor view. The focused view will receive a
  // wheel event, but since it doesn't process the event, the view under the
  // cursor will receive the wheel event.
  ui::ScrollEvent scroll(ui::ET_SCROLL, gfx::Point(65, 5), 0, 0, 20);
  widget->OnScrollEvent(&scroll);

  EXPECT_EQ(0, focused_view->GetEventCount(ui::ET_SCROLL));
  EXPECT_EQ(1, focused_view->GetEventCount(ui::ET_MOUSEWHEEL));

  EXPECT_EQ(1, cursor_view->GetEventCount(ui::ET_SCROLL));
  EXPECT_EQ(1, cursor_view->GetEventCount(ui::ET_MOUSEWHEEL));

  focused_view->ResetCounts();
  cursor_view->ResetCounts();

  ui::ScrollEvent scroll2(ui::ET_SCROLL, gfx::Point(5, 5), 0, 0, 20);
  widget->OnScrollEvent(&scroll2);
  EXPECT_EQ(1, focused_view->GetEventCount(ui::ET_SCROLL));
  EXPECT_EQ(1, focused_view->GetEventCount(ui::ET_MOUSEWHEEL));

  EXPECT_EQ(0, cursor_view->GetEventCount(ui::ET_SCROLL));
  EXPECT_EQ(0, cursor_view->GetEventCount(ui::ET_MOUSEWHEEL));

  widget->CloseNow();
}

#endif  // defined(USE_AURA)

// Tests that if a scroll-begin gesture is not handled, then subsequent scroll
// events are not dispatched to any view.
TEST_F(WidgetTest, GestureScrollEventDispatching) {
  EventCountView* noscroll_view = new EventCountView;
  EventCountView* scroll_view = new ScrollableEventCountView;

  noscroll_view->SetBounds(0, 0, 50, 40);
  scroll_view->SetBounds(60, 0, 40, 40);

  Widget* widget = CreateTopLevelPlatformWidget();
  widget->GetRootView()->AddChildView(noscroll_view);
  widget->GetRootView()->AddChildView(scroll_view);

  {
    ui::GestureEvent begin(ui::ET_GESTURE_SCROLL_BEGIN,
        5, 5, 0, base::TimeDelta(),
        ui::GestureEventDetails(ui::ET_GESTURE_SCROLL_BEGIN, 0, 0),
        1);
    widget->OnGestureEvent(&begin);
    ui::GestureEvent update(ui::ET_GESTURE_SCROLL_UPDATE,
        25, 15, 0, base::TimeDelta(),
        ui::GestureEventDetails(ui::ET_GESTURE_SCROLL_UPDATE, 20, 10),
        1);
    widget->OnGestureEvent(&update);
    ui::GestureEvent end(ui::ET_GESTURE_SCROLL_END,
        25, 15, 0, base::TimeDelta(),
        ui::GestureEventDetails(ui::ET_GESTURE_SCROLL_END, 0, 0),
        1);
    widget->OnGestureEvent(&end);

    EXPECT_EQ(1, noscroll_view->GetEventCount(ui::ET_GESTURE_SCROLL_BEGIN));
    EXPECT_EQ(0, noscroll_view->GetEventCount(ui::ET_GESTURE_SCROLL_UPDATE));
    EXPECT_EQ(0, noscroll_view->GetEventCount(ui::ET_GESTURE_SCROLL_END));
  }

  {
    ui::GestureEvent begin(ui::ET_GESTURE_SCROLL_BEGIN,
        65, 5, 0, base::TimeDelta(),
        ui::GestureEventDetails(ui::ET_GESTURE_SCROLL_BEGIN, 0, 0),
        1);
    widget->OnGestureEvent(&begin);
    ui::GestureEvent update(ui::ET_GESTURE_SCROLL_UPDATE,
        85, 15, 0, base::TimeDelta(),
        ui::GestureEventDetails(ui::ET_GESTURE_SCROLL_UPDATE, 20, 10),
        1);
    widget->OnGestureEvent(&update);
    ui::GestureEvent end(ui::ET_GESTURE_SCROLL_END,
        85, 15, 0, base::TimeDelta(),
        ui::GestureEventDetails(ui::ET_GESTURE_SCROLL_END, 0, 0),
        1);
    widget->OnGestureEvent(&end);

    EXPECT_EQ(1, scroll_view->GetEventCount(ui::ET_GESTURE_SCROLL_BEGIN));
    EXPECT_EQ(1, scroll_view->GetEventCount(ui::ET_GESTURE_SCROLL_UPDATE));
    EXPECT_EQ(1, scroll_view->GetEventCount(ui::ET_GESTURE_SCROLL_END));
  }

  widget->CloseNow();
}

}  // namespace
}  // namespace views
