// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/test/views_test_base.h"
#include "ui/views/widget/desktop_aura/desktop_native_widget_aura.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/dialog_delegate.h"

namespace views {

typedef ViewsTestBase DesktopScreenPositionClientTest;

// Verifies setting the bounds of a dialog parented to a Widget with a
// DesktopNativeWidgetAura is positioned correctly.
TEST_F(DesktopScreenPositionClientTest, PositionDialog) {
  Widget parent_widget;
  Widget::InitParams init_params =
      CreateParams(Widget::InitParams::TYPE_WINDOW);
  init_params.bounds = gfx::Rect(10, 11, 200, 200);
  init_params.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  init_params.native_widget = new DesktopNativeWidgetAura(&parent_widget);
  parent_widget.Init(init_params);
  //  parent_widget.Show();

  // Owned by |dialog|.
  DialogDelegateView* dialog_delegate_view = new DialogDelegateView;
  // Owned by |parent_widget|.
  Widget* dialog = DialogDelegate::CreateDialogWidget(
      dialog_delegate_view,
      NULL,
      parent_widget.GetNativeView());
  dialog->SetBounds(gfx::Rect(11, 12, 200, 200));
  EXPECT_EQ("11,12", dialog->GetWindowBoundsInScreen().origin().ToString());
}

}  // namespace views
