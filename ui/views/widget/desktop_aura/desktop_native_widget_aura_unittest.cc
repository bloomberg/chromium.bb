// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/window.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/widget/desktop_aura/desktop_native_widget_aura.h"
#include "ui/views/widget/widget.h"

namespace views {

typedef ViewsTestBase DesktopNativeWidgetAuraTest;

// Verifies creating a Widget with a parent that is not in a RootWindow doesn't
// crash.
TEST_F(DesktopNativeWidgetAuraTest, CreateWithParentNotInRootWindow) {
  scoped_ptr<aura::Window> window(new aura::Window(NULL));
  Widget widget;
  Widget::InitParams params = CreateParams(Widget::InitParams::TYPE_WINDOW);
  params.bounds = gfx::Rect(0, 0, 200, 200);
  params.parent = window.get();
  params.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.native_widget = new DesktopNativeWidgetAura(&widget);
  widget.Init(params);
}

}  // namespace views
