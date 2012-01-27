
// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/widget/native_widget_aura.h"

#include "ui/aura/root_window.h"
#include "ui/aura/window.h"
#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "testing/gtest/include/gtest/gtest.h"

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
    aura::RootWindow::GetInstance()->SetBounds(gfx::Rect(0, 0, 640, 480));
  }
  virtual void TearDown() OVERRIDE {
    aura::RootWindow::DeleteInstance();
    message_loop_.RunAllPending();
  }

 private:
  MessageLoopForUI message_loop_;

  DISALLOW_COPY_AND_ASSIGN(NativeWidgetAuraTest);
};

TEST_F(NativeWidgetAuraTest, CenterWindowLargeParent) {
  // Make a parent window larger than the host represented by rootwindow.
  scoped_ptr<aura::Window> parent(new aura::Window(NULL));
  parent->Init(ui::Layer::LAYER_HAS_NO_TEXTURE);
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
  parent->Init(ui::Layer::LAYER_HAS_NO_TEXTURE);
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

TEST_F(NativeWidgetAuraTest, GetClientAreaScreenBounds) {
  // Create a widget.
  Widget::InitParams params(Widget::InitParams::TYPE_WINDOW);
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.bounds.SetRect(10, 20, 300, 400);
  Widget* widget = new Widget();
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
