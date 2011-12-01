// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/point.h"
#include "ui/views/test/test_views_delegate.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/views_delegate.h"
#include "ui/views/widget/native_widget_delegate.h"

#if defined(USE_AURA)
#include "ui/aura/window.h"
#include "ui/views/widget/native_widget_aura.h"
#elif defined(OS_WIN)
#include "ui/views/widget/native_widget_win.h"
#elif defined(TOOLKIT_USES_GTK)
#include "ui/views/widget/native_widget_gtk.h"
#endif

namespace views {
namespace {

// A generic typedef to pick up relevant NativeWidget implementations.
#if defined(USE_AURA)
typedef NativeWidgetAura NativeWidgetPlatform;
#elif defined(OS_WIN)
typedef NativeWidgetWin NativeWidgetPlatform;
#elif defined(TOOLKIT_USES_GTK)
typedef NativeWidgetGtk NativeWidgetPlatform;
#endif

// A widget that assumes mouse capture always works. It won't on Gtk/Aura in
// testing, so we mock it.
#if defined(TOOLKIT_USES_GTK) || defined(USE_AURA)
class NativeWidgetCapture : public NativeWidgetPlatform {
 public:
  NativeWidgetCapture(internal::NativeWidgetDelegate* delegate)
      : NativeWidgetPlatform(delegate),
        mouse_capture_(false) {}
  virtual ~NativeWidgetCapture() {}

