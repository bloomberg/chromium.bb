// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/mus/focus_synchronizer.h"

#include <memory>

#include "ui/aura/mus/window_mus.h"
#include "ui/aura/mus/window_tree_client.h"
#include "ui/aura/mus/window_tree_host_mus.h"
#include "ui/aura/mus/window_tree_host_mus_init_params.h"
#include "ui/aura/test/aura_mus_test_base.h"
#include "ui/aura/window.h"
#include "ui/wm/core/base_focus_rules.h"
#include "ui/wm/core/focus_controller.h"

namespace aura {
namespace {

class TestFocusRules : public wm::BaseFocusRules {
 public:
  TestFocusRules() = default;
  ~TestFocusRules() override = default;

  // wm::BaseFocusRules overrides:
  bool SupportsChildActivation(const Window* window) const override {
    return true;
  }
  bool CanActivateWindow(const Window* window) const override { return true; }

 private:
  DISALLOW_COPY_AND_ASSIGN(TestFocusRules);
};

}  // namespace

using FocusSynchronizerTest = test::AuraMusClientTestBase;

TEST_F(FocusSynchronizerTest, SetFocusFromServerResetsActiveFocusClient) {
  std::unique_ptr<WindowTreeHostMus> window_tree_host =
      std::make_unique<WindowTreeHostMus>(
          CreateInitParamsForTopLevel(window_tree_client_impl()));
  window_tree_host->InitHost();
  window_tree_host->Show();
  Window child_window(nullptr);
  child_window.Init(ui::LAYER_NOT_DRAWN);
  window_tree_host->window()->AddChild(&child_window);
  child_window.Show();
  wm::FocusController focus_controller(new TestFocusRules);
  client::SetFocusClient(window_tree_host->window(), &focus_controller);
  window_tree_client_impl()->focus_synchronizer()->SetFocusFromServer(
      WindowMus::Get(&child_window));
  EXPECT_TRUE(child_window.HasFocus());
  EXPECT_EQ(
      &focus_controller,
      window_tree_client_impl()->focus_synchronizer()->active_focus_client());

  window_tree_client_impl()->focus_synchronizer()->SetFocusFromServer(nullptr);
  // At this point the window still has focus. This is because we expect the
  // client to take action that results in clearing focus.
  EXPECT_EQ(
      nullptr,
      window_tree_client_impl()->focus_synchronizer()->active_focus_client());
}

TEST_F(FocusSynchronizerTest, SetFocusFromServerNull) {
  // Create a WindowTreeHostMus and make it active.
  std::unique_ptr<WindowTreeHostMus> window_tree_host =
      std::make_unique<WindowTreeHostMus>(
          CreateInitParamsForTopLevel(window_tree_client_impl()));
  window_tree_host->InitHost();
  wm::FocusController focus_controller(new TestFocusRules);
  client::SetFocusClient(window_tree_host->window(), &focus_controller);
  window_tree_host->Show();
  focus_controller.FocusWindow(window_tree_host->window());
  FocusSynchronizer* focus_synchronizer =
      window_tree_client_impl()->focus_synchronizer();
  focus_synchronizer->SetActiveFocusClient(&focus_controller,
                                           window_tree_host->window());
  EXPECT_EQ(&focus_controller, focus_synchronizer->active_focus_client());
  ASSERT_TRUE(focus_synchronizer->focused_window());
  EXPECT_EQ(focus_synchronizer->focused_window()->GetWindow(),
            window_tree_host->window());

  // Simulate the server clearing focus.
  focus_synchronizer->SetFocusFromServer(nullptr);

  // FocusSynchronizer should no longer think there is a focused window.
  EXPECT_FALSE(focus_synchronizer->focused_window());
}

}  // namespace aura
