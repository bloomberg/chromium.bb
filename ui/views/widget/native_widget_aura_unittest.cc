// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/widget/native_widget_aura.h"

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/env.h"
#include "ui/aura/layout_manager.h"
#include "ui/aura/monitor_manager.h"
#include "ui/aura/root_window.h"
#include "ui/aura/single_monitor_manager.h"
#include "ui/aura/test/aura_test_helper.h"
#include "ui/aura/window.h"
#include "ui/gfx/screen.h"
#include "ui/views/widget/root_view.h"
#include "ui/views/widget/widget_delegate.h"

namespace views {
namespace {

NativeWidgetAura* Init(aura::Window* parent, Widget* widget) {
  Widget::InitParams params(Widget::InitParams::TYPE_POPUP);
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.parent = parent;
  widget->Init(params);
  return static_cast<NativeWidgetAura*>(widget->native_widget());
}

class NativeWidgetAuraTest : public testing::Test {
 public:
  NativeWidgetAuraTest() {}
  virtual ~NativeWidgetAuraTest() {}

  // testing::Test overrides:
  virtual void SetUp() OVERRIDE {
    aura_test_helper_.reset(new aura::test::AuraTestHelper(&message_loop_));
    aura_test_helper_->SetUp();
    root_window()->SetBounds(gfx::Rect(0, 0, 640, 480));
    root_window()->SetHostSize(gfx::Size(640, 480));
  }
  virtual void TearDown() OVERRIDE {
    message_loop_.RunAllPending();
    aura_test_helper_->TearDown();
  }

 protected:
  aura::RootWindow* root_window() { return aura_test_helper_->root_window(); }

 private:
  MessageLoopForUI message_loop_;
  scoped_ptr<aura::test::AuraTestHelper> aura_test_helper_;

  DISALLOW_COPY_AND_ASSIGN(NativeWidgetAuraTest);
};

TEST_F(NativeWidgetAuraTest, CenterWindowLargeParent) {
  // Make a parent window larger than the host represented by rootwindow.
  scoped_ptr<aura::Window> parent(new aura::Window(NULL));
  parent->Init(ui::LAYER_NOT_DRAWN);
  parent->SetBounds(gfx::Rect(0, 0, 1024, 800));
  scoped_ptr<Widget>  widget(new Widget());
  NativeWidgetAura* window = Init(parent.get(), widget.get());

  window->CenterWindow(gfx::Size(100, 100));
  EXPECT_EQ(gfx::Rect( (640 - 100) / 2,
                       (480 - 100) / 2,
                       100, 100),
            window->GetNativeWindow()->bounds());
  widget->CloseNow();
}

TEST_F(NativeWidgetAuraTest, CenterWindowSmallParent) {
  // Make a parent window smaller than the host represented by rootwindow.
  scoped_ptr<aura::Window> parent(new aura::Window(NULL));
  parent->Init(ui::LAYER_NOT_DRAWN);
  parent->SetBounds(gfx::Rect(0, 0, 480, 320));
  scoped_ptr<Widget> widget(new Widget());
  NativeWidgetAura* window  = Init(parent.get(), widget.get());

  window->CenterWindow(gfx::Size(100, 100));
  EXPECT_EQ(gfx::Rect( (480 - 100) / 2,
                       (320 - 100) / 2,
                       100, 100),
            window->GetNativeWindow()->bounds());
  widget->CloseNow();
}

// Used by ShowMaximizedDoesntBounceAround. See it for details.
class TestLayoutManager : public aura::LayoutManager {
 public:
  TestLayoutManager() {}

  virtual void OnWindowResized() OVERRIDE {
  }
  virtual void OnWindowAddedToLayout(aura::Window* child) OVERRIDE {
    // This simulates what happens when adding a maximized window.
    SetChildBoundsDirect(child, gfx::Rect(0, 0, 300, 300));
  }
  virtual void OnWillRemoveWindowFromLayout(aura::Window* child) OVERRIDE {
  }
  virtual void OnWindowRemovedFromLayout(aura::Window* child) OVERRIDE {
  }
  virtual void OnChildWindowVisibilityChanged(aura::Window* child,
                                              bool visible) OVERRIDE {
  }
  virtual void SetChildBounds(aura::Window* child,
                              const gfx::Rect& requested_bounds) OVERRIDE {
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(TestLayoutManager);
};

// This simulates BrowserView, which creates a custom RootView so that
// OnNativeWidgetSizeChanged that is invoked during Init matters.
class TestWidget : public views::Widget {
 public:
  TestWidget() : did_size_change_more_than_once_(false) {
  }

  // Returns true if the size changes to a non-empty size, and then to another
  // size.
  bool did_size_change_more_than_once() const {
    return did_size_change_more_than_once_;
  }

  virtual void OnNativeWidgetSizeChanged(const gfx::Size& new_size) OVERRIDE {
    if (last_size_.IsEmpty())
      last_size_ = new_size;
    else if (!did_size_change_more_than_once_ && new_size != last_size_)
      did_size_change_more_than_once_ = true;
    Widget::OnNativeWidgetSizeChanged(new_size);
  }

 private:
  bool did_size_change_more_than_once_;
  gfx::Size last_size_;

  DISALLOW_COPY_AND_ASSIGN(TestWidget);
};

// Verifies the size of the widget doesn't change more than once during Init if
// the window ends up maximized. This is important as otherwise
// RenderWidgetHostViewAura ends up getting resized during construction, which
// leads to noticable flashes.
TEST_F(NativeWidgetAuraTest, ShowMaximizedDoesntBounceAround) {
  root_window()->SetBounds(gfx::Rect(0, 0, 640, 480));
  root_window()->SetLayoutManager(new TestLayoutManager);
  scoped_ptr<TestWidget> widget(new TestWidget());
  Widget::InitParams params(Widget::InitParams::TYPE_WINDOW);
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.parent = NULL;
  params.show_state = ui::SHOW_STATE_MAXIMIZED;
  params.bounds = gfx::Rect(10, 10, 100, 200);
  widget->Init(params);
  EXPECT_FALSE(widget->did_size_change_more_than_once());
  widget->CloseNow();
}

TEST_F(NativeWidgetAuraTest, GetClientAreaScreenBounds) {
  // Create a widget.
  Widget::InitParams params(Widget::InitParams::TYPE_WINDOW);
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.bounds.SetRect(10, 20, 300, 400);
  scoped_ptr<Widget> widget(new Widget());
  widget->Init(params);

  // For Aura, client area bounds match window bounds.
  gfx::Rect client_bounds = widget->GetClientAreaScreenBounds();
  EXPECT_EQ(10, client_bounds.x());
  EXPECT_EQ(20, client_bounds.y());
  EXPECT_EQ(300, client_bounds.width());
  EXPECT_EQ(400, client_bounds.height());
}

}  // namespace
}  // namespace views