  virtual void SetMouseCapture() OVERRIDE {
    mouse_capture_ = true;
  }
  virtual void ReleaseMouseCapture() OVERRIDE {
    if (mouse_capture_)
      delegate()->OnMouseCaptureLost();
    mouse_capture_ = false;
  }
  virtual bool HasMouseCapture() const OVERRIDE {
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
#elif defined(TOOLKIT_USES_GTK)
typedef NativeWidgetCapture NativeWidgetPlatformForTest;
#endif

// A view that always processes all mouse events.
class MouseView : public View {
 public:
  MouseView() : View() {
  }
  virtual ~MouseView() {}

  virtual bool OnMousePressed(const MouseEvent& event) OVERRIDE {
    return true;
  }
};

typedef ViewsTestBase WidgetTest;

NativeWidget* CreatePlatformNativeWidget(
    internal::NativeWidgetDelegate* delegate) {
  return new NativeWidgetPlatformForTest(delegate);
}

Widget* CreateTopLevelPlatformWidget() {
  Widget* toplevel = new Widget;
  Widget::InitParams toplevel_params(Widget::InitParams::TYPE_WINDOW);
  toplevel_params.native_widget = CreatePlatformNativeWidget(toplevel);
  toplevel->Init(toplevel_params);
  return toplevel;
}

Widget* CreateChildPlatformWidget(gfx::NativeView parent_native_view) {
  Widget* child = new Widget;
  Widget::InitParams child_params(Widget::InitParams::TYPE_CONTROL);
  child_params.native_widget = CreatePlatformNativeWidget(child);
  child_params.parent = parent_native_view;
  child->Init(child_params);
  child->SetContentsView(new View);
  return child;
}

#if defined(OS_WIN) && !defined(USE_AURA)
// On Windows, it is possible for us to have a child window that is TYPE_POPUP.
Widget* CreateChildPopupPlatformWidget(gfx::NativeView parent_native_view) {
  Widget* child = new Widget;
  Widget::InitParams child_params(Widget::InitParams::TYPE_POPUP);
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
  Widget::InitParams params(Widget::InitParams::TYPE_WINDOW);
  toplevel->Init(params);
  toplevel->SetContentsView(new View);
  return toplevel;
}

Widget* CreateChildNativeWidgetWithParent(Widget* parent) {
  Widget* child = new Widget;
  Widget::InitParams params(Widget::InitParams::TYPE_CONTROL);
  params.parent_widget = parent;
  child->Init(params);
  child->SetContentsView(new View);
  return child;
}

Widget* CreateChildNativeWidget() {
  return CreateChildNativeWidgetWithParent(NULL);
}

bool WidgetHasMouseCapture(const Widget* widget) {
  return static_cast<const internal::NativeWidgetPrivate*>(widget->
      native_widget())->HasMouseCapture();
}

////////////////////////////////////////////////////////////////////////////////
// Widget::GetTopLevelWidget tests.

TEST_F(WidgetTest, GetTopLevelWidget_Native) {
  // Create a hierarchy of native widgets.
  Widget* toplevel = CreateTopLevelPlatformWidget();
#if defined(TOOLKIT_USES_GTK)
  NativeWidgetGtk* native_widget =
      static_cast<NativeWidgetGtk*>(toplevel->native_widget());
  gfx::NativeView parent = native_widget->window_contents();
#else
  gfx::NativeView parent = toplevel->GetNativeView();
#endif
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
  MouseEvent pressed(ui::ET_MOUSE_PRESSED, 45, 45, ui::EF_LEFT_BUTTON_DOWN);
  toplevel->OnMouseEvent(pressed);

  EXPECT_TRUE(WidgetHasMouseCapture(toplevel));
  EXPECT_TRUE(WidgetHasMouseCapture(child1));
  EXPECT_FALSE(WidgetHasMouseCapture(child2));

  MouseEvent released(ui::ET_MOUSE_RELEASED, 45, 45, ui::EF_LEFT_BUTTON_DOWN);
  toplevel->OnMouseEvent(released);

  EXPECT_FALSE(WidgetHasMouseCapture(toplevel));
  EXPECT_FALSE(WidgetHasMouseCapture(child1));
  EXPECT_FALSE(WidgetHasMouseCapture(child2));

  RunPendingMessages();

  // Click on child2
  MouseEvent pressed2(ui::ET_MOUSE_PRESSED, 315, 45, ui::EF_LEFT_BUTTON_DOWN);
  EXPECT_TRUE(toplevel->OnMouseEvent(pressed2));
  EXPECT_TRUE(WidgetHasMouseCapture(toplevel));
  EXPECT_TRUE(WidgetHasMouseCapture(child2));
  EXPECT_FALSE(WidgetHasMouseCapture(child1));

  MouseEvent released2(ui::ET_MOUSE_RELEASED, 315, 45, ui::EF_LEFT_BUTTON_DOWN);
  toplevel->OnMouseEvent(released2);
  EXPECT_FALSE(WidgetHasMouseCapture(toplevel));
  EXPECT_FALSE(WidgetHasMouseCapture(child1));
  EXPECT_FALSE(WidgetHasMouseCapture(child2));

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
#if defined(TOOLKIT_USES_GTK)
  NativeWidgetGtk* native_widget =
      static_cast<NativeWidgetGtk*>(toplevel->native_widget());
  gfx::NativeView parent = native_widget->window_contents();
#else
  gfx::NativeView parent = toplevel->GetNativeView();
#endif
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
  OwnershipTestWidget(OwnershipTestState* state) : state_(state) {}
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
  Widget::InitParams params(Widget::InitParams::TYPE_POPUP);
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
  Widget::InitParams params(Widget::InitParams::TYPE_POPUP);
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
  Widget::InitParams params(Widget::InitParams::TYPE_POPUP);
  params.native_widget =
      new OwnershipTestNativeWidgetPlatform(widget.get(), &state);
  params.parent_widget = toplevel;
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
  Widget::InitParams params(Widget::InitParams::TYPE_POPUP);
  params.native_widget =
      new OwnershipTestNativeWidgetPlatform(widget, &state);
  widget->Init(params);

  // Now destroy the native widget.
  widget->CloseNow();

  EXPECT_TRUE(state.widget_deleted);
  EXPECT_TRUE(state.native_widget_deleted);
}

// NativeWidget owns its Widget, part 2: NativeWidget is a NativeWidget.
#if defined(OS_CHROMEOS) && defined(TOOLKIT_USES_GTK)
// Temporarily disable the test (http://crbug.com/104945).
TEST_F(WidgetOwnershipTest, DISABLED_Ownership_ViewsNativeWidgetOwnsWidget) {
#else
TEST_F(WidgetOwnershipTest, Ownership_ViewsNativeWidgetOwnsWidget) {
#endif
  OwnershipTestState state;

  Widget* toplevel = CreateTopLevelPlatformWidget();

  Widget* widget = new OwnershipTestWidget(&state);
  Widget::InitParams params(Widget::InitParams::TYPE_POPUP);
  params.native_widget =
      new OwnershipTestNativeWidgetPlatform(widget, &state);
  params.parent_widget = toplevel;
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
  Widget::InitParams params(Widget::InitParams::TYPE_POPUP);
  params.native_widget =
      new OwnershipTestNativeWidgetPlatform(widget, &state);
  widget->Init(params);

  // Now simulate a destroy of the platform native widget from the OS:
#if defined(USE_AURA)
  delete widget->GetNativeView();
#elif defined(OS_WIN)
  DestroyWindow(widget->GetNativeView());
#elif defined(TOOLKIT_USES_GTK)
  gtk_widget_destroy(widget->GetNativeView());
#endif

  EXPECT_TRUE(state.widget_deleted);
  EXPECT_TRUE(state.native_widget_deleted);
}

// NativeWidget owns its Widget, part 4: NativeWidget is a NativeWidget,
// destroyed by the view hierarchy that contains it.
#if defined(OS_CHROMEOS) && defined(TOOLKIT_USES_GTK)
// Temporarily disable the test (http://crbug.com/104945).
TEST_F(WidgetOwnershipTest,
       DISABLED_Ownership_ViewsNativeWidgetOwnsWidget_NativeDestroy) {
#else
TEST_F(WidgetOwnershipTest,
       Ownership_ViewsNativeWidgetOwnsWidget_NativeDestroy) {
#endif
  OwnershipTestState state;

  Widget* toplevel = CreateTopLevelPlatformWidget();

  Widget* widget = new OwnershipTestWidget(&state);
  Widget::InitParams params(Widget::InitParams::TYPE_POPUP);
  params.native_widget =
      new OwnershipTestNativeWidgetPlatform(widget, &state);
  params.parent_widget = toplevel;
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
  Widget::InitParams params(Widget::InitParams::TYPE_POPUP);
  params.native_widget =
      new OwnershipTestNativeWidgetPlatform(widget, &state);
  params.parent_widget = toplevel;
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

class WidgetObserverTest : public WidgetTest,
                                  Widget::Observer {
 public:
  WidgetObserverTest()
      : active_(NULL),
        widget_closed_(NULL),
        widget_activated_(NULL),
        widget_shown_(NULL),
        widget_hidden_(NULL) {
  }

  virtual ~WidgetObserverTest() {}

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

  void reset() {
    active_ = NULL;
    widget_closed_ = NULL;
    widget_activated_ = NULL;
    widget_deactivated_ = NULL;
    widget_shown_ = NULL;
    widget_hidden_ = NULL;
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

 private:

  Widget* active_;

  Widget* widget_closed_;
  Widget* widget_activated_;
  Widget* widget_deactivated_;
  Widget* widget_shown_;
  Widget* widget_hidden_;
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

#if !defined(USE_AURA) && defined(OS_WIN)
// Aura needs shell to maximize/fullscreen window.
// NativeWidgetGtk doesn't implement GetRestoredBounds.
TEST_F(WidgetTest, GetRestoredBounds) {
  Widget* toplevel = CreateTopLevelPlatformWidget();
  EXPECT_EQ(toplevel->GetWindowScreenBounds().ToString(),
            toplevel->GetRestoredBounds().ToString());
  toplevel->Show();
  toplevel->Maximize();
  RunPendingMessages();
  EXPECT_NE(toplevel->GetWindowScreenBounds().ToString(),
            toplevel->GetRestoredBounds().ToString());
  EXPECT_GT(toplevel->GetRestoredBounds().width(), 0);
  EXPECT_GT(toplevel->GetRestoredBounds().height(), 0);

  toplevel->Restore();
  RunPendingMessages();
  EXPECT_EQ(toplevel->GetWindowScreenBounds().ToString(),
            toplevel->GetRestoredBounds().ToString());

  toplevel->SetFullscreen(true);
  RunPendingMessages();
  EXPECT_NE(toplevel->GetWindowScreenBounds().ToString(),
            toplevel->GetRestoredBounds().ToString());
  EXPECT_GT(toplevel->GetRestoredBounds().width(), 0);
  EXPECT_GT(toplevel->GetRestoredBounds().height(), 0);
}
#endif

}  // namespace
}  // namespace views
