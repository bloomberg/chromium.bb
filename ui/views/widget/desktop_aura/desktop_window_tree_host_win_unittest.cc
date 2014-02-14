// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/widget/desktop_aura/desktop_window_tree_host_win.h"

#include "ui/aura/root_window.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/widget/desktop_aura/desktop_native_widget_aura.h"
#include "ui/views/widget/widget.h"

namespace views {

typedef ViewsTestBase DesktopWindowTreeHostWinTest;

namespace {

// See description above SaveFocusOnDeactivateFromHandleCreate.
class TestDesktopWindowTreeHostWin : public DesktopWindowTreeHostWin {
 public:
  TestDesktopWindowTreeHostWin(
      internal::NativeWidgetDelegate* native_widget_delegate,
      DesktopNativeWidgetAura* desktop_native_widget_aura)
      : DesktopWindowTreeHostWin(native_widget_delegate,
                                 desktop_native_widget_aura) {}
  virtual ~TestDesktopWindowTreeHostWin() {}

  // DesktopWindowTreeHostWin overrides:
  virtual void HandleCreate() OVERRIDE {
    DesktopWindowTreeHostWin::HandleCreate();
    SaveFocusOnDeactivate();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(TestDesktopWindowTreeHostWin);
};

}  // namespace

// Verifies if SaveFocusOnDeactivate() is invoked from
// DesktopWindowTreeHostWin::HandleCreate we don't crash.
TEST_F(DesktopWindowTreeHostWinTest, SaveFocusOnDeactivateFromHandleCreate) {
  Widget widget;
  Widget::InitParams params = CreateParams(Widget::InitParams::TYPE_WINDOW);
  params.bounds = gfx::Rect(0, 0, 200, 200);
  params.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  DesktopNativeWidgetAura* desktop_native_widget_aura =
      new DesktopNativeWidgetAura(&widget);
  params.native_widget = desktop_native_widget_aura;
  params.desktop_window_tree_host = new TestDesktopWindowTreeHostWin(
      &widget, desktop_native_widget_aura);
  widget.Init(params);
}

}  // namespace views
