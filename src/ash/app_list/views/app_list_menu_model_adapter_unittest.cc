// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/views/app_list_menu_model_adapter.h"

#include "ash/public/cpp/app_menu_constants.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/test/views_test_base.h"

namespace app_list {

namespace {

class MockAppListMenuModelAdapterDelegate
    : public AppListMenuModelAdapter::Delegate {
 public:
  MockAppListMenuModelAdapterDelegate() = default;
  virtual ~MockAppListMenuModelAdapterDelegate() = default;

  void ExecuteCommand(int command_id, int event_flags) override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(MockAppListMenuModelAdapterDelegate);
};

}  // namespace

class AppListMenuModelAdapterTest : public views::ViewsTestBase {
 public:
  AppListMenuModelAdapterTest() {}
  ~AppListMenuModelAdapterTest() override = default;

  void SetUp() override {
    views::ViewsTestBase::SetUp();
    mock_app_list_menu_model_adapter_delegate_ =
        std::make_unique<MockAppListMenuModelAdapterDelegate>();
    app_list_menu_model_adapter_ = std::make_unique<AppListMenuModelAdapter>(
        "test-app-id", nullptr, ui::MenuSourceType::MENU_SOURCE_TYPE_LAST,
        mock_app_list_menu_model_adapter_delegate_.get(),
        AppListMenuModelAdapter::FULLSCREEN_APP_GRID, base::OnceClosure());
  }

  std::unique_ptr<AppListMenuModelAdapter> app_list_menu_model_adapter_;

 private:
  std::unique_ptr<MockAppListMenuModelAdapterDelegate>
      mock_app_list_menu_model_adapter_delegate_;

  DISALLOW_COPY_AND_ASSIGN(AppListMenuModelAdapterTest);
};

// Tests that NOTIFICATION_CONTAINER is enabled. This ensures that the
// container is able to handle gesture events.
TEST_F(AppListMenuModelAdapterTest, NotificationContainerEnabled) {
  EXPECT_TRUE(app_list_menu_model_adapter_->IsCommandEnabled(
      ash::NOTIFICATION_CONTAINER));
}

}  // namespace app_list
