// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/eche_app_ui/eche_recent_app_click_handler.h"

#include <string>

#include "ash/components/phonehub/fake_phone_hub_manager.h"
#include "ash/constants/ash_features.h"
#include "ash/webui/eche_app_ui/fake_feature_status_provider.h"
#include "ash/webui/eche_app_ui/fake_launch_app_helper.h"
#include "ash/webui/eche_app_ui/launch_app_helper.h"
#include "base/bind.h"
#include "base/test/scoped_feature_list.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image.h"

namespace ash {
namespace eche_app {

class EcheRecentAppClickHandlerTest : public testing::Test {
 protected:
  EcheRecentAppClickHandlerTest() = default;
  EcheRecentAppClickHandlerTest(const EcheRecentAppClickHandlerTest&) = delete;
  EcheRecentAppClickHandlerTest& operator=(
      const EcheRecentAppClickHandlerTest&) = delete;
  ~EcheRecentAppClickHandlerTest() override = default;

  // testing::Test:
  void SetUp() override {
    fake_phone_hub_manager_.fake_feature_status_provider()->SetStatus(
        phonehub::FeatureStatus::kEnabledAndConnected);
    fake_feature_status_provider_.SetStatus(FeatureStatus::kIneligible);
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{features::kEcheSWA},
        /*disabled_features=*/{});
    launch_app_helper_ = std::make_unique<FakeLaunchAppHelper>(
        &fake_phone_hub_manager_,
        base::BindRepeating(
            &EcheRecentAppClickHandlerTest::FakeLaunchEcheAppFunction,
            base::Unretained(this)),
        base::BindRepeating(
            &EcheRecentAppClickHandlerTest::FakeCloseEcheAppFunction,
            base::Unretained(this)),
        base::BindRepeating(
            &EcheRecentAppClickHandlerTest::FakeLaunchNotificationFunction,
            base::Unretained(this)));
    handler_ = std::make_unique<EcheRecentAppClickHandler>(
        &fake_phone_hub_manager_, &fake_feature_status_provider_,
        launch_app_helper_.get());
  }

  void TearDown() override {
    launch_app_helper_.reset();
    handler_.reset();
  }

  void FakeLaunchEcheAppFunction(const absl::optional<int64_t>& notification_id,
                                 const std::string& package_name,
                                 const std::u16string& visible_name,
                                 const absl::optional<int64_t>& user_id,
                                 const gfx::Image& icon) {
    package_name_ = package_name;
    visible_name_ = visible_name;
    user_id_ = user_id.value();
  }

  void FakeLaunchNotificationFunction(
      const absl::optional<std::u16string>& title,
      const absl::optional<std::u16string>& message,
      std::unique_ptr<LaunchAppHelper::NotificationInfo> info) {
    num_notifications_shown_++;
  }

  void FakeCloseEcheAppFunction() {
    // Do nothing.
  }

  void SetStatus(FeatureStatus status) {
    fake_feature_status_provider_.SetStatus(status);
  }

  size_t GetNumberOfRecentAppsInteractionHandlers() {
    return fake_phone_hub_manager_.fake_recent_apps_interaction_handler()
        ->recent_app_click_observer_count();
  }

  void RecentAppClicked(
      const phonehub::Notification::AppMetadata& app_metadata) {
    handler_->OnRecentAppClicked(app_metadata);
  }

  void HandleNotificationClick(
      int64_t notification_id,
      const phonehub::Notification::AppMetadata& app_metadata) {
    handler_->HandleNotificationClick(notification_id, app_metadata);
  }

  std::vector<phonehub::Notification::AppMetadata>
  FetchRecentAppMetadataList() {
    return fake_phone_hub_manager_.fake_recent_apps_interaction_handler()
        ->FetchRecentAppMetadataList();
  }

  void SetAppLaunchProhibitedReason(
      LaunchAppHelper::AppLaunchProhibitedReason reason) {
    launch_app_helper_->SetAppLaunchProhibitedReason(reason);
  }

  void reset() { num_notifications_shown_ = 0; }

  const std::string& get_package_name() { return package_name_; }

  const std::u16string& get_visible_name() { return visible_name_; }

  int64_t get_user_id() { return user_id_; }

  size_t num_notifications_shown() { return num_notifications_shown_; }

