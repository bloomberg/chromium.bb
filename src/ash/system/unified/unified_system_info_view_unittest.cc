// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/unified/unified_system_info_view.h"

#include "ash/public/cpp/ash_features.h"
#include "ash/session/session_controller_impl.h"
#include "ash/session/test_session_controller_client.h"
#include "ash/shell.h"
#include "ash/system/model/enterprise_domain_model.h"
#include "ash/system/model/system_tray_model.h"
#include "ash/system/unified/unified_system_tray_controller.h"
#include "ash/system/unified/unified_system_tray_model.h"
#include "ash/test/ash_test_base.h"
#include "base/test/scoped_feature_list.h"

namespace ash {

class UnifiedSystemInfoViewTest : public AshTestBase,
                                  public testing::WithParamInterface<bool> {
 public:
  UnifiedSystemInfoViewTest() = default;
  ~UnifiedSystemInfoViewTest() override = default;

  void SetUp() override {
    AshTestBase::SetUp();

    scoped_feature_list_ = std::make_unique<base::test::ScopedFeatureList>();
    scoped_feature_list_->InitWithFeatureState(
        features::kManagedDeviceUIRedesign, IsManagedDeviceUIRedesignEnabled());

    model_ = std::make_unique<UnifiedSystemTrayModel>(nullptr);
    controller_ = std::make_unique<UnifiedSystemTrayController>(model_.get());
    info_view_ = std::make_unique<UnifiedSystemInfoView>(controller_.get());
  }

  bool IsManagedDeviceUIRedesignEnabled() const { return GetParam(); }

  void TearDown() override {
    info_view_.reset();
    controller_.reset();
    model_.reset();
    AshTestBase::TearDown();
  }

 protected:
  UnifiedSystemInfoView* info_view() { return info_view_.get(); }

 private:
  std::unique_ptr<UnifiedSystemTrayModel> model_;
  std::unique_ptr<UnifiedSystemTrayController> controller_;
  std::unique_ptr<UnifiedSystemInfoView> info_view_;
  std::unique_ptr<base::test::ScopedFeatureList> scoped_feature_list_;

  DISALLOW_COPY_AND_ASSIGN(UnifiedSystemInfoViewTest);
};

INSTANTIATE_TEST_SUITE_P(
    All,
    UnifiedSystemInfoViewTest,
    testing::Bool() /* IsManagedDeviceUIRedesignEnabled() */);

TEST_P(UnifiedSystemInfoViewTest, EnterpriseManagedVisible) {
  // By default, EnterpriseManagedView is not shown.
  EXPECT_FALSE(info_view()->enterprise_managed_->GetVisible());

  // Simulate enterprise information becoming available.
  const bool active_directory = false;
  Shell::Get()
      ->system_tray_model()
      ->enterprise_domain()
      ->SetEnterpriseDomainInfo("example.com", active_directory);

  // EnterpriseManagedView should be shown.
  EXPECT_TRUE(info_view()->enterprise_managed_->GetVisible());
}

TEST_P(UnifiedSystemInfoViewTest, EnterpriseManagedVisibleForActiveDirectory) {
  // Active directory information becoming available.
  const std::string empty_domain;
  const bool active_directory = true;
  Shell::Get()
      ->system_tray_model()
      ->enterprise_domain()
      ->SetEnterpriseDomainInfo(empty_domain, active_directory);

  // EnterpriseManagedView should be shown.
  EXPECT_TRUE(info_view()->enterprise_managed_->GetVisible());
}

TEST_P(UnifiedSystemInfoViewTest, EnterpriseUserManagedVisible) {
  // By default, EnterpriseManagedView is not shown.
  EXPECT_FALSE(info_view()->enterprise_managed_->GetVisible());

  // Simulate enterprise information becoming available.
  Shell::Get()
      ->system_tray_model()
      ->enterprise_domain()
      ->SetEnterpriseAccountDomainInfo("example.com");

  // EnterpriseManagedView should be shown if the feature is enabled.
  EXPECT_EQ(IsManagedDeviceUIRedesignEnabled(),
            info_view()->enterprise_managed_->GetVisible());
}

using UnifiedSystemInfoViewNoSessionTest = NoSessionAshTestBase;

TEST_F(UnifiedSystemInfoViewNoSessionTest, ChildVisible) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(features::kManagedDeviceUIRedesign);
  std::unique_ptr<UnifiedSystemTrayModel> model_ =
      std::make_unique<UnifiedSystemTrayModel>(nullptr);
  std::unique_ptr<UnifiedSystemTrayController> controller_ =
      std::make_unique<UnifiedSystemTrayController>(model_.get());

  SessionControllerImpl* session = Shell::Get()->session_controller();
  ASSERT_FALSE(session->IsActiveUserSessionStarted());

  // Before login the supervised user view is invisible.
  std::unique_ptr<UnifiedSystemInfoView> info_view_;
  info_view_ = std::make_unique<UnifiedSystemInfoView>(controller_.get());
  EXPECT_FALSE(info_view_->supervised_->GetVisible());
  info_view_.reset();

  // Simulate a supervised user logging in.
  TestSessionControllerClient* client = GetSessionControllerClient();
  client->Reset();
  client->AddUserSession("child@test.com", user_manager::USER_TYPE_CHILD);
  client->SetSessionState(session_manager::SessionState::ACTIVE);
  UserSession user_session = *session->GetUserSession(0);
  user_session.custodian_email = "parent@test.com";
  session->UpdateUserSession(std::move(user_session));

  // Now the supervised user view is visible.
  info_view_ = std::make_unique<UnifiedSystemInfoView>(controller_.get());
  ASSERT_TRUE(info_view_->supervised_->GetVisible());
}

}  // namespace ash
