// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/ash/os_apps_page/app_notification_handler.h"

#include <memory>
#include <utility>
#include <vector>

#include "ash/public/cpp/message_center_ash.h"
#include "base/logging.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/ui/webui/settings/ash/os_apps_page/mojom/app_notification_handler.mojom.h"
#include "chrome/test/base/testing_profile.h"
#include "components/services/app_service/public/cpp/app_registry_cache.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/services/app_service/public/cpp/features.h"
#include "components/services/app_service/public/cpp/permission.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace settings {

namespace {

class FakeMessageCenterAsh : public ash::MessageCenterAsh {
 public:
  FakeMessageCenterAsh() = default;
  ~FakeMessageCenterAsh() override = default;

  // MessageCenterAsh override:
  void SetQuietMode(bool in_quiet_mode) override {
    if (in_quiet_mode != in_quiet_mode_) {
      in_quiet_mode_ = in_quiet_mode;
      NotifyOnQuietModeChanged(in_quiet_mode);
    }
  }

  bool IsQuietMode() const override { return in_quiet_mode_; }

 private:
  bool in_quiet_mode_ = false;
};

class AppNotificationHandlerTestObserver
    : public app_notification::mojom::AppNotificationsObserver {
 public:
  AppNotificationHandlerTestObserver() {}
  ~AppNotificationHandlerTestObserver() override {}

  void OnNotificationAppChanged(app_notification::mojom::AppPtr app) override {
    recently_updated_app_ = std::move(app);
    app_list_changed_++;
  }

  void OnQuietModeChanged(bool enabled) override {
    is_quiet_mode_ = enabled;
    quiet_mode_changed_++;
  }

  mojo::PendingRemote<app_notification::mojom::AppNotificationsObserver>
  GenerateRemote() {
    return receiver_.BindNewPipeAndPassRemote();
  }

  const std::vector<app_notification::mojom::AppPtr>& apps() { return apps_; }
  const app_notification::mojom::AppPtr& recently_updated_app() {
    return recently_updated_app_;
  }

  bool is_quiet_mode() { return is_quiet_mode_; }

  int app_list_changed() { return app_list_changed_; }
  int quiet_mode_changed() { return quiet_mode_changed_; }

 private:
  std::vector<app_notification::mojom::AppPtr> apps_;
  app_notification::mojom::AppPtr recently_updated_app_;
  bool is_quiet_mode_ = false;

  int app_list_changed_ = 0;
  int quiet_mode_changed_ = 0;

  mojo::Receiver<app_notification::mojom::AppNotificationsObserver> receiver_{
      this};
};

}  // namespace

class AppNotificationHandlerTest : public testing::Test {
 public:
  AppNotificationHandlerTest()
      : task_environment_(content::BrowserTaskEnvironment::REAL_IO_THREAD),
        profile_(std::make_unique<TestingProfile>()) {}
  ~AppNotificationHandlerTest() override = default;

  void SetUp() override {
    ash::MessageCenterAsh::SetForTesting(&message_center_ash_);
    app_service_proxy_ =
        std::make_unique<apps::AppServiceProxy>(profile_.get());
    handler_ =
        std::make_unique<AppNotificationHandler>(app_service_proxy_.get());

    observer_ = std::make_unique<AppNotificationHandlerTestObserver>();
    handler_->AddObserver(observer_->GenerateRemote());
  }

  void TearDown() override {
    handler_.reset();
    app_service_proxy_.reset();
    ash::MessageCenterAsh::SetForTesting(nullptr);
  }

 protected:
  AppNotificationHandlerTestObserver* observer() { return observer_.get(); }

  void SetQuietModeState(bool quiet_mode_enabled) {
    handler_->SetQuietMode(quiet_mode_enabled);
  }

  void CreateAndStoreFakeApp(std::string fake_id,
                             apps::AppType app_type,
                             apps::PermissionType permission_type,
                             bool permission_value = true) {
    apps::PermissionValuePtr fake_permission_value =
        std::make_unique<apps::PermissionValue>(permission_value);
    apps::PermissionPtr fake_permission = std::make_unique<apps::Permission>(
        permission_type, std::move(fake_permission_value),
        /*is_managed=*/false);
    std::vector<apps::PermissionPtr> fake_permissions;
    fake_permissions.push_back(std::move(fake_permission));

    std::vector<apps::AppPtr> fake_apps;
    apps::AppPtr fake_app = std::make_unique<apps::App>(app_type, fake_id);
    fake_app->show_in_management = true;
    fake_app->readiness = apps::Readiness::kReady;
    fake_app->permissions = std::move(fake_permissions);

    fake_apps.push_back(std::move(fake_app));

    UpdateAppRegistryCache(fake_apps, app_type);
  }

