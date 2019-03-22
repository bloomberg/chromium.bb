// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/quick_launch/public/mojom/constants.mojom.h"
#include "ash/public/interfaces/constants.mojom.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/run_loop.h"
#include "base/test/scoped_task_environment.h"
#include "services/service_manager/public/cpp/test/test_service.h"
#include "services/service_manager/public/cpp/test/test_service_manager.h"
#include "services/ws/public/mojom/constants.mojom.h"
#include "services/ws/public/mojom/window_server_test.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/ui_base_features.h"
#include "ui/views/layout/layout_provider.h"

namespace ash {

void RunCallback(bool* success, base::RepeatingClosure callback, bool result) {
  *success = result;
  std::move(callback).Run();
}

class AppLaunchTest : public testing::Test {
 public:
  AppLaunchTest()
      : test_service_(
            test_service_manager_.RegisterTestInstance("ash_unittests")) {}
  ~AppLaunchTest() override = default;

 protected:
  service_manager::Connector* connector() { return test_service_.connector(); }

 private:
  void SetUp() override {
    base::CommandLine::ForCurrentProcess()->AppendSwitch("use-test-config");
  }

  base::test::ScopedTaskEnvironment task_environment_;
  service_manager::TestServiceManager test_service_manager_;
  service_manager::TestService test_service_;
  views::LayoutProvider layout_provider_;

  DISALLOW_COPY_AND_ASSIGN(AppLaunchTest);
};

TEST_F(AppLaunchTest, TestQuickLaunch) {
  // This test launches ash in a separate service. That doesn't make sense with
  // SingleProcessMash.
  if (features::IsSingleProcessMash())
    return;

  // TODO(https://crbug.com/904148): These should not use |WarmService()|.
  connector()->WarmService(
      service_manager::ServiceFilter::ByName(mojom::kServiceName));
  connector()->WarmService(service_manager::ServiceFilter::ByName(
      quick_launch::mojom::kServiceName));

  ws::mojom::WindowServerTestPtr test_interface;
  connector()->BindInterface(
      service_manager::ServiceFilter::ByName(ws::mojom::kServiceName),
      mojo::MakeRequest(&test_interface));

  base::RunLoop run_loop;
  bool success = false;
  test_interface->EnsureClientHasDrawnWindow(
      quick_launch::mojom::kServiceName,
      base::BindOnce(&RunCallback, &success, run_loop.QuitClosure()));
  run_loop.Run();
  EXPECT_TRUE(success);
}

}  // namespace ash
