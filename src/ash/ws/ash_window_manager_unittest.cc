// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/ash_switches.h"
#include "ash/public/interfaces/ash_window_manager.mojom.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/ash_test_helper.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "ash/wm/tablet_mode/tablet_mode_controller_test_api.h"
#include "ash/wm/tablet_mode/tablet_mode_window_manager.h"
#include "base/test/scoped_feature_list.h"
#include "ui/aura/env.h"
#include "ui/aura/mus/window_mus.h"
#include "ui/aura/mus/window_tree_client.h"
#include "ui/aura/test/env_test_helper.h"
#include "ui/aura/test/mus/window_tree_client_test_api.h"
#include "ui/aura/window.h"
#include "ui/base/ui_base_features.h"
#include "ui/views/mus/mus_client.h"
#include "ui/views/widget/widget.h"

namespace ash {

class AshWindowManagerTest : public AshTestBase {
 public:
  AshWindowManagerTest() = default;
  ~AshWindowManagerTest() override = default;

  // AshTestBase:
  void SetUp() override {
    original_aura_env_mode_ =
        aura::test::EnvTestHelper().SetMode(aura::Env::Mode::MUS);
    feature_list_.InitWithFeatures({::features::kSingleProcessMash}, {});
    AshTestBase::SetUp();
  }
  void TearDown() override {
    AshTestBase::TearDown();
    aura::test::EnvTestHelper().SetMode(original_aura_env_mode_);
  }

 private:
  aura::Env::Mode original_aura_env_mode_ = aura::Env::Mode::LOCAL;
  base::test::ScopedFeatureList feature_list_;

  DISALLOW_COPY_AND_ASSIGN(AshWindowManagerTest);
};

TEST_F(AshWindowManagerTest, AddWindowToTabletMode) {
  // TabletModeController calls to PowerManagerClient with a callback that is
  // run via a posted task. Run the loop now so that we know the task is
  // processed. Without this, the task gets processed later on, interfering
  // with this test.
  RunAllPendingInMessageLoop();

  // This test configures views with mus, which means it triggers some of the
  // DCHECKs ensuring Shell's Env is used.
  SetRunningOutsideAsh();

  // Configure views backed by mus.
  views::MusClient::InitParams mus_client_init_params;
  mus_client_init_params.connector =
      ash_test_helper()->GetWindowServiceConnector();
  mus_client_init_params.create_wm_state = false;
  mus_client_init_params.running_in_ws_process = true;
  views::MusClient mus_client(mus_client_init_params);

  // Create a widget. This widget is backed by mus.
  views::Widget widget;
  views::Widget::InitParams params;
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.bounds = gfx::Rect(0, 0, 200, 200);
  params.native_widget = mus_client.CreateNativeWidget(params, &widget);
  widget.Init(params);

  // Flush all messages from the WindowTreeClient to ensure the window service
  // has finished Widget createion.
  aura::WindowTreeClientTestApi(mus_client.window_tree_client())
      .FlushForTesting();

  // Turn on tablet mode.
  Shell::Get()->tablet_mode_controller()->EnableTabletModeWindowManager(true);
  TabletModeWindowManager* tablet_wm =
      TabletModeControllerTestApi().tablet_mode_window_manager();
  EXPECT_EQ(0, tablet_wm->GetNumberOfManagedWindows());

  // Call to AddWindowToTabletMode() over the mojom.
  ash::mojom::AshWindowManagerAssociatedPtr ash_window_manager =
      mus_client.window_tree_client()
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