  void UpdateAppRegistryCache(std::vector<apps::AppPtr>& fake_apps,
                              apps::AppType app_type) {
    if (base::FeatureList::IsEnabled(
            apps::kAppServiceOnAppUpdateWithoutMojom)) {
      app_service_proxy_->AppRegistryCache().OnApps(std::move(fake_apps),
                                                    app_type, false);
    } else {
      std::vector<apps::mojom::AppPtr> mojom_apps;
      mojom_apps.push_back(apps::ConvertAppToMojomApp(fake_apps[0]));
      app_service_proxy_->AppRegistryCache().OnApps(
          std::move(mojom_apps), apps::mojom::AppType::kUnknown,
          /*should_notify_initialized=*/false);
    }
  }

  bool CheckIfFakeAppInList(std::string fake_id) {
    for (app_notification::mojom::AppPtr const& app : observer_->apps()) {
      if (app->id.compare(fake_id) == 0)
        return true;
    }
    return false;
  }

 private:
  std::unique_ptr<AppNotificationHandler> handler_;
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<apps::AppServiceProxy> app_service_proxy_;
  FakeMessageCenterAsh message_center_ash_;
  std::unique_ptr<AppNotificationHandlerTestObserver> observer_;
};

// Tests for update of in_quiet_mode_ variable by MessageCenterAsh observer
// OnQuietModeChange() after quiet mode state change between true and false.
TEST_F(AppNotificationHandlerTest, TestOnQuietModeChanged) {
  ash::MessageCenterAsh::Get()->SetQuietMode(true);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(observer()->is_quiet_mode());
  EXPECT_EQ(observer()->quiet_mode_changed(), 1);

  ash::MessageCenterAsh::Get()->SetQuietMode(false);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(observer()->is_quiet_mode());
  EXPECT_EQ(observer()->quiet_mode_changed(), 2);
}

// Tests for update of in_quiet_mode_ variable after setting state
// with MessageCenterAsh SetQuietMode() true and false.
TEST_F(AppNotificationHandlerTest, TestSetQuietMode) {
  SetQuietModeState(true);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(observer()->is_quiet_mode());
  EXPECT_EQ(observer()->quiet_mode_changed(), 1);

  SetQuietModeState(false);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(observer()->is_quiet_mode());
  EXPECT_EQ(observer()->quiet_mode_changed(), 2);
}

// Tests notifying observers with only kArc and kWeb apps that have the
// NOTIFICATIONS permission.
TEST_F(AppNotificationHandlerTest, TestAppListUpdated) {
  CreateAndStoreFakeApp("arcAppWithNotifications", apps::AppType::kArc,
                        apps::PermissionType::kNotifications,
                        /*permission_value=*/true);

  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(observer()->app_list_changed(), 1);
  EXPECT_EQ("arcAppWithNotifications", observer()->recently_updated_app()->id);
  EXPECT_TRUE(observer()
                  ->recently_updated_app()
                  ->notification_permission->value->bool_value.value());

  CreateAndStoreFakeApp("webAppWithNotifications", apps::AppType::kWeb,
                        apps::PermissionType::kNotifications,
                        /*permission_value=*/true);

  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(observer()->app_list_changed(), 2);
  EXPECT_EQ("webAppWithNotifications", observer()->recently_updated_app()->id);
  EXPECT_TRUE(observer()
                  ->recently_updated_app()
                  ->notification_permission->value->bool_value.value());

  CreateAndStoreFakeApp("arcAppWithCamera", apps::AppType::kArc,
                        apps::PermissionType::kCamera);

  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(observer()->app_list_changed(), 2);

  CreateAndStoreFakeApp("webAppWithGeolocation", apps::AppType::kWeb,
                        apps::PermissionType::kLocation);

  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(observer()->app_list_changed(), 2);

  CreateAndStoreFakeApp("pluginVmAppWithPrinting", apps::AppType::kPluginVm,
                        apps::PermissionType::kPrinting);

  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(observer()->app_list_changed(), 2);

  CreateAndStoreFakeApp("arcAppWithNotifications", apps::AppType::kArc,
                        apps::PermissionType::kNotifications,
                        /*permission_value=*/false);

  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(observer()->app_list_changed(), 3);
  EXPECT_EQ("arcAppWithNotifications", observer()->recently_updated_app()->id);
  EXPECT_FALSE(observer()
                   ->recently_updated_app()
                   ->notification_permission->value->bool_value.value());

  CreateAndStoreFakeApp("webAppWithNotifications", apps::AppType::kWeb,
                        apps::PermissionType::kNotifications,
                        /*permission_value=*/false);

  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(observer()->app_list_changed(), 4);
  EXPECT_EQ("webAppWithNotifications", observer()->recently_updated_app()->id);
  EXPECT_FALSE(observer()
                   ->recently_updated_app()
                   ->notification_permission->value->bool_value.value());
}

}  // namespace settings
}  // namespace chromeos
