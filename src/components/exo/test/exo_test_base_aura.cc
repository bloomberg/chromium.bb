// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/test/exo_test_base_aura.h"

#include "components/exo/wm_helper.h"
#include "ui/base/ime/input_method_factory.h"
#include "ui/compositor/test/context_factories_for_test.h"
#include "ui/display/manager/managed_display_info.h"
#include "ui/wm/core/default_activation_client.h"
#include "ui/wm/core/wm_core_switches.h"

namespace exo {
namespace test {

namespace {

class WMHelperTester : public WMHelper {
 public:
  WMHelperTester(aura::Env* env, aura::Window* root_window)
      : env_(env), root_window_(root_window) {}
  ~WMHelperTester() override {}

  // Overridden from WMHelper
  aura::Env* env() override { return env_; }
  void AddActivationObserver(wm::ActivationChangeObserver* observer) override {}
  void RemoveActivationObserver(
      wm::ActivationChangeObserver* observer) override {}
  void AddFocusObserver(aura::client::FocusChangeObserver* observer) override {}
  void RemoveFocusObserver(
      aura::client::FocusChangeObserver* observer) override {}
  void AddDragDropObserver(DragDropObserver* observer) override {}
  void RemoveDragDropObserver(DragDropObserver* observer) override {}
  void SetDragDropDelegate(aura::Window*) override {}
  void ResetDragDropDelegate(aura::Window*) override {}
  void AddVSyncObserver(
      ui::CompositorVSyncManager::Observer* observer) override {}
  void RemoveVSyncObserver(
      ui::CompositorVSyncManager::Observer* observer) override {}

  const display::ManagedDisplayInfo& GetDisplayInfo(
      int64_t display_id) const override {
    static display::ManagedDisplayInfo md;
    return md;
  }
  const std::vector<uint8_t>& GetDisplayIdentificationData(
      int64_t display_id) const override {
    static std::vector<uint8_t> no_data;
    return no_data;
  }

  aura::Window* GetPrimaryDisplayContainer(int container_id) override {
    return root_window_;
  }
  aura::Window* GetActiveWindow() const override { return nullptr; }
  aura::Window* GetFocusedWindow() const override { return nullptr; }
  aura::Window* GetRootWindowForNewWindows() const override {
    return root_window_;
  }
  aura::client::CursorClient* GetCursorClient() override { return nullptr; }
  void AddPreTargetHandler(ui::EventHandler* handler) override {}
  void PrependPreTargetHandler(ui::EventHandler* handler) override {}
  void RemovePreTargetHandler(ui::EventHandler* handler) override {}
  void AddPostTargetHandler(ui::EventHandler* handler) override {}
  void RemovePostTargetHandler(ui::EventHandler* handler) override {}
  bool IsTabletModeWindowManagerEnabled() const override { return false; }
  double GetDefaultDeviceScaleFactor() const override { return 1.0; }

  LifetimeManager* GetLifetimeManager() override { return &lifetime_manager_; }
  aura::client::CaptureClient* GetCaptureClient() override { return nullptr; }

  // Overridden from aura::client::DragDropDelegate:
  void OnDragEntered(const ui::DropTargetEvent& event) override {}
  int OnDragUpdated(const ui::DropTargetEvent& event) override { return 0; }
  void OnDragExited() override {}
  int OnPerformDrop(const ui::DropTargetEvent& event) override { return 0; }

 private:
  aura::Env* const env_;
  aura::Window* root_window_;
  LifetimeManager lifetime_manager_;

  DISALLOW_COPY_AND_ASSIGN(WMHelperTester);
};

}  // namespace

ExoTestBaseAura::ExoTestBaseAura() {}
ExoTestBaseAura::~ExoTestBaseAura() {}

void ExoTestBaseAura::SetUp() {
  ui::SetUpInputMethodFactoryForTesting();
  aura::test::AuraTestBase::SetUp();
  // Takes care of its own lifetime.
  new wm::DefaultActivationClient(root_window());

  wm_helper_ =
      std::make_unique<WMHelperTester>(aura::Env::GetInstance(), root_window());
  WMHelper::SetInstance(wm_helper_.get());
}

void ExoTestBaseAura::TearDown() {
  WMHelper::SetInstance(nullptr);
  wm_helper_.reset();

  aura::test::AuraTestBase::TearDown();
}

}  // namespace test
}  // namespace exo
