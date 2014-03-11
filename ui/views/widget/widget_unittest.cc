// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <set>

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/client/window_tree_client.h"
#include "ui/aura/test/event_generator.h"
#include "ui/aura/test/test_window_delegate.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/hit_test.h"
#include "ui/events/event_processor.h"
#include "ui/events/event_utils.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/point.h"
#include "ui/views/bubble/bubble_delegate.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/test/test_views_delegate.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/views_delegate.h"
#include "ui/views/widget/native_widget_aura.h"
#include "ui/views/widget/native_widget_delegate.h"
#include "ui/views/widget/root_view.h"
#include "ui/views/widget/widget_deletion_observer.h"
#include "ui/views/window/dialog_delegate.h"
#include "ui/views/window/native_frame_view.h"

#if !defined(OS_CHROMEOS)
#include "ui/views/widget/desktop_aura/desktop_native_widget_aura.h"
#endif

#if defined(OS_WIN)
#include "ui/views/win/hwnd_util.h"
#endif

namespace views {
namespace test {

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

// A view that implements GetMinimumSize.
class MinimumSizeFrameView : public NativeFrameView {
 public:
  explicit MinimumSizeFrameView(Widget* frame): NativeFrameView(frame) {}
  virtual ~MinimumSizeFrameView() {}

 private:
  // Overridden from View:
  virtual gfx::Size GetMinimumSize() OVERRIDE {
    return gfx::Size(300, 400);
  }

  DISALLOW_COPY_AND_ASSIGN(MinimumSizeFrameView);
};

// An event handler that simply keeps a count of the different types of events
// it receives.
class EventCountHandler : public ui::EventHandler {
 public:
  EventCountHandler() {}
  virtual ~EventCountHandler() {}

  int GetEventCount(ui::EventType type) {
    return event_count_[type];
  }

  void ResetCounts() {
    event_count_.clear();
  }

 protected:
  // Overridden from ui::EventHandler:
  virtual void OnEvent(ui::Event* event) OVERRIDE {
    RecordEvent(*event);
    ui::EventHandler::OnEvent(event);
  }

 private:
  void RecordEvent(const ui::Event& event) {
    ++event_count_[event.type()];
  }

  std::map<ui::EventType, int> event_count_;

  DISALLOW_COPY_AND_ASSIGN(EventCountHandler);
};

// Class that closes the widget (which ends up deleting it immediately) when the
// appropriate event is received.
class CloseWidgetView : public View {
 public:
  explicit CloseWidgetView(ui::EventType event_type)
      : event_type_(event_type) {
  }

  // ui::EventHandler override:
  virtual void OnEvent(ui::Event* event) OVERRIDE {
    if (event->type() == event_type_) {
      // Go through NativeWidgetPrivate to simulate what happens if the OS
      // deletes the NativeWindow out from under us.
      GetWidget()->native_widget_private()->CloseNow();
    } else {
      View::OnEvent(event);
      if (!event->IsTouchEvent())
        event->SetHandled();
    }
  }

 private:
  const ui::EventType event_type_;

