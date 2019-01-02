// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/interfaces/constants.mojom.h"
#include "ash/public/interfaces/shell_test_api.mojom.h"
#include "base/run_loop.h"
#include "chrome/browser/ui/ash/tablet_mode_client.h"
#include "chrome/browser/ui/ash/tablet_mode_client_observer.h"
#include "content/public/common/service_manager_connection.h"
#include "services/service_manager/public/cpp/connector.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace test {

namespace {

// A helper used to wait for an expected change to the tablet mode state.
class TestTabletModeClientObserver : public TabletModeClientObserver {
 public:
  explicit TestTabletModeClientObserver(bool target_state)
      : target_state_(target_state) {
    TabletModeClient::Get()->AddObserver(this);
  }

  ~TestTabletModeClientObserver() override {
    TabletModeClient::Get()->RemoveObserver(this);
  }

  void OnTabletModeToggled(bool enabled) override {
    if (enabled == target_state_)
      run_loop_.Quit();
  }

  base::RunLoop* run_loop() { return &run_loop_; }

 private:
  const bool target_state_;
  base::RunLoop run_loop_;

  DISALLOW_COPY_AND_ASSIGN(TestTabletModeClientObserver);
};

}  // namespace

// Enables or disables the tablet mode and waits to until the change has made
// its way back into Chrome (from Ash). Should only be called to toggle the
// current mode.
void SetAndWaitForTabletMode(bool enabled) {
  ASSERT_NE(enabled, TabletModeClient::Get()->tablet_mode_enabled());

  ash::mojom::ShellTestApiPtr shell_test_api;
  content::ServiceManagerConnection::GetForProcess()
      ->GetConnector()
      ->BindInterface(ash::mojom::kServiceName, &shell_test_api);
  shell_test_api->EnableTabletModeWindowManager(enabled);

  TestTabletModeClientObserver observer(enabled);
  observer.run_loop()->Run();

  ASSERT_EQ(enabled, TabletModeClient::Get()->tablet_mode_enabled());
}

}  // namespace test
