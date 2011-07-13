// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "views/widget/native_widget_views.h"

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "views/test/test_views_delegate.h"
#include "views/views_delegate.h"

#if defined(OS_WIN)
#include "views/widget/native_widget_win.h"
#elif defined(TOOLKIT_USES_GTK)
#include "views/widget/native_widget_gtk.h"
#endif

namespace views {
namespace {

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

class WidgetTest : public testing::Test {
 public:
  WidgetTest() {
#if defined(OS_WIN)
    OleInitialize(NULL);
#endif
  }
  virtual ~WidgetTest() {
#if defined(OS_WIN)
    OleUninitialize();
#endif
  }

  virtual void TearDown() {
    // Flush the message loop because we have pending release tasks
    // and these tasks if un-executed would upset Valgrind.
    RunPendingMessages();
  }

  void RunPendingMessages() {
    message_loop_.RunAllPending();
  }

 protected:
  WidgetTestViewsDelegate views_delegate;

 private:
  MessageLoopForUI message_loop_;

  DISALLOW_COPY_AND_ASSIGN(WidgetTest);
};

NativeWidget* CreatePlatformNativeWidget(
    internal::NativeWidgetDelegate* delegate) {
#if defined(OS_WIN)
  return new NativeWidgetWin(delegate);
#elif defined(TOOLKIT_USES_GTK)
  return new NativeWidgetGtk(delegate);
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

////////////////////////////////////////////////////////////////////////////////
// Widget::GetTopLevelWidget tests.

TEST_F(WidgetTest, GetTopLevelWidget_Native) {
  // Create a hierarchy of native widgets.
  Widget* toplevel = CreateTopLevelPlatformWidget();
#if defined(OS_WIN)
  gfx::NativeView parent = toplevel->GetNativeView();
#elif defined(TOOLKIT_USES_GTK)
  NativeWidgetGtk* native_widget =
      static_cast<NativeWidgetGtk*>(toplevel->native_widget());
  gfx::NativeView parent = native_widget->window_contents();
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
  views_delegate.set_default_parent_view(toplevel->GetRootView());
  Widget* child = CreateChildNativeWidgetViews();

  EXPECT_EQ(toplevel, toplevel->GetTopLevelWidget());
  EXPECT_EQ(child, child->GetTopLevelWidget());

  toplevel->CloseNow();
  // |child| should be automatically destroyed with |toplevel|.
}

// Creates a hierarchy consisting of a top level platform native widget, a child
// NativeWidgetViews, and a child of that child, another NativeWidgetViews.
TEST_F(WidgetTest, GetTopLevelWidget_SyntheticParent) {
  Widget* toplevel = CreateTopLevelPlatformWidget();
  views_delegate.set_default_parent_view(toplevel->GetRootView());

  Widget* child1 = CreateChildNativeWidgetViews(); // Will be parented
                                                   // automatically to
                                                   // |toplevel|.
  Widget* child11 = CreateChildNativeWidgetViewsWithParent(child1);

  EXPECT_EQ(toplevel, toplevel->GetTopLevelWidget());
  EXPECT_EQ(child1, child1->GetTopLevelWidget());
  EXPECT_EQ(child1, child11->GetTopLevelWidget());

  toplevel->CloseNow();
  // |child1| and |child11| should be destroyed with |toplevel|.
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
    desktop_widget_ = CreateTopLevelPlatformWidget();
    views_delegate.set_default_parent_view(desktop_widget_->GetRootView());
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
#if defined(OS_WIN)
    public NativeWidgetWin {
#elif defined(TOOLKIT_USES_GTK)
    public NativeWidgetGtk {
#endif
public:
  OwnershipTestNativeWidget(internal::NativeWidgetDelegate* delegate,
                            OwnershipTestState* state)
#if defined(OS_WIN)
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
#if defined(OS_WIN)
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
      widget_activated_ = widget;
      active_ = widget;
    } else
      widget_deactivated_ = widget;
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
    Widget* widget = CreateChildNativeWidgetViews();
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

// TODO: This test should be enabled when NativeWidgetViews::Activate is
// implemented.
TEST_F(WidgetObserverTest, DISABLED_ActivationChange) {
  Widget* toplevel = CreateTopLevelPlatformWidget();
  views_delegate.set_default_parent_view(toplevel->GetRootView());

  Widget* child1 = NewWidget();
  Widget* child2 = NewWidget();

  reset();

  child1->Activate();
  EXPECT_EQ(child1, widget_activated());

  child2->Activate();
  EXPECT_EQ(child1, widget_deactivated());
  EXPECT_EQ(child2, widget_activated());
  EXPECT_EQ(child2, active());
}

TEST_F(WidgetObserverTest, VisibilityChange) {
  Widget* toplevel = CreateTopLevelPlatformWidget();
  views_delegate.set_default_parent_view(toplevel->GetRootView());

  Widget* child1 = NewWidget();
  Widget* child2 = NewWidget();

  reset();

  child1->Hide();
  EXPECT_EQ(child1, widget_hidden());

  child2->Hide();
  EXPECT_EQ(child2, widget_hidden());

  child1->Show();
  EXPECT_EQ(child1, widget_shown());

  child2->Show();
  EXPECT_EQ(child2, widget_shown());
}

}  // namespace
}  // namespace views