  DISALLOW_COPY_AND_ASSIGN(CloseWidgetView);
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

TEST_F(WidgetTest, WidgetInitParams) {
  // Widgets are not transparent by default.
  Widget::InitParams init1;
  EXPECT_EQ(Widget::InitParams::INFER_OPACITY, init1.opacity);
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
class OwnershipTestNativeWidget : public NativeWidgetAura {
 public:
  OwnershipTestNativeWidget(internal::NativeWidgetDelegate* delegate,
                            OwnershipTestState* state)
      : NativeWidgetAura(delegate),
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
class OwnershipTestNativeWidgetAura : public NativeWidgetCapture {
 public:
  OwnershipTestNativeWidgetAura(internal::NativeWidgetDelegate* delegate,
                                OwnershipTestState* state)
      : NativeWidgetCapture(delegate),
        state_(state) {
  }
  virtual ~OwnershipTestNativeWidgetAura() {
    state_->native_widget_deleted = true;
  }

 private:
  OwnershipTestState* state_;

  DISALLOW_COPY_AND_ASSIGN(OwnershipTestNativeWidgetAura);
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
      new OwnershipTestNativeWidgetAura(widget.get(), &state);
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
      new OwnershipTestNativeWidgetAura(widget.get(), &state);
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
      new OwnershipTestNativeWidgetAura(widget.get(), &state);
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
      new OwnershipTestNativeWidgetAura(widget, &state);
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
      new OwnershipTestNativeWidgetAura(widget, &state);
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
      new OwnershipTestNativeWidgetAura(widget, &state);
  widget->Init(params);

  // Now simulate a destroy of the platform native widget from the OS:
  delete widget->GetNativeView();

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
      new OwnershipTestNativeWidgetAura(widget, &state);
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
      new OwnershipTestNativeWidgetAura(widget, &state);
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

// Widget owns its NativeWidget and has a WidgetDelegateView as its contents.
TEST_F(WidgetOwnershipTest,
       Ownership_WidgetOwnsNativeWidgetWithWithWidgetDelegateView) {
  OwnershipTestState state;

  WidgetDelegateView* delegate_view = new WidgetDelegateView;

  scoped_ptr<Widget> widget(new OwnershipTestWidget(&state));
  Widget::InitParams params = CreateParams(Widget::InitParams::TYPE_POPUP);
  params.native_widget =
      new OwnershipTestNativeWidgetAura(widget.get(), &state);
  params.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.delegate = delegate_view;
  widget->Init(params);
  widget->SetContentsView(delegate_view);

  // Now delete the Widget. There should be no crash or use-after-free.
  widget.reset();

  EXPECT_TRUE(state.widget_deleted);
  EXPECT_TRUE(state.native_widget_deleted);
}

////////////////////////////////////////////////////////////////////////////////
// Test to verify using various Widget methods doesn't crash when the underlying
// NativeView is destroyed.
//
class WidgetWithDestroyedNativeViewTest : public ViewsTestBase {
 public:
  WidgetWithDestroyedNativeViewTest() {}
  virtual ~WidgetWithDestroyedNativeViewTest() {}

  void InvokeWidgetMethods(Widget* widget) {
    widget->GetNativeView();
    widget->GetNativeWindow();
    ui::Accelerator accelerator;
    widget->GetAccelerator(0, &accelerator);
    widget->GetTopLevelWidget();
    widget->GetWindowBoundsInScreen();
    widget->GetClientAreaBoundsInScreen();
    widget->SetBounds(gfx::Rect(0, 0, 100, 80));
    widget->SetSize(gfx::Size(10, 11));
    widget->SetBoundsConstrained(gfx::Rect(0, 0, 120, 140));
    widget->SetVisibilityChangedAnimationsEnabled(false);
    widget->StackAtTop();
    widget->IsClosed();
    widget->Close();
    widget->Hide();
    widget->Activate();
    widget->Deactivate();
    widget->IsActive();
    widget->DisableInactiveRendering();
    widget->SetAlwaysOnTop(true);
    widget->IsAlwaysOnTop();
    widget->Maximize();
    widget->Minimize();
    widget->Restore();
    widget->IsMaximized();
    widget->IsFullscreen();
    widget->SetOpacity(0);
    widget->SetUseDragFrame(true);
    widget->FlashFrame(true);
    widget->IsVisible();
    widget->GetThemeProvider();
    widget->GetNativeTheme();
    widget->GetFocusManager();
    widget->GetInputMethod();
    widget->SchedulePaintInRect(gfx::Rect(0, 0, 1, 2));
    widget->IsMouseEventsEnabled();
    widget->SetNativeWindowProperty("xx", widget);
    widget->GetNativeWindowProperty("xx");
    widget->GetFocusTraversable();
    widget->GetLayer();
    widget->ReorderNativeViews();
    widget->SetCapture(widget->GetRootView());
    widget->ReleaseCapture();
    widget->HasCapture();
    widget->GetWorkAreaBoundsInScreen();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(WidgetWithDestroyedNativeViewTest);
};

TEST_F(WidgetWithDestroyedNativeViewTest, Test) {
  {
    Widget widget;
    Widget::InitParams params = CreateParams(Widget::InitParams::TYPE_POPUP);
    params.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
    widget.Init(params);
    widget.Show();

    widget.native_widget_private()->CloseNow();
    InvokeWidgetMethods(&widget);
  }
#if !defined(OS_CHROMEOS)
  {
    Widget widget;
    Widget::InitParams params = CreateParams(Widget::InitParams::TYPE_POPUP);
    params.native_widget = new DesktopNativeWidgetAura(&widget);
    params.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
    widget.Init(params);
    widget.Show();

    widget.native_widget_private()->CloseNow();
    InvokeWidgetMethods(&widget);
  }
#endif
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
  virtual void OnWidgetDestroying(Widget* widget) OVERRIDE {
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
  anchor->Show();

  BubbleDelegateView* bubble_delegate =
      new BubbleDelegateView(anchor->client_view(), BubbleBorder::NONE);
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

#if defined(false)
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

// The key-event propagation from Widget happens differently on aura and
// non-aura systems because of the difference in IME. So this test works only on
// aura.
TEST_F(WidgetTest, KeyboardInputEvent) {
  Widget* toplevel = CreateTopLevelPlatformWidget();
  View* container = toplevel->client_view();

  Textfield* textfield = new Textfield();
  textfield->SetText(base::ASCIIToUTF16("some text"));
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
// TODO(msw): this tests relies on focus, it needs to be in
// interactive_ui_tests.
TEST_F(WidgetTest, DISABLED_FocusChangesOnBubble) {
  // Create a widget, show and activate it and focus the contents view.
  View* contents_view = new View;
  contents_view->SetFocusable(true);
  Widget widget;
  Widget::InitParams init_params =
      CreateParams(Widget::InitParams::TYPE_WINDOW_FRAMELESS);
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

  // Show a bubble.
  BubbleDelegateView* bubble_delegate_view =
      new BubbleDelegateView(contents_view, BubbleBorder::TOP_LEFT);
  bubble_delegate_view->SetFocusable(true);
  BubbleDelegateView::CreateBubble(bubble_delegate_view)->Show();
  bubble_delegate_view->RequestFocus();

  // |contents_view_| should no longer have focus.
  EXPECT_FALSE(contents_view->HasFocus());
  EXPECT_TRUE(bubble_delegate_view->HasFocus());

  bubble_delegate_view->GetWidget()->CloseNow();

  // Closing the bubble should result in focus going back to the contents view.
  EXPECT_TRUE(contents_view->HasFocus());
}

class TestBubbleDelegateView : public BubbleDelegateView {
 public:
  TestBubbleDelegateView(View* anchor)
      : BubbleDelegateView(anchor, BubbleBorder::NONE),
        reset_controls_called_(false) {}
  virtual ~TestBubbleDelegateView() {}

  virtual bool ShouldShowCloseButton() const OVERRIDE {
    reset_controls_called_ = true;
    return true;
  }

  mutable bool reset_controls_called_;
};

TEST_F(WidgetTest, BubbleControlsResetOnInit) {
  Widget* anchor = CreateTopLevelPlatformWidget();
  anchor->Show();

  TestBubbleDelegateView* bubble_delegate =
      new TestBubbleDelegateView(anchor->client_view());
  Widget* bubble_widget(BubbleDelegateView::CreateBubble(bubble_delegate));
  EXPECT_TRUE(bubble_delegate->reset_controls_called_);
  bubble_widget->Show();
  bubble_widget->CloseNow();

  anchor->Hide();
  anchor->CloseNow();
}

// Desktop native widget Aura tests are for non Chrome OS platforms.
#if !defined(OS_CHROMEOS)
// Test to ensure that after minimize, view width is set to zero.
TEST_F(WidgetTest, TestViewWidthAfterMinimizingWidget) {
  // Create a widget.
  Widget widget;
  Widget::InitParams init_params =
      CreateParams(Widget::InitParams::TYPE_WINDOW);
  init_params.show_state = ui::SHOW_STATE_NORMAL;
  gfx::Rect initial_bounds(0, 0, 300, 400);
  init_params.bounds = initial_bounds;
  init_params.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  init_params.native_widget = new DesktopNativeWidgetAura(&widget);
  widget.Init(init_params);
  NonClientView* non_client_view = widget.non_client_view();
  NonClientFrameView* frame_view = new MinimumSizeFrameView(&widget);
  non_client_view->SetFrameView(frame_view);
  widget.Show();
  widget.Minimize();
  EXPECT_EQ(0, frame_view->width());
}

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
  contents_view->SetFocusable(true);
  DesktopAuraTestValidPaintWidget widget;
  Widget::InitParams init_params =
      CreateParams(Widget::InitParams::TYPE_WINDOW_FRAMELESS);
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
  contents_view->SetFocusable(true);
  DesktopAuraTestValidPaintWidget widget;
  Widget::InitParams init_params =
      CreateParams(Widget::InitParams::TYPE_WINDOW_FRAMELESS);
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

// This class provides functionality to create fullscreen and top level popup
// windows. It additionally tests whether the destruction of these windows
// occurs correctly in desktop AURA without crashing.
// It provides facilities to test the following cases:-
// 1. Child window destroyed which should lead to the destruction of the
//    parent.
// 2. Parent window destroyed which should lead to the child being destroyed.
class DesktopAuraTopLevelWindowTest
    : public views::TestViewsDelegate,
      public aura::WindowObserver {
 public:
  DesktopAuraTopLevelWindowTest()
      : top_level_widget_(NULL),
        owned_window_(NULL),
        owner_destroyed_(false),
        owned_window_destroyed_(false) {}

  virtual ~DesktopAuraTopLevelWindowTest() {
    EXPECT_TRUE(owner_destroyed_);
    EXPECT_TRUE(owned_window_destroyed_);
    top_level_widget_ = NULL;
    owned_window_ = NULL;
  }

  // views::TestViewsDelegate overrides.
  virtual void OnBeforeWidgetInit(
      Widget::InitParams* params,
      internal::NativeWidgetDelegate* delegate) OVERRIDE {
    if (!params->native_widget)
      params->native_widget = new views::DesktopNativeWidgetAura(delegate);
  }

  void CreateTopLevelWindow(const gfx::Rect& bounds, bool fullscreen) {
    Widget::InitParams init_params;
    init_params.type = Widget::InitParams::TYPE_WINDOW;
    init_params.bounds = bounds;
    init_params.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
    init_params.layer_type = aura::WINDOW_LAYER_NOT_DRAWN;
    init_params.accept_events = fullscreen;

    widget_.Init(init_params);

    owned_window_ = new aura::Window(&child_window_delegate_);
    owned_window_->SetType(ui::wm::WINDOW_TYPE_NORMAL);
    owned_window_->SetName("TestTopLevelWindow");
    if (fullscreen) {
      owned_window_->SetProperty(aura::client::kShowStateKey,
                                 ui::SHOW_STATE_FULLSCREEN);
    } else {
      owned_window_->SetType(ui::wm::WINDOW_TYPE_MENU);
    }
    owned_window_->Init(aura::WINDOW_LAYER_TEXTURED);
    aura::client::ParentWindowWithContext(
        owned_window_,
        widget_.GetNativeView()->GetRootWindow(),
        gfx::Rect(0, 0, 1900, 1600));
    owned_window_->Show();
    owned_window_->AddObserver(this);

    ASSERT_TRUE(owned_window_->parent() != NULL);
    owned_window_->parent()->AddObserver(this);

    top_level_widget_ =
        views::Widget::GetWidgetForNativeView(owned_window_->parent());
    ASSERT_TRUE(top_level_widget_ != NULL);
  }

  void DestroyOwnedWindow() {
    ASSERT_TRUE(owned_window_ != NULL);
    delete owned_window_;
  }

  void DestroyOwnerWindow() {
    ASSERT_TRUE(top_level_widget_ != NULL);
    top_level_widget_->CloseNow();
  }

  virtual void OnWindowDestroying(aura::Window* window) OVERRIDE {
    window->RemoveObserver(this);
    if (window == owned_window_) {
      owned_window_destroyed_ = true;
    } else if (window == top_level_widget_->GetNativeView()) {
      owner_destroyed_ = true;
    } else {
      ADD_FAILURE() << "Unexpected window destroyed callback: " << window;
    }
  }

  aura::Window* owned_window() {
    return owned_window_;
  }

  views::Widget* top_level_widget() {
    return top_level_widget_;
  }

 private:
  views::Widget widget_;
  views::Widget* top_level_widget_;
  aura::Window* owned_window_;
  bool owner_destroyed_;
  bool owned_window_destroyed_;
  aura::test::TestWindowDelegate child_window_delegate_;

  DISALLOW_COPY_AND_ASSIGN(DesktopAuraTopLevelWindowTest);
};

TEST_F(WidgetTest, DesktopAuraFullscreenWindowDestroyedBeforeOwnerTest) {
  ViewsDelegate::views_delegate = NULL;
  DesktopAuraTopLevelWindowTest fullscreen_window;
  ASSERT_NO_FATAL_FAILURE(fullscreen_window.CreateTopLevelWindow(
      gfx::Rect(0, 0, 200, 200), true));

  RunPendingMessages();
  ASSERT_NO_FATAL_FAILURE(fullscreen_window.DestroyOwnedWindow());
  RunPendingMessages();
}

TEST_F(WidgetTest, DesktopAuraFullscreenWindowOwnerDestroyed) {
  ViewsDelegate::views_delegate = NULL;

  DesktopAuraTopLevelWindowTest fullscreen_window;
  ASSERT_NO_FATAL_FAILURE(fullscreen_window.CreateTopLevelWindow(
      gfx::Rect(0, 0, 200, 200), true));

  RunPendingMessages();
  ASSERT_NO_FATAL_FAILURE(fullscreen_window.DestroyOwnerWindow());
  RunPendingMessages();
}

TEST_F(WidgetTest, DesktopAuraTopLevelOwnedPopupTest) {
  ViewsDelegate::views_delegate = NULL;
  DesktopAuraTopLevelWindowTest popup_window;
  ASSERT_NO_FATAL_FAILURE(popup_window.CreateTopLevelWindow(
      gfx::Rect(0, 0, 200, 200), false));

  RunPendingMessages();
  ASSERT_NO_FATAL_FAILURE(popup_window.DestroyOwnedWindow());
  RunPendingMessages();
}

// This test validates that when a top level owned popup Aura window is
// resized, the widget is resized as well.
TEST_F(WidgetTest, DesktopAuraTopLevelOwnedPopupResizeTest) {
  ViewsDelegate::views_delegate = NULL;
  DesktopAuraTopLevelWindowTest popup_window;
  ASSERT_NO_FATAL_FAILURE(popup_window.CreateTopLevelWindow(
      gfx::Rect(0, 0, 200, 200), false));

  gfx::Rect new_size(0, 0, 400, 400);
  popup_window.owned_window()->SetBounds(new_size);

  EXPECT_EQ(popup_window.top_level_widget()->GetNativeView()->bounds().size(),
            new_size.size());
  RunPendingMessages();
  ASSERT_NO_FATAL_FAILURE(popup_window.DestroyOwnedWindow());
  RunPendingMessages();
}

// Test to ensure that the aura Window's visiblity state is set to visible if
// the underlying widget is hidden and then shown.
TEST_F(WidgetTest, TestWindowVisibilityAfterHide) {
  // Create a widget.
  Widget widget;
  Widget::InitParams init_params =
      CreateParams(Widget::InitParams::TYPE_WINDOW);
  init_params.show_state = ui::SHOW_STATE_NORMAL;
  gfx::Rect initial_bounds(0, 0, 300, 400);
  init_params.bounds = initial_bounds;
  init_params.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  init_params.native_widget = new DesktopNativeWidgetAura(&widget);
  widget.Init(init_params);
  NonClientView* non_client_view = widget.non_client_view();
  NonClientFrameView* frame_view = new MinimumSizeFrameView(&widget);
  non_client_view->SetFrameView(frame_view);

  widget.Hide();
  EXPECT_FALSE(widget.GetNativeView()->IsVisible());
  widget.Show();
  EXPECT_TRUE(widget.GetNativeView()->IsVisible());
}

// The following code verifies we can correctly destroy a Widget from a mouse
// enter/exit. We could test move/drag/enter/exit but in general we don't run
// nested message loops from such events, nor has the code ever really dealt
// with this situation.

// Generates two moves (first generates enter, second real move), a press, drag
// and release stopping at |last_event_type|.
void GenerateMouseEvents(Widget* widget, ui::EventType last_event_type) {
  const gfx::Rect screen_bounds(widget->GetWindowBoundsInScreen());
  ui::MouseEvent move_event(ui::ET_MOUSE_MOVED, screen_bounds.CenterPoint(),
                            screen_bounds.CenterPoint(), 0, 0);
  ui::EventProcessor* dispatcher =
      widget->GetNativeWindow()->GetHost()->event_processor();
  ui::EventDispatchDetails details = dispatcher->OnEventFromSource(&move_event);
  if (last_event_type == ui::ET_MOUSE_ENTERED || details.dispatcher_destroyed)
    return;
  details = dispatcher->OnEventFromSource(&move_event);
  if (last_event_type == ui::ET_MOUSE_MOVED || details.dispatcher_destroyed)
    return;

  ui::MouseEvent press_event(ui::ET_MOUSE_PRESSED, screen_bounds.CenterPoint(),
                             screen_bounds.CenterPoint(), 0, 0);
  details = dispatcher->OnEventFromSource(&press_event);
  if (last_event_type == ui::ET_MOUSE_PRESSED || details.dispatcher_destroyed)
    return;

  gfx::Point end_point(screen_bounds.CenterPoint());
  end_point.Offset(1, 1);
  ui::MouseEvent drag_event(ui::ET_MOUSE_DRAGGED, end_point, end_point, 0, 0);
  details = dispatcher->OnEventFromSource(&drag_event);
  if (last_event_type == ui::ET_MOUSE_DRAGGED || details.dispatcher_destroyed)
    return;

  ui::MouseEvent release_event(ui::ET_MOUSE_RELEASED, end_point, end_point, 0,
                               0);
  details = dispatcher->OnEventFromSource(&release_event);
  if (details.dispatcher_destroyed)
    return;
}

// Creates a widget and invokes GenerateMouseEvents() with |last_event_type|.
void RunCloseWidgetDuringDispatchTest(WidgetTest* test,
                                      ui::EventType last_event_type) {
  // |widget| is deleted by CloseWidgetView.
  Widget* widget = new Widget;
  Widget::InitParams params =
      test->CreateParams(Widget::InitParams::TYPE_POPUP);
  params.native_widget = new DesktopNativeWidgetAura(widget);
  params.bounds = gfx::Rect(0, 0, 50, 100);
  widget->Init(params);
  widget->SetContentsView(new CloseWidgetView(last_event_type));
  widget->Show();
  GenerateMouseEvents(widget, last_event_type);
}

// Verifies deleting the widget from a mouse pressed event doesn't crash.
TEST_F(WidgetTest, CloseWidgetDuringMousePress) {
  RunCloseWidgetDuringDispatchTest(this, ui::ET_MOUSE_PRESSED);
}

// Verifies deleting the widget from a mouse released event doesn't crash.
TEST_F(WidgetTest, CloseWidgetDuringMouseReleased) {
  RunCloseWidgetDuringDispatchTest(this, ui::ET_MOUSE_RELEASED);
}

#endif  // !defined(OS_CHROMEOS)

// Tests that wheel events generated from scroll events are targetted to the
// views under the cursor when the focused view does not processed them.
TEST_F(WidgetTest, WheelEventsFromScrollEventTarget) {
  EventCountView* cursor_view = new EventCountView;
  cursor_view->SetBounds(60, 0, 50, 40);

  Widget* widget = CreateTopLevelPlatformWidget();
  widget->GetRootView()->AddChildView(cursor_view);

  // Generate a scroll event on the cursor view.
  ui::ScrollEvent scroll(ui::ET_SCROLL,
                         gfx::Point(65, 5),
                         ui::EventTimeForNow(),
                         0,
                         0, 20,
                         0, 20,
                         2);
  widget->OnScrollEvent(&scroll);

  EXPECT_EQ(1, cursor_view->GetEventCount(ui::ET_SCROLL));
  EXPECT_EQ(1, cursor_view->GetEventCount(ui::ET_MOUSEWHEEL));

  cursor_view->ResetCounts();

  ui::ScrollEvent scroll2(ui::ET_SCROLL,
                          gfx::Point(5, 5),
                          ui::EventTimeForNow(),
                          0,
                          0, 20,
                          0, 20,
                          2);
  widget->OnScrollEvent(&scroll2);

  EXPECT_EQ(0, cursor_view->GetEventCount(ui::ET_SCROLL));
  EXPECT_EQ(0, cursor_view->GetEventCount(ui::ET_MOUSEWHEEL));

  widget->CloseNow();
}

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

// Tests that event-handlers installed on the RootView get triggered correctly.
TEST_F(WidgetTest, EventHandlersOnRootView) {
  Widget* widget = CreateTopLevelNativeWidget();
  View* root_view = widget->GetRootView();

  EventCountView* view = new EventCountView;
  view->SetBounds(0, 0, 20, 20);
  root_view->AddChildView(view);

  EventCountHandler h1;
  root_view->AddPreTargetHandler(&h1);

  EventCountHandler h2;
  root_view->AddPostTargetHandler(&h2);

  widget->SetBounds(gfx::Rect(0, 0, 100, 100));
  widget->Show();

  ui::TouchEvent pressed(ui::ET_TOUCH_PRESSED,
                         gfx::Point(10, 10),
                         0, 0,
                         ui::EventTimeForNow(),
                         1.0, 0.0, 1.0, 0.0);
  widget->OnTouchEvent(&pressed);
  EXPECT_EQ(1, h1.GetEventCount(ui::ET_TOUCH_PRESSED));
  EXPECT_EQ(1, view->GetEventCount(ui::ET_TOUCH_PRESSED));
  EXPECT_EQ(1, h2.GetEventCount(ui::ET_TOUCH_PRESSED));

  ui::GestureEvent begin(ui::ET_GESTURE_BEGIN,
      5, 5, 0, ui::EventTimeForNow(),
      ui::GestureEventDetails(ui::ET_GESTURE_BEGIN, 0, 0), 1);
  ui::GestureEvent end(ui::ET_GESTURE_END,
      5, 5, 0, ui::EventTimeForNow(),
      ui::GestureEventDetails(ui::ET_GESTURE_END, 0, 0), 1);
  widget->OnGestureEvent(&begin);
  EXPECT_EQ(1, h1.GetEventCount(ui::ET_GESTURE_BEGIN));
  EXPECT_EQ(1, view->GetEventCount(ui::ET_GESTURE_BEGIN));
  EXPECT_EQ(1, h2.GetEventCount(ui::ET_GESTURE_BEGIN));

  ui::TouchEvent released(ui::ET_TOUCH_RELEASED,
                          gfx::Point(10, 10),
                          0, 0,
                          ui::EventTimeForNow(),
                          1.0, 0.0, 1.0, 0.0);
  widget->OnTouchEvent(&released);
  EXPECT_EQ(1, h1.GetEventCount(ui::ET_TOUCH_RELEASED));
  EXPECT_EQ(1, view->GetEventCount(ui::ET_TOUCH_RELEASED));
  EXPECT_EQ(1, h2.GetEventCount(ui::ET_TOUCH_RELEASED));

  widget->OnGestureEvent(&end);
  EXPECT_EQ(1, h1.GetEventCount(ui::ET_GESTURE_END));
  EXPECT_EQ(1, view->GetEventCount(ui::ET_GESTURE_END));
  EXPECT_EQ(1, h2.GetEventCount(ui::ET_GESTURE_END));

  ui::ScrollEvent scroll(ui::ET_SCROLL,
                         gfx::Point(5, 5),
                         ui::EventTimeForNow(),
                         0,
                         0, 20,
                         0, 20,
                         2);
  widget->OnScrollEvent(&scroll);
  EXPECT_EQ(1, h1.GetEventCount(ui::ET_SCROLL));
  EXPECT_EQ(1, view->GetEventCount(ui::ET_SCROLL));
  EXPECT_EQ(1, h2.GetEventCount(ui::ET_SCROLL));

  widget->CloseNow();
}

TEST_F(WidgetTest, SynthesizeMouseMoveEvent) {
  Widget* widget = CreateTopLevelNativeWidget();
  View* root_view = widget->GetRootView();

  EventCountView* v1 = new EventCountView();
  v1->SetBounds(0, 0, 10, 10);
  root_view->AddChildView(v1);
  EventCountView* v2 = new EventCountView();
  v2->SetBounds(0, 10, 10, 10);
  root_view->AddChildView(v2);

  gfx::Point cursor_location(5, 5);
  ui::MouseEvent move(ui::ET_MOUSE_MOVED, cursor_location, cursor_location,
                      ui::EF_NONE, ui::EF_NONE);
  widget->OnMouseEvent(&move);

  EXPECT_EQ(1, v1->GetEventCount(ui::ET_MOUSE_ENTERED));
  EXPECT_EQ(0, v2->GetEventCount(ui::ET_MOUSE_ENTERED));

  delete v1;
  v2->SetBounds(0, 0, 10, 10);
  EXPECT_EQ(0, v2->GetEventCount(ui::ET_MOUSE_ENTERED));

  widget->SynthesizeMouseMoveEvent();
  EXPECT_EQ(1, v2->GetEventCount(ui::ET_MOUSE_ENTERED));
}

// Used by SingleWindowClosing to count number of times WindowClosing() has
// been invoked.
class ClosingDelegate : public WidgetDelegate {
 public:
  ClosingDelegate() : count_(0), widget_(NULL) {}

  int count() const { return count_; }

  void set_widget(views::Widget* widget) { widget_ = widget; }

  // WidgetDelegate overrides:
  virtual Widget* GetWidget() OVERRIDE { return widget_; }
  virtual const Widget* GetWidget() const OVERRIDE { return widget_; }
  virtual void WindowClosing() OVERRIDE {
    count_++;
  }

 private:
  int count_;
  views::Widget* widget_;

  DISALLOW_COPY_AND_ASSIGN(ClosingDelegate);
};

// Verifies WindowClosing() is invoked correctly on the delegate when a Widget
// is closed.
TEST_F(WidgetTest, SingleWindowClosing) {
  scoped_ptr<ClosingDelegate> delegate(new ClosingDelegate());
  Widget* widget = new Widget();  // Destroyed by CloseNow() below.
  Widget::InitParams init_params =
      CreateParams(Widget::InitParams::TYPE_WINDOW);
  init_params.bounds = gfx::Rect(0, 0, 200, 200);
  init_params.delegate = delegate.get();
#if !defined(OS_CHROMEOS)
  init_params.native_widget = new DesktopNativeWidgetAura(widget);
#endif
  widget->Init(init_params);
  EXPECT_EQ(0, delegate->count());
  widget->CloseNow();
  EXPECT_EQ(1, delegate->count());
}

class WidgetWindowTitleTest : public WidgetTest {
 protected:
  void RunTest(bool desktop_native_widget) {
    Widget* widget = new Widget();  // Destroyed by CloseNow() below.
    Widget::InitParams init_params =
        CreateParams(Widget::InitParams::TYPE_WINDOW);
    widget->Init(init_params);

#if !defined(OS_CHROMEOS)
    if (desktop_native_widget)
      init_params.native_widget = new DesktopNativeWidgetAura(widget);
#else
    DCHECK(!desktop_native_widget)
        << "DesktopNativeWidget does not exist on non-Aura or on ChromeOS.";
#endif

    internal::NativeWidgetPrivate* native_widget =
        widget->native_widget_private();

    base::string16 empty;
    base::string16 s1(base::UTF8ToUTF16("Title1"));
    base::string16 s2(base::UTF8ToUTF16("Title2"));
    base::string16 s3(base::UTF8ToUTF16("TitleLong"));

    // The widget starts with no title, setting empty should not change
    // anything.
    EXPECT_FALSE(native_widget->SetWindowTitle(empty));
    // Setting the title to something non-empty should cause a change.
    EXPECT_TRUE(native_widget->SetWindowTitle(s1));
    // Setting the title to something else with the same length should cause a
    // change.
    EXPECT_TRUE(native_widget->SetWindowTitle(s2));
    // Setting the title to something else with a different length should cause
    // a change.
    EXPECT_TRUE(native_widget->SetWindowTitle(s3));
    // Setting the title to the same thing twice should not cause a change.
    EXPECT_FALSE(native_widget->SetWindowTitle(s3));

    widget->CloseNow();
  }
};

TEST_F(WidgetWindowTitleTest, SetWindowTitleChanged_NativeWidget) {
  // Use the default NativeWidget.
  bool desktop_native_widget = false;
  RunTest(desktop_native_widget);
}

// DesktopNativeWidget does not exist on non-Aura or on ChromeOS.
#if !defined(OS_CHROMEOS)
TEST_F(WidgetWindowTitleTest, SetWindowTitleChanged_DesktopNativeWidget) {
  // Override to use a DesktopNativeWidget.
  bool desktop_native_widget = true;
  RunTest(desktop_native_widget);
}
#endif  // !OS_CHROMEOS

// Used by SetTopLevelCorrectly to track calls to OnBeforeWidgetInit().
class VerifyTopLevelDelegate : public TestViewsDelegate {
 public:
  VerifyTopLevelDelegate()
      : on_before_init_called_(false),
        is_top_level_(false) {
  }

  bool on_before_init_called() const { return on_before_init_called_; }
  bool is_top_level() const { return is_top_level_; }

  virtual void OnBeforeWidgetInit(
      Widget::InitParams* params,
      internal::NativeWidgetDelegate* delegate) OVERRIDE {
    on_before_init_called_ = true;
    is_top_level_ = params->top_level;
  }

 private:
  bool on_before_init_called_;
  bool is_top_level_;

  DISALLOW_COPY_AND_ASSIGN(VerifyTopLevelDelegate);
};

// Verifies |top_level| is correctly passed to
// ViewsDelegate::OnBeforeWidgetInit().
TEST_F(WidgetTest, SetTopLevelCorrectly) {
  set_views_delegate(NULL);
  VerifyTopLevelDelegate* delegate = new VerifyTopLevelDelegate;
  set_views_delegate(delegate);  // ViewsTestBase takes ownership.
  scoped_ptr<Widget> widget(new Widget);
  Widget::InitParams params =
      CreateParams(views::Widget::InitParams::TYPE_POPUP);
  params.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  widget->Init(params);
  EXPECT_TRUE(delegate->on_before_init_called());
  EXPECT_TRUE(delegate->is_top_level());
}

TEST_F(WidgetTest, WidgetDeleted_InOnMousePressed) {
  Widget* widget = new Widget;
  Widget::InitParams params =
      CreateParams(views::Widget::InitParams::TYPE_POPUP);
  widget->Init(params);

  widget->SetContentsView(new CloseWidgetView(ui::ET_MOUSE_PRESSED));

  widget->SetSize(gfx::Size(100, 100));
  widget->Show();

  aura::test::EventGenerator generator(GetContext(), widget->GetNativeWindow());

  WidgetDeletionObserver deletion_observer(widget);
  generator.ClickLeftButton();
  EXPECT_FALSE(deletion_observer.IsWidgetAlive());

  // Yay we did not crash!
}

TEST_F(WidgetTest, WidgetDeleted_InDispatchGestureEvent) {
  Widget* widget = new Widget;
  Widget::InitParams params =
      CreateParams(views::Widget::InitParams::TYPE_POPUP);
  widget->Init(params);

  widget->SetContentsView(new CloseWidgetView(ui::ET_GESTURE_TAP_DOWN));

  widget->SetSize(gfx::Size(100, 100));
  widget->Show();

  aura::test::EventGenerator generator(GetContext());

  WidgetDeletionObserver deletion_observer(widget);
  generator.GestureTapAt(widget->GetWindowBoundsInScreen().CenterPoint());
  EXPECT_FALSE(deletion_observer.IsWidgetAlive());

  // Yay we did not crash!
}

// See description of RunGetNativeThemeFromDestructor() for details.
class GetNativeThemeFromDestructorView : public WidgetDelegateView {
 public:
  GetNativeThemeFromDestructorView() {}
  virtual ~GetNativeThemeFromDestructorView() {
    VerifyNativeTheme();
  }

  virtual View* GetContentsView() OVERRIDE {
    return this;
  }

 private:
  void VerifyNativeTheme() {
    ASSERT_TRUE(GetNativeTheme() != NULL);
  }

  DISALLOW_COPY_AND_ASSIGN(GetNativeThemeFromDestructorView);
};

// Verifies GetNativeTheme() from the destructor of a WidgetDelegateView doesn't
// crash. |is_first_run| is true if this is the first call. A return value of
// true indicates this should be run again with a value of false.
// First run uses DesktopNativeWidgetAura (if possible). Second run doesn't.
bool RunGetNativeThemeFromDestructor(const Widget::InitParams& in_params,
                                     bool is_first_run) {
  bool needs_second_run = false;
  // Destroyed by CloseNow() below.
  Widget* widget = new Widget;
  Widget::InitParams params(in_params);
  // Deletes itself when the Widget is destroyed.
  params.delegate = new GetNativeThemeFromDestructorView;
#if !defined(OS_CHROMEOS)
  if (is_first_run) {
    params.native_widget = new DesktopNativeWidgetAura(widget);
    needs_second_run = true;
  }
#endif
  widget->Init(params);
  widget->CloseNow();
  return needs_second_run;
}

// See description of RunGetNativeThemeFromDestructor() for details.
TEST_F(WidgetTest, GetNativeThemeFromDestructor) {
  Widget::InitParams params = CreateParams(Widget::InitParams::TYPE_POPUP);
  if (RunGetNativeThemeFromDestructor(params, true))
    RunGetNativeThemeFromDestructor(params, false);
}

// Used by HideCloseDestroy. Allows setting a boolean when the widget is
// destroyed.
class CloseDestroysWidget : public Widget {
 public:
  explicit CloseDestroysWidget(bool* destroyed)
      : destroyed_(destroyed) {
  }

  virtual ~CloseDestroysWidget() {
    if (destroyed_) {
      *destroyed_ = true;
      base::MessageLoop::current()->QuitNow();
    }
  }

  void Detach() { destroyed_ = NULL; }

 private:
  // If non-null set to true from destructor.
  bool* destroyed_;

  DISALLOW_COPY_AND_ASSIGN(CloseDestroysWidget);
};

// Verifies Close() results in destroying.
TEST_F(WidgetTest, CloseDestroys) {
  bool destroyed = false;
  CloseDestroysWidget* widget = new CloseDestroysWidget(&destroyed);
  Widget::InitParams params =
      CreateParams(views::Widget::InitParams::TYPE_MENU);
  params.opacity = Widget::InitParams::OPAQUE_WINDOW;
#if !defined(OS_CHROMEOS)
  params.native_widget = new DesktopNativeWidgetAura(widget);
#endif
  widget->Init(params);
  widget->Show();
  widget->Hide();
  widget->Close();
  // Run the message loop as Close() asynchronously deletes.
  RunPendingMessages();
  EXPECT_TRUE(destroyed);
  // Close() should destroy the widget. If not we'll cleanup to avoid leaks.
  if (!destroyed) {
    widget->Detach();
    widget->CloseNow();
  }
}

// A view that consumes mouse-pressed event and gesture-tap-down events.
class RootViewTestView : public View {
 public:
  RootViewTestView(): View() {}

 private:
  virtual bool OnMousePressed(const ui::MouseEvent& event) OVERRIDE {
    return true;
  }

  virtual void OnGestureEvent(ui::GestureEvent* event) OVERRIDE {
    if (event->type() == ui::ET_GESTURE_TAP_DOWN)
      event->SetHandled();
  }
};

// Checks if RootView::*_handler_ fields are unset when widget is hidden.
// Fails on chromium.webkit Windows bot, see crbug.com/264872.
#if defined(OS_WIN)
#define MAYBE_DisableTestRootViewHandlersWhenHidden\
    DISABLED_TestRootViewHandlersWhenHidden
#else
#define MAYBE_DisableTestRootViewHandlersWhenHidden\
    TestRootViewHandlersWhenHidden
#endif
TEST_F(WidgetTest, MAYBE_DisableTestRootViewHandlersWhenHidden) {
  Widget* widget = CreateTopLevelNativeWidget();
  widget->SetBounds(gfx::Rect(0, 0, 300, 300));
  View* view = new RootViewTestView();
  view->SetBounds(0, 0, 300, 300);
  internal::RootView* root_view =
      static_cast<internal::RootView*>(widget->GetRootView());
  root_view->AddChildView(view);

  // Check RootView::mouse_pressed_handler_.
  widget->Show();
  EXPECT_EQ(NULL, GetMousePressedHandler(root_view));
  gfx::Point click_location(45, 15);
  ui::MouseEvent press(ui::ET_MOUSE_PRESSED, click_location, click_location,
                       ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON);
  widget->OnMouseEvent(&press);
  EXPECT_EQ(view, GetMousePressedHandler(root_view));
  widget->Hide();
  EXPECT_EQ(NULL, GetMousePressedHandler(root_view));

  // Check RootView::mouse_move_handler_.
  widget->Show();
  EXPECT_EQ(NULL, GetMouseMoveHandler(root_view));
  gfx::Point move_location(45, 15);
  ui::MouseEvent move(ui::ET_MOUSE_MOVED, move_location, move_location, 0, 0);
  widget->OnMouseEvent(&move);
  EXPECT_EQ(view, GetMouseMoveHandler(root_view));
  widget->Hide();
  EXPECT_EQ(NULL, GetMouseMoveHandler(root_view));

  // Check RootView::gesture_handler_.
  widget->Show();
  EXPECT_EQ(NULL, GetGestureHandler(root_view));
  ui::GestureEvent tap_down(
      ui::ET_GESTURE_TAP_DOWN,
      15,
      15,
      0,
      base::TimeDelta(),
      ui::GestureEventDetails(ui::ET_GESTURE_TAP_DOWN, 0, 0),
      1);
  widget->OnGestureEvent(&tap_down);
  EXPECT_EQ(view, GetGestureHandler(root_view));
  widget->Hide();
  EXPECT_EQ(NULL, GetGestureHandler(root_view));

  widget->Close();
}

class GestureEndConsumerView : public View {
 private:
  virtual void OnGestureEvent(ui::GestureEvent* event) OVERRIDE {
    if (event->type() == ui::ET_GESTURE_END)
      event->SetHandled();
  }
};

TEST_F(WidgetTest, GestureHandlerNotSetOnGestureEnd) {
  Widget* widget = CreateTopLevelNativeWidget();
  widget->SetBounds(gfx::Rect(0, 0, 300, 300));
  View* view = new GestureEndConsumerView();
  view->SetBounds(0, 0, 300, 300);
  internal::RootView* root_view =
      static_cast<internal::RootView*>(widget->GetRootView());
  root_view->AddChildView(view);

  widget->Show();
  EXPECT_EQ(NULL, GetGestureHandler(root_view));
  ui::GestureEvent end(ui::ET_GESTURE_END, 15, 15, 0, base::TimeDelta(),
                       ui::GestureEventDetails(ui::ET_GESTURE_END, 0, 0), 1);
  widget->OnGestureEvent(&end);
  EXPECT_EQ(NULL, GetGestureHandler(root_view));

  widget->Close();
}

// Test the result of Widget::GetAllChildWidgets().
TEST_F(WidgetTest, GetAllChildWidgets) {
  // Create the following widget hierarchy:
  //
  // toplevel
  // +-- w1
  //     +-- w11
  // +-- w2
  //     +-- w21
  //     +-- w22
  Widget* toplevel = CreateTopLevelPlatformWidget();
  Widget* w1 = CreateChildPlatformWidget(toplevel->GetNativeView());
  Widget* w11 = CreateChildPlatformWidget(w1->GetNativeView());
  Widget* w2 = CreateChildPlatformWidget(toplevel->GetNativeView());
  Widget* w21 = CreateChildPlatformWidget(w2->GetNativeView());
  Widget* w22 = CreateChildPlatformWidget(w2->GetNativeView());

  std::set<Widget*> expected;
  expected.insert(toplevel);
  expected.insert(w1);
  expected.insert(w11);
  expected.insert(w2);
  expected.insert(w21);
  expected.insert(w22);

  std::set<Widget*> widgets;
  Widget::GetAllChildWidgets(toplevel->GetNativeView(), &widgets);

  EXPECT_EQ(expected.size(), widgets.size());
  EXPECT_TRUE(std::equal(expected.begin(), expected.end(), widgets.begin()));
}

// Used by DestroyChildWidgetsInOrder. On destruction adds the supplied name to
// a vector.
class DestroyedTrackingView : public View {
 public:
  DestroyedTrackingView(const std::string& name,
                        std::vector<std::string>* add_to)
      : name_(name),
        add_to_(add_to) {
  }

  virtual ~DestroyedTrackingView() {
    add_to_->push_back(name_);
  }

 private:
  const std::string name_;
  std::vector<std::string>* add_to_;

  DISALLOW_COPY_AND_ASSIGN(DestroyedTrackingView);
};

class WidgetChildDestructionTest : public WidgetTest {
 public:
  WidgetChildDestructionTest() {}

  // Creates a top level and a child, destroys the child and verifies the views
  // of the child are destroyed before the views of the parent.
  void RunDestroyChildWidgetsTest(bool top_level_has_desktop_native_widget_aura,
                                  bool child_has_desktop_native_widget_aura) {
    // When a View is destroyed its name is added here.
    std::vector<std::string> destroyed;

    Widget* top_level = new Widget;
    Widget::InitParams params =
        CreateParams(views::Widget::InitParams::TYPE_WINDOW);
#if !defined(OS_CHROMEOS)
    if (top_level_has_desktop_native_widget_aura)
      params.native_widget = new DesktopNativeWidgetAura(top_level);
#endif
    top_level->Init(params);
    top_level->GetRootView()->AddChildView(
        new DestroyedTrackingView("parent", &destroyed));
    top_level->Show();

    Widget* child = new Widget;
    Widget::InitParams child_params =
        CreateParams(views::Widget::InitParams::TYPE_POPUP);
    child_params.parent = top_level->GetNativeView();
#if !defined(OS_CHROMEOS)
    if (child_has_desktop_native_widget_aura)
      child_params.native_widget = new DesktopNativeWidgetAura(child);
#endif
    child->Init(child_params);
    child->GetRootView()->AddChildView(
        new DestroyedTrackingView("child", &destroyed));
    child->Show();

    // Should trigger destruction of the child too.
    top_level->native_widget_private()->CloseNow();

    // Child should be destroyed first.
    ASSERT_EQ(2u, destroyed.size());
    EXPECT_EQ("child", destroyed[0]);
    EXPECT_EQ("parent", destroyed[1]);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(WidgetChildDestructionTest);
};

#if !defined(OS_CHROMEOS)
// See description of RunDestroyChildWidgetsTest(). Parent uses
// DesktopNativeWidgetAura.
TEST_F(WidgetChildDestructionTest,
       DestroyChildWidgetsInOrderWithDesktopNativeWidget) {
  RunDestroyChildWidgetsTest(true, false);
}

// See description of RunDestroyChildWidgetsTest(). Both parent and child use
// DesktopNativeWidgetAura.
TEST_F(WidgetChildDestructionTest,
       DestroyChildWidgetsInOrderWithDesktopNativeWidgetForBoth) {
  RunDestroyChildWidgetsTest(true, true);
}
#endif

// See description of RunDestroyChildWidgetsTest().
TEST_F(WidgetChildDestructionTest, DestroyChildWidgetsInOrder) {
  RunDestroyChildWidgetsTest(false, false);
}

#if !defined(OS_CHROMEOS)
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

// This test verifies that whether mouse events when a modal dialog is
// displayed are eaten or recieved by the dialog.
TEST_F(WidgetTest, WindowMouseModalityTest) {
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
  EXPECT_TRUE(top_level_widget.IsVisible());

  // Create a view and validate that a mouse moves makes it to the view.
  EventCountView* widget_view = new EventCountView();
  widget_view->SetBounds(0, 0, 10, 10);
  top_level_widget.GetRootView()->AddChildView(widget_view);

  gfx::Point cursor_location_main(5, 5);
  ui::MouseEvent move_main(ui::ET_MOUSE_MOVED,
                           cursor_location_main,
                           cursor_location_main,
                           ui::EF_NONE,
                           ui::EF_NONE);
  ui::EventDispatchDetails details = top_level_widget.GetNativeView()->
      GetHost()->event_processor()->OnEventFromSource(&move_main);
  ASSERT_FALSE(details.dispatcher_destroyed);

  EXPECT_EQ(1, widget_view->GetEventCount(ui::ET_MOUSE_ENTERED));
  widget_view->ResetCounts();

  // Create a modal dialog and validate that a mouse down message makes it to
  // the main view within the dialog.

  // This instance will be destroyed when the dialog is destroyed.
  ModalDialogDelegate* dialog_delegate = new ModalDialogDelegate;

  Widget* modal_dialog_widget = views::DialogDelegate::CreateDialogWidget(
      dialog_delegate, NULL, top_level_widget.GetNativeWindow());
  modal_dialog_widget->SetBounds(gfx::Rect(100, 100, 200, 200));
  EventCountView* dialog_widget_view = new EventCountView();
  dialog_widget_view->SetBounds(0, 0, 50, 50);
  modal_dialog_widget->GetRootView()->AddChildView(dialog_widget_view);
  modal_dialog_widget->Show();
  EXPECT_TRUE(modal_dialog_widget->IsVisible());

  gfx::Point cursor_location_dialog(100, 100);
  ui::MouseEvent mouse_down_dialog(ui::ET_MOUSE_PRESSED,
                                   cursor_location_dialog,
                                   cursor_location_dialog,
                                   ui::EF_NONE,
                                   ui::EF_NONE);
  details = top_level_widget.GetNativeView()->GetHost()->event_processor()->
      OnEventFromSource(&mouse_down_dialog);
  ASSERT_FALSE(details.dispatcher_destroyed);
  EXPECT_EQ(1, dialog_widget_view->GetEventCount(ui::ET_MOUSE_PRESSED));

  // Send a mouse move message to the main window. It should not be received by
  // the main window as the modal dialog is still active.
  gfx::Point cursor_location_main2(6, 6);
  ui::MouseEvent mouse_down_main(ui::ET_MOUSE_MOVED,
                                 cursor_location_main2,
                                 cursor_location_main2,
                                 ui::EF_NONE,
                                 ui::EF_NONE);
  details = top_level_widget.GetNativeView()->GetHost()->event_processor()->
      OnEventFromSource(&mouse_down_main);
  ASSERT_FALSE(details.dispatcher_destroyed);
  EXPECT_EQ(0, widget_view->GetEventCount(ui::ET_MOUSE_MOVED));

  modal_dialog_widget->CloseNow();
  top_level_widget.CloseNow();
}

// Verifies nativeview visbility matches that of Widget visibility when
// SetFullscreen is invoked.
TEST_F(WidgetTest, FullscreenStatePropagated) {
  Widget::InitParams init_params =
      CreateParams(Widget::InitParams::TYPE_WINDOW);
  init_params.show_state = ui::SHOW_STATE_NORMAL;
  init_params.bounds = gfx::Rect(0, 0, 500, 500);
  init_params.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;

  {
    Widget top_level_widget;
    top_level_widget.Init(init_params);
    top_level_widget.SetFullscreen(true);
    EXPECT_EQ(top_level_widget.IsVisible(),
              top_level_widget.GetNativeView()->IsVisible());
    top_level_widget.CloseNow();
  }

#if !defined(OS_CHROMEOS)
  {
    Widget top_level_widget;
    init_params.native_widget = new DesktopNativeWidgetAura(&top_level_widget);
    top_level_widget.Init(init_params);
    top_level_widget.SetFullscreen(true);
    EXPECT_EQ(top_level_widget.IsVisible(),
              top_level_widget.GetNativeView()->IsVisible());
    top_level_widget.CloseNow();
  }
#endif
}

#if defined(OS_WIN)

// Provides functionality to test widget activation via an activation flag
// which can be set by an accessor.
class ModalWindowTestWidgetDelegate : public WidgetDelegate {
 public:
  ModalWindowTestWidgetDelegate()
      : widget_(NULL),
        can_activate_(true) {}

  virtual ~ModalWindowTestWidgetDelegate() {}

  // Overridden from WidgetDelegate:
  virtual void DeleteDelegate() OVERRIDE {
    delete this;
  }
  virtual Widget* GetWidget() OVERRIDE {
    return widget_;
  }
  virtual const Widget* GetWidget() const OVERRIDE {
    return widget_;
  }
  virtual bool CanActivate() const OVERRIDE {
    return can_activate_;
  }
  virtual bool ShouldAdvanceFocusToTopLevelWidget() const OVERRIDE {
    return true;
  }

  void set_can_activate(bool can_activate) {
    can_activate_ = can_activate;
  }

  void set_widget(Widget* widget) {
    widget_ = widget;
  }

 private:
  Widget* widget_;
  bool can_activate_;

  DISALLOW_COPY_AND_ASSIGN(ModalWindowTestWidgetDelegate);
};

// Tests whether we can activate the top level widget when a modal dialog is
// active.
TEST_F(WidgetTest, WindowModalityActivationTest) {
  // Destroyed when the top level widget created below is destroyed.
  ModalWindowTestWidgetDelegate* widget_delegate =
      new ModalWindowTestWidgetDelegate;
  // Create a top level widget.
  Widget top_level_widget;
  Widget::InitParams init_params =
      CreateParams(Widget::InitParams::TYPE_WINDOW);
  init_params.show_state = ui::SHOW_STATE_NORMAL;
  gfx::Rect initial_bounds(0, 0, 500, 500);
  init_params.bounds = initial_bounds;
  init_params.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  init_params.native_widget = new DesktopNativeWidgetAura(&top_level_widget);
  init_params.delegate = widget_delegate;
  top_level_widget.Init(init_params);
  widget_delegate->set_widget(&top_level_widget);
  top_level_widget.Show();
  EXPECT_TRUE(top_level_widget.IsVisible());

  HWND win32_window = views::HWNDForWidget(&top_level_widget);
  EXPECT_TRUE(::IsWindow(win32_window));

  // This instance will be destroyed when the dialog is destroyed.
  ModalDialogDelegate* dialog_delegate = new ModalDialogDelegate;

  // We should be able to activate the window even if the WidgetDelegate
  // says no, when a modal dialog is active.
  widget_delegate->set_can_activate(false);

  Widget* modal_dialog_widget = views::DialogDelegate::CreateDialogWidget(
      dialog_delegate, NULL, top_level_widget.GetNativeWindow());
  modal_dialog_widget->SetBounds(gfx::Rect(100, 100, 200, 200));
  modal_dialog_widget->Show();
  EXPECT_TRUE(modal_dialog_widget->IsVisible());

  LRESULT activate_result = ::SendMessage(
      win32_window,
      WM_MOUSEACTIVATE,
      reinterpret_cast<WPARAM>(win32_window),
      MAKELPARAM(WM_LBUTTONDOWN, HTCLIENT));
  EXPECT_EQ(activate_result, MA_ACTIVATE);

  modal_dialog_widget->CloseNow();
  top_level_widget.CloseNow();
}
#endif
#endif

TEST_F(WidgetTest, ShowCreatesActiveWindow) {
  Widget* widget = CreateTopLevelPlatformWidget();

  widget->Show();
  EXPECT_EQ(GetWidgetShowState(widget), ui::SHOW_STATE_NORMAL);

  widget->CloseNow();
}

TEST_F(WidgetTest, ShowInactive) {
  Widget* widget = CreateTopLevelPlatformWidget();

  widget->ShowInactive();
  EXPECT_EQ(GetWidgetShowState(widget), ui::SHOW_STATE_INACTIVE);

  widget->CloseNow();
}

TEST_F(WidgetTest, ShowInactiveAfterShow) {
  Widget* widget = CreateTopLevelPlatformWidget();

  widget->Show();
  widget->ShowInactive();
  EXPECT_EQ(GetWidgetShowState(widget), ui::SHOW_STATE_NORMAL);

  widget->CloseNow();
}

TEST_F(WidgetTest, ShowAfterShowInactive) {
  Widget* widget = CreateTopLevelPlatformWidget();

  widget->ShowInactive();
  widget->Show();
  EXPECT_EQ(GetWidgetShowState(widget), ui::SHOW_STATE_NORMAL);

  widget->CloseNow();
}

#if !defined(OS_CHROMEOS)
TEST_F(WidgetTest, InactiveWidgetDoesNotGrabActivation) {
  Widget* widget = CreateTopLevelPlatformWidget();
  widget->Show();
  EXPECT_EQ(GetWidgetShowState(widget), ui::SHOW_STATE_NORMAL);

  Widget widget2;
  Widget::InitParams params = CreateParams(Widget::InitParams::TYPE_POPUP);
  params.can_activate = false;
  params.native_widget = new DesktopNativeWidgetAura(&widget2);
  params.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  widget2.Init(params);
  widget2.Show();

  EXPECT_EQ(GetWidgetShowState(&widget2), ui::SHOW_STATE_INACTIVE);
  EXPECT_EQ(GetWidgetShowState(widget), ui::SHOW_STATE_NORMAL);

  widget->CloseNow();
  widget2.CloseNow();
}
#endif

namespace {

class FullscreenAwareFrame : public views::NonClientFrameView {
 public:
  explicit FullscreenAwareFrame(views::Widget* widget)
      : widget_(widget), fullscreen_layout_called_(false) {}
  virtual ~FullscreenAwareFrame() {}

  // views::NonClientFrameView overrides:
  virtual gfx::Rect GetBoundsForClientView() const OVERRIDE {
    return gfx::Rect();
  }
  virtual gfx::Rect GetWindowBoundsForClientBounds(
      const gfx::Rect& client_bounds) const OVERRIDE {
    return gfx::Rect();
  }
  virtual int NonClientHitTest(const gfx::Point& point) OVERRIDE {
    return HTNOWHERE;
  }
  virtual void GetWindowMask(const gfx::Size& size,
                             gfx::Path* window_mask) OVERRIDE {}
  virtual void ResetWindowControls() OVERRIDE {}
  virtual void UpdateWindowIcon() OVERRIDE {}
  virtual void UpdateWindowTitle() OVERRIDE {}

  // views::View overrides:
  virtual void Layout() OVERRIDE {
    if (widget_->IsFullscreen())
      fullscreen_layout_called_ = true;
  }

  bool fullscreen_layout_called() const { return fullscreen_layout_called_; }

 private:
  views::Widget* widget_;
  bool fullscreen_layout_called_;

  DISALLOW_COPY_AND_ASSIGN(FullscreenAwareFrame);
};

}  // namespace

// Tests that frame Layout is called when a widget goes fullscreen without
// changing its size or title.
TEST_F(WidgetTest, FullscreenFrameLayout) {
  Widget* widget = CreateTopLevelPlatformWidget();
  FullscreenAwareFrame* frame = new FullscreenAwareFrame(widget);
  widget->non_client_view()->SetFrameView(frame);  // Owns |frame|.

  widget->Maximize();
  RunPendingMessages();

  EXPECT_FALSE(frame->fullscreen_layout_called());
  widget->SetFullscreen(true);
  widget->Show();
  RunPendingMessages();
  EXPECT_TRUE(frame->fullscreen_layout_called());

  widget->CloseNow();
}

#if !defined(OS_CHROMEOS)
namespace {

// Trivial WidgetObserverTest that invokes Widget::IsActive() from
// OnWindowDestroying.
class IsActiveFromDestroyObserver : public WidgetObserver {
 public:
  IsActiveFromDestroyObserver() {}
  virtual ~IsActiveFromDestroyObserver() {}
  virtual void OnWidgetDestroying(Widget* widget) OVERRIDE {
    widget->IsActive();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(IsActiveFromDestroyObserver);
};

}  // namespace

// Verifies Widget::IsActive() invoked from
// WidgetObserver::OnWidgetDestroying() in a child widget doesn't crash.
TEST_F(WidgetTest, IsActiveFromDestroy) {
  // Create two widgets, one a child of the other.
  IsActiveFromDestroyObserver observer;
  Widget parent_widget;
  Widget::InitParams parent_params =
      CreateParams(Widget::InitParams::TYPE_POPUP);
  parent_params.native_widget = new DesktopNativeWidgetAura(&parent_widget);
  parent_params.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  parent_widget.Init(parent_params);
  parent_widget.Show();

  Widget child_widget;
  Widget::InitParams child_params =
      CreateParams(Widget::InitParams::TYPE_POPUP);
  child_params.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  child_params.context = parent_widget.GetNativeView();
  child_widget.Init(child_params);
  child_widget.AddObserver(&observer);
  child_widget.Show();

  parent_widget.CloseNow();
}
#endif

}  // namespace test
}  // namespace views
