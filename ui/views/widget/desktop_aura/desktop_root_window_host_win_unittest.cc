// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/widget/desktop_aura/desktop_root_window_host_win.h"

#include "ui/aura/root_window.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/widget/desktop_aura/desktop_native_widget_aura.h"
#include "ui/views/widget/widget.h"

namespace views {

typedef ViewsTestBase DesktopRootWindowHostWinTest;

namespace {

// See description above SaveFocusOnDeactivateFromHandleCreate.
class TestDesktopRootWindowHostWin : public DesktopRootWindowHostWin {
 public:
  TestDesktopRootWindowHostWin(
      internal::NativeWidgetDelegate* native_widget_delegate,
      DesktopNativeWidgetAura* desktop_native_widget_aura)
      : DesktopRootWindowHostWin(native_widget_delegate,
                                 desktop_native_widget_aura) {}
  virtual ~TestDesktopRootWindowHostWin() {}

  // DesktopRootWindowHostWin overrides:
  virtual void HandleCreate() OVERRIDE {
    DesktopRootWindowHostWin::HandleCreate();
    SaveFocusOnDeactivate();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(TestDesktopRootWindowHostWin);
};

}  // namespace

// Verifies if SaveFocusOnDeactivate() is invoked from
// DesktopRootWindowHostWin::HandleCreate we don't crash.
TEST_F(DesktopRootWindowHostWinTest, SaveFocusOnDeactivateFromHandleCreate) {
  Widget widget;
  Widget::InitParams params = CreateParams(Widget::InitParams::TYPE_WINDOW);
  params.bounds = gfx::Rect(0, 0, 200, 200);
  params.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  DesktopNativeWidgetAura* desktop_native_widget_aura =
      new DesktopNativeWidgetAura(&widget);
  params.native_widget = desktop_native_widget_aura;
  params.desktop_root_window_host = new TestDesktopRootWindowHostWin(
      &widget, desktop_native_widget_aura);
  widget.Init(params);
}

}  // namespace views
