// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/interfaces/ash_window_manager.mojom.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "ash/wm/tablet_mode/tablet_mode_controller_test_api.h"
#include "ash/wm/tablet_mode/tablet_mode_window_manager.h"
#include "ui/aura/mus/window_mus.h"
#include "ui/aura/mus/window_tree_client.h"
#include "ui/aura/test/env_test_helper.h"
#include "ui/aura/test/mus/change_completion_waiter.h"
#include "ui/aura/window.h"
#include "ui/views/mus/mus_client.h"
#include "ui/views/widget/widget.h"

namespace ash {

using AshWindowManagerTest = SingleProcessMashTestBase;

TEST_F(AshWindowManagerTest, AddWindowToTabletMode) {
  // Create a widget. This widget is backed by mus.
  views::Widget widget;
  views::Widget::InitParams params;
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.bounds = gfx::Rect(0, 0, 200, 200);
  params.native_widget =
      views::MusClient::Get()->CreateNativeWidget(params, &widget);
  widget.Init(params);

  // Flush all messages from the WindowTreeClient to ensure the window service
  // has finished Widget creation.
  aura::test::WaitForAllChangesToComplete();

  // Turn on tablet mode.
  Shell::Get()->tablet_mode_controller()->EnableTabletModeWindowManager(true);
  TabletModeWindowManager* tablet_wm =
      TabletModeControllerTestApi().tablet_mode_window_manager();
  EXPECT_EQ(0, tablet_wm->GetNumberOfManagedWindows());

  // Call to AddWindowToTabletMode() over the mojom.
  ash::mojom::AshWindowManagerAssociatedPtr ash_window_manager =
      views::MusClient::Get()
          ->window_tree_client()
          ->BindWindowManagerInterface<ash::mojom::AshWindowManager>();
  ash_window_manager->AddWindowToTabletMode(
      aura::WindowMus::Get(widget.GetNativeWindow()->parent())->server_id());

  // Ensures the message is processed by Ash.
  ash_window_manager.FlushForTesting();

  // The callback to AddWindowToTabletMode() should have been processed and
  // added the window to the TabletModeWindowManager.
  EXPECT_EQ(1, tablet_wm->GetNumberOfManagedWindows());
}

}  // namespace ash
