// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "views/widget/native_widget_views.h"

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/test/test_views_delegate.h"
#include "ui/views/test/views_test_base.h"
#include "views/views_delegate.h"

#if defined(USE_AURA)
#include "ui/aura/window.h"
#include "views/widget/native_widget_aura.h"
#elif defined(OS_WIN)
#include "views/widget/native_widget_win.h"
#elif defined(TOOLKIT_USES_GTK)
#include "views/widget/native_widget_gtk.h"
#endif

namespace views {
namespace {

#if defined(TOOLKIT_USES_GTK)
// A widget that assumes mouse capture always works.
class NativeWidgetGtkCapture : public NativeWidgetGtk {
 public:
  NativeWidgetGtkCapture(internal::NativeWidgetDelegate* delegate)
      : NativeWidgetGtk(delegate),
        mouse_capture_(false) {}
  virtual ~NativeWidgetGtkCapture() {}
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

  DISALLOW_COPY_AND_ASSIGN(NativeWidgetGtkCapture);
};
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

class WidgetTestViewsDelegate : public TestViewsDelegate {
 public:
  WidgetTestViewsDelegate() : default_parent_view_(NULL) {
  }
  virtual ~WidgetTestViewsDelegate() {}

  void set_default_parent_view(View* default_parent_view) {
    default_parent_view_ = default_parent_view;
  }

  // Overridden from TestViewsDelegate:
  virtual View* GetDefaultParentView() OVERRIDE {
    return default_parent_view_;
  }

 private:
  View* default_parent_view_;

  DISALLOW_COPY_AND_ASSIGN(WidgetTestViewsDelegate);
};

class WidgetTest : public ViewsTestBase {
 public:
  WidgetTest() {
  }
  virtual ~WidgetTest() {
  }

  virtual void SetUp() OVERRIDE {
    set_views_delegate(new WidgetTestViewsDelegate());
    ViewsTestBase::SetUp();
  }

