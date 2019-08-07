// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ws/window_lookup.h"

#include "ash/test/ash_test_base.h"
#include "ui/aura/env.h"
#include "ui/aura/test/mus/change_completion_waiter.h"
#include "ui/aura/window.h"
#include "ui/views/mus/mus_client.h"
#include "ui/views/widget/widget.h"

namespace ash {

using WindowLookupTest = SingleProcessMashTestBase;

TEST_F(WindowLookupTest, AddWindowToTabletMode) {
  // Create a widget. This widget is backed by mus.
  views::Widget widget;
  views::Widget::InitParams params;
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.bounds = gfx::Rect(0, 0, 200, 200);
  params.native_widget =
      views::MusClient::Get()->CreateNativeWidget(params, &widget);
  widget.Init(params);

  aura::Window* widget_root = widget.GetNativeWindow()->GetRootWindow();
  ASSERT_TRUE(CurrentContext());
  ASSERT_NE(widget_root, CurrentContext());

  // At this point ash hasn't created the proxy (request is async, over mojo).
  // So, there should be no proxy available yet.
  EXPECT_FALSE(window_lookup::IsProxyWindow(widget_root));
  EXPECT_FALSE(window_lookup::GetProxyWindowForClientWindow(widget_root));

  // Flush all messages from the WindowTreeClient to ensure the window service
  // has finished Widget creation.
  aura::test::WaitForAllChangesToComplete();

  // Now the proxy should be available.
  EXPECT_FALSE(window_lookup::IsProxyWindow(widget_root));
  aura::Window* proxy =
      window_lookup::GetProxyWindowForClientWindow(widget_root);
  ASSERT_TRUE(proxy);
  EXPECT_NE(proxy, widget_root);
  EXPECT_EQ(aura::Env::Mode::LOCAL, proxy->env()->mode());

  // Ensure we can go back to the client created window.
  EXPECT_EQ(widget_root, window_lookup::GetClientWindowForProxyWindow(proxy));
}

}  // namespace ash
