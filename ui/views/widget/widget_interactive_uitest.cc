// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/views/widget/widget.h"

#if defined(USE_AURA)
#include "ui/aura/client/activation_client.h"
#include "ui/aura/root_window.h"
#include "ui/views/test/views_test_base.h"
#if !defined(OS_CHROMEOS)
#include "ui/views/widget/desktop_aura/desktop_native_widget_aura.h"
#endif
#endif

namespace views {

#if defined(OS_WIN) && defined(USE_AURA)
// Tests whether activation and focus change works correctly in Windows AURA.
// We test the following:-
// 1. If the active aura window is correctly set when a top level widget is
//    created.
// 2. If the active aura window in widget 1 created above, is set to NULL when
//    another top level widget is created and focused.
// 3. On focusing the native platform window for widget 1, the active aura
//    window for widget 1 should be set and that for widget 2 should reset.
// TODO(ananta)
// Discuss with erg on how to write this test for linux x11 aura.
TEST_F(ViewsTestBase, DesktopNativeWidgetAuraActivationAndFocusTest) {
  // Create widget 1 and expect the active window to be its window.
  View* contents_view1 = new View;
  contents_view1->set_focusable(true);
  Widget widget1;
  Widget::InitParams init_params =
      CreateParams(Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  init_params.bounds = gfx::Rect(0, 0, 200, 200);
  init_params.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  init_params.native_widget = new DesktopNativeWidgetAura(&widget1);
  widget1.Init(init_params);
  widget1.SetContentsView(contents_view1);
  widget1.Show();
  aura::RootWindow* root_window1= widget1.GetNativeView()->GetRootWindow();
  contents_view1->RequestFocus();

  EXPECT_TRUE(root_window1 != NULL);
  aura::client::ActivationClient* activation_client1 =
      aura::client::GetActivationClient(root_window1);
  EXPECT_TRUE(activation_client1 != NULL);
  EXPECT_EQ(activation_client1->GetActiveWindow(), widget1.GetNativeView());

  // Create widget 2 and expect the active window to be its window.
  View* contents_view2 = new View;
  Widget widget2;
  Widget::InitParams init_params2 =
      CreateParams(Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  init_params2.bounds = gfx::Rect(0, 0, 200, 200);
  init_params2.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  init_params2.native_widget = new DesktopNativeWidgetAura(&widget2);
  widget2.Init(init_params2);
  widget2.SetContentsView(contents_view2);
  widget2.Show();
  aura::RootWindow* root_window2 = widget2.GetNativeView()->GetRootWindow();
  contents_view2->RequestFocus();
  ::SetActiveWindow(root_window2->GetAcceleratedWidget());

  aura::client::ActivationClient* activation_client2 =
      aura::client::GetActivationClient(root_window2);
  EXPECT_TRUE(activation_client2 != NULL);
  EXPECT_EQ(activation_client2->GetActiveWindow(), widget2.GetNativeView());
  EXPECT_EQ(activation_client1->GetActiveWindow(),
            reinterpret_cast<aura::Window*>(NULL));

  // Now set focus back to widget 1 and expect the active window to be its
  // window.
  contents_view1->RequestFocus();
  ::SetActiveWindow(root_window1->GetAcceleratedWidget());
  EXPECT_EQ(activation_client2->GetActiveWindow(),
            reinterpret_cast<aura::Window*>(NULL));
  EXPECT_EQ(activation_client1->GetActiveWindow(), widget1.GetNativeView());
}
#endif

}  // namespace views