  WidgetTestViewsDelegate& widget_views_delegate() const {
    return static_cast<WidgetTestViewsDelegate&>(views_delegate());
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(WidgetTest);
};

NativeWidget* CreatePlatformNativeWidget(
    internal::NativeWidgetDelegate* delegate) {
#if defined(USE_AURA)
  return new NativeWidgetAura(delegate);
#elif defined(OS_WIN)
  return new NativeWidgetWin(delegate);
#elif defined(TOOLKIT_USES_GTK)
  return new NativeWidgetGtkCapture(delegate);
#endif
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

Widget* CreateTopLevelNativeWidgetViews() {
  Widget* toplevel = new Widget;
  Widget::InitParams params(Widget::InitParams::TYPE_WINDOW);
  params.native_widget = new NativeWidgetViews(toplevel);
  toplevel->Init(params);
  toplevel->SetContentsView(new View);
  return toplevel;
}

Widget* CreateChildNativeWidgetViewsWithParent(Widget* parent) {
  Widget* child = new Widget;
  Widget::InitParams params(Widget::InitParams::TYPE_CONTROL);
  params.native_widget = new NativeWidgetViews(child);
  params.parent_widget = parent;
  child->Init(params);
  child->SetContentsView(new View);
  return child;
}

Widget* CreateChildNativeWidgetViews() {
  return CreateChildNativeWidgetViewsWithParent(NULL);
}

bool WidgetHasMouseCapture(const Widget* widget) {
  return static_cast<const internal::NativeWidgetPrivate*>(widget->
      native_widget())-> HasMouseCapture();
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

TEST_F(WidgetTest, GetTopLevelWidget_Synthetic) {
  // Create a hierarchy consisting of a top level platform native widget and a
  // child NativeWidgetViews.
  Widget* toplevel = CreateTopLevelPlatformWidget();
  widget_views_delegate().set_default_parent_view(toplevel->GetRootView());
  Widget* child = CreateTopLevelNativeWidgetViews();

  EXPECT_EQ(toplevel, toplevel->GetTopLevelWidget());
  EXPECT_EQ(child, child->GetTopLevelWidget());

  toplevel->CloseNow();
  // |child| should be automatically destroyed with |toplevel|.
}

// Creates a hierarchy consisting of a desktop platform native widget, a
// toplevel NativeWidgetViews, and a child of that toplevel, another
// NativeWidgetViews.
TEST_F(WidgetTest, GetTopLevelWidget_SyntheticDesktop) {
  // Create a hierarchy consisting of a desktop platform native widget,
  // a toplevel NativeWidgetViews and a chlid NativeWidgetViews.
  Widget* desktop = CreateTopLevelPlatformWidget();
  widget_views_delegate().set_default_parent_view(desktop->GetRootView());
  Widget* toplevel = CreateTopLevelNativeWidgetViews(); // Will be parented
                                                        // automatically to
                                                        // |toplevel|.

  Widget* child = CreateChildNativeWidgetViewsWithParent(toplevel);

  EXPECT_EQ(desktop, desktop->GetTopLevelWidget());
  EXPECT_EQ(toplevel, toplevel->GetTopLevelWidget());
  EXPECT_EQ(toplevel, child->GetTopLevelWidget());

  desktop->CloseNow();
  // |toplevel|, |child| should be automatically destroyed with |toplevel|.
}

// This is flaky on touch build. See crbug.com/94137.
#if defined(TOUCH_UI)
#define MAYBE_GrabUngrab DISABLED_GrabUngrab
#else
#define MAYBE_GrabUngrab GrabUngrab
#endif
// Tests some grab/ungrab events.
TEST_F(WidgetTest, MAYBE_GrabUngrab) {
  Widget* toplevel = CreateTopLevelPlatformWidget();
  Widget* child1 = CreateChildNativeWidgetViewsWithParent(toplevel);
  Widget* child2 = CreateChildNativeWidgetViewsWithParent(toplevel);

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

// Tests visibility of synthetic child widgets.
TEST_F(WidgetTest, Visibility_Synthetic) {
  // Create a hierarchy consisting of a desktop platform native widget,
  // a toplevel NativeWidgetViews and a chlid NativeWidgetViews.
  Widget* desktop = CreateTopLevelPlatformWidget();
  desktop->Show();

  widget_views_delegate().set_default_parent_view(desktop->GetRootView());
  Widget* toplevel = CreateTopLevelNativeWidgetViews(); // Will be parented
                                                        // automatically to
                                                        // |toplevel|.

  Widget* child = CreateChildNativeWidgetViewsWithParent(toplevel);

  EXPECT_FALSE(toplevel->IsVisible());
  EXPECT_FALSE(child->IsVisible());

  child->Show();

  EXPECT_FALSE(toplevel->IsVisible());
  EXPECT_FALSE(child->IsVisible());

  toplevel->Show();

  EXPECT_TRUE(toplevel->IsVisible());
  EXPECT_TRUE(child->IsVisible());

  desktop->CloseNow();
}

////////////////////////////////////////////////////////////////////////////////
// Widget ownership tests.
//
// Tests various permutations of Widget ownership specified in the
// InitParams::Ownership param.

// A WidgetTest that supplies a toplevel widget for NativeWidgetViews to parent
// to.
class WidgetOwnershipTest : public WidgetTest {
 public:
  WidgetOwnershipTest() {}
  virtual ~WidgetOwnershipTest() {}

  virtual void SetUp() {
    WidgetTest::SetUp();
    desktop_widget_ = CreateTopLevelPlatformWidget();
    widget_views_delegate().set_default_parent_view(
        desktop_widget_->GetRootView());
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
class OwnershipTestNativeWidget :
#if defined(USE_AURA)
    public NativeWidgetAura {
#elif defined(OS_WIN)
    public NativeWidgetWin {
#elif defined(TOOLKIT_USES_GTK)
    public NativeWidgetGtk {
#endif
public:
  OwnershipTestNativeWidget(internal::NativeWidgetDelegate* delegate,
                            OwnershipTestState* state)
#if defined(USE_AURA)
    : NativeWidgetAura(delegate),
#elif defined(OS_WIN)
    : NativeWidgetWin(delegate),
#elif defined(TOOLKIT_USES_GTK)
    : NativeWidgetGtk(delegate),
#endif
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
class OwnershipTestNativeWidgetViews : public NativeWidgetViews {
 public:
  OwnershipTestNativeWidgetViews(internal::NativeWidgetDelegate* delegate,
                                 OwnershipTestState* state)
      : NativeWidgetViews(delegate),
        state_(state) {
  }
  virtual ~OwnershipTestNativeWidgetViews() {
    state_->native_widget_deleted = true;
  }

 private:
  OwnershipTestState* state_;

  DISALLOW_COPY_AND_ASSIGN(OwnershipTestNativeWidgetViews);
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
  params.native_widget = new OwnershipTestNativeWidget(widget.get(), &state);
  params.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  widget->Init(params);

  // Now delete the Widget, which should delete the NativeWidget.
  widget.reset();

  EXPECT_TRUE(state.widget_deleted);
  EXPECT_TRUE(state.native_widget_deleted);

  // TODO(beng): write test for this ownership scenario and the NativeWidget
  //             being deleted out from under the Widget.
}

// Widget owns its NativeWidget, part 2: NativeWidget is a NativeWidgetViews.
TEST_F(WidgetOwnershipTest, Ownership_WidgetOwnsViewsNativeWidget) {
  OwnershipTestState state;

  scoped_ptr<Widget> widget(new OwnershipTestWidget(&state));
  Widget::InitParams params(Widget::InitParams::TYPE_POPUP);
  params.native_widget =
      new OwnershipTestNativeWidgetViews(widget.get(), &state);
  params.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  widget->Init(params);

  // Now delete the Widget, which should delete the NativeWidget.
  widget.reset();

  EXPECT_TRUE(state.widget_deleted);
  EXPECT_TRUE(state.native_widget_deleted);

  // TODO(beng): write test for this ownership scenario and the NativeWidget
  //             being deleted out from under the Widget.
}

// Widget owns its NativeWidget, part 3: NativeWidget is a NativeWidgetViews,
// destroy the parent view.
TEST_F(WidgetOwnershipTest,
       Ownership_WidgetOwnsViewsNativeWidget_DestroyParentView) {
  OwnershipTestState state;

  Widget* toplevel = CreateTopLevelPlatformWidget();

  scoped_ptr<Widget> widget(new OwnershipTestWidget(&state));
  Widget::InitParams params(Widget::InitParams::TYPE_POPUP);
  params.native_widget = new OwnershipTestNativeWidgetViews(widget.get(),
                                                            &state);
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
  params.native_widget = new OwnershipTestNativeWidget(widget, &state);
  widget->Init(params);

  // Now destroy the native widget.
  widget->CloseNow();

  EXPECT_TRUE(state.widget_deleted);
  EXPECT_TRUE(state.native_widget_deleted);
}

// NativeWidget owns its Widget, part 2: NativeWidget is a NativeWidgetViews.
TEST_F(WidgetOwnershipTest, Ownership_ViewsNativeWidgetOwnsWidget) {
  OwnershipTestState state;

  Widget* toplevel = CreateTopLevelPlatformWidget();

  Widget* widget = new OwnershipTestWidget(&state);
  Widget::InitParams params(Widget::InitParams::TYPE_POPUP);
  params.native_widget = new OwnershipTestNativeWidgetViews(widget, &state);
  params.parent_widget = toplevel;
  widget->Init(params);

  // Now destroy the native widget. This is achieved by closing the toplevel.
  toplevel->CloseNow();

  // The NativeWidgetViews won't be deleted until after a return to the message
  // loop so we have to run pending messages before testing the destruction
  // status.
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
  params.native_widget = new OwnershipTestNativeWidget(widget, &state);
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

// NativeWidget owns its Widget, part 4: NativeWidget is a NativeWidgetViews,
// destroyed by the view hierarchy that contains it.
TEST_F(WidgetOwnershipTest,
       Ownership_ViewsNativeWidgetOwnsWidget_NativeDestroy) {
  OwnershipTestState state;

  Widget* toplevel = CreateTopLevelPlatformWidget();

  Widget* widget = new OwnershipTestWidget(&state);
  Widget::InitParams params(Widget::InitParams::TYPE_POPUP);
  params.native_widget = new OwnershipTestNativeWidgetViews(widget, &state);
  params.parent_widget = toplevel;
  widget->Init(params);

  // Destroy the widget (achieved by closing the toplevel).
  toplevel->CloseNow();

  // The NativeWidgetViews won't be deleted until after a return to the message
  // loop so we have to run pending messages before testing the destruction
  // status.
  RunPendingMessages();

  EXPECT_TRUE(state.widget_deleted);
  EXPECT_TRUE(state.native_widget_deleted);
}

// NativeWidget owns its Widget, part 5: NativeWidget is a NativeWidgetViews,
// we close it directly.
TEST_F(WidgetOwnershipTest,
       Ownership_ViewsNativeWidgetOwnsWidget_Close) {
  OwnershipTestState state;

  Widget* toplevel = CreateTopLevelPlatformWidget();

  Widget* widget = new OwnershipTestWidget(&state);
  Widget::InitParams params(Widget::InitParams::TYPE_POPUP);
  params.native_widget = new OwnershipTestNativeWidgetViews(widget, &state);
  params.parent_widget = toplevel;
  widget->Init(params);

  // Destroy the widget.
  widget->Close();
  toplevel->CloseNow();

  // The NativeWidgetViews won't be deleted until after a return to the message
  // loop so we have to run pending messages before testing the destruction
  // status.
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
    Widget* widget = CreateTopLevelNativeWidgetViews();
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

TEST_F(WidgetObserverTest, ActivationChange) {
  Widget* toplevel = CreateTopLevelPlatformWidget();
  widget_views_delegate().set_default_parent_view(toplevel->GetRootView());

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

TEST_F(WidgetObserverTest, VisibilityChange) {
  Widget* toplevel = CreateTopLevelPlatformWidget();
  widget_views_delegate().set_default_parent_view(toplevel->GetRootView());

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