 private:
  phonehub::FakePhoneHubManager fake_phone_hub_manager_;
  base::test::ScopedFeatureList scoped_feature_list_;
  FakeFeatureStatusProvider fake_feature_status_provider_;
  std::unique_ptr<FakeLaunchAppHelper> launch_app_helper_;
  std::unique_ptr<EcheRecentAppClickHandler> handler_;
  std::string package_name_;
  std::u16string visible_name_;
  int64_t user_id_;
  size_t num_notifications_shown_ = 0;
};

TEST_F(EcheRecentAppClickHandlerTest, StatusChangeTransitions) {
  EXPECT_EQ(0u, GetNumberOfRecentAppsInteractionHandlers());
  SetStatus(FeatureStatus::kDisabled);
  EXPECT_EQ(0u, GetNumberOfRecentAppsInteractionHandlers());
  SetStatus(FeatureStatus::kDisconnected);
  EXPECT_EQ(1u, GetNumberOfRecentAppsInteractionHandlers());
  SetStatus(FeatureStatus::kConnecting);
  EXPECT_EQ(1u, GetNumberOfRecentAppsInteractionHandlers());
  SetStatus(FeatureStatus::kConnected);
  EXPECT_EQ(1u, GetNumberOfRecentAppsInteractionHandlers());
  SetStatus(FeatureStatus::kIneligible);
  EXPECT_EQ(0u, GetNumberOfRecentAppsInteractionHandlers());
  SetStatus(FeatureStatus::kDisconnected);
  EXPECT_EQ(1u, GetNumberOfRecentAppsInteractionHandlers());
  SetStatus(FeatureStatus::kDisabled);
  EXPECT_EQ(0u, GetNumberOfRecentAppsInteractionHandlers());
  SetStatus(FeatureStatus::kDisconnected);
  EXPECT_EQ(1u, GetNumberOfRecentAppsInteractionHandlers());
  SetStatus(FeatureStatus::kDependentFeature);
  EXPECT_EQ(0u, GetNumberOfRecentAppsInteractionHandlers());
  SetStatus(FeatureStatus::kDisconnected);
  EXPECT_EQ(1u, GetNumberOfRecentAppsInteractionHandlers());
  SetStatus(FeatureStatus::kDependentFeaturePending);
  EXPECT_EQ(0u, GetNumberOfRecentAppsInteractionHandlers());
}

TEST_F(EcheRecentAppClickHandlerTest, LaunchEcheAppFunction) {
  const int64_t user_id = 1;
  const char16_t app_visible_name[] = u"Fake App";
  const char package_name[] = "com.fakeapp";
  auto fake_app_metadata = phonehub::Notification::AppMetadata(
      app_visible_name, package_name, gfx::Image(),
      /*icon_color=*/absl::nullopt, /*icon_is_monochrome=*/true, user_id);

  RecentAppClicked(fake_app_metadata);

  EXPECT_EQ(fake_app_metadata.package_name, get_package_name());
  EXPECT_EQ(fake_app_metadata.visible_app_name, get_visible_name());
  EXPECT_EQ(fake_app_metadata.user_id, get_user_id());
}

TEST_F(EcheRecentAppClickHandlerTest, HandleNotificationClick) {
  const int64_t notification_id = 1;
  const int64_t user_id = 1;
  const char16_t app_visible_name[] = u"Fake App";
  const char package_name[] = "com.fakeapp";
  auto fake_app_metadata = phonehub::Notification::AppMetadata(
      app_visible_name, package_name, gfx::Image(),
      /*icon_color=*/absl::nullopt, /*icon_is_monochrome=*/true, user_id);

  HandleNotificationClick(notification_id, fake_app_metadata);
  std::vector<phonehub::Notification::AppMetadata> app_metadata =
      FetchRecentAppMetadataList();

  EXPECT_EQ(fake_app_metadata.visible_app_name,
            app_metadata[0].visible_app_name);
  EXPECT_EQ(fake_app_metadata.package_name, app_metadata[0].package_name);
  EXPECT_EQ(fake_app_metadata.user_id, app_metadata[0].user_id);
}

TEST_F(EcheRecentAppClickHandlerTest,
       HandleRecentAppClickWithProhibitedReason) {
  const int64_t user_id = 1;
  const char16_t app_visible_name[] = u"Fake App";
  const char package_name[] = "com.fakeapp";
  auto fake_app_metadata = phonehub::Notification::AppMetadata(
      app_visible_name, package_name, gfx::Image(),
      /*icon_color=*/absl::nullopt, /*icon_is_monochrome=*/true, user_id);

  SetAppLaunchProhibitedReason(
      LaunchAppHelper::AppLaunchProhibitedReason::kDisabledByScreenLock);
  RecentAppClicked(fake_app_metadata);
  EXPECT_EQ(num_notifications_shown(), 1u);
}

}  // namespace eche_app
}  // namespace ash
