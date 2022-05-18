// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/dbus/dlp_files_policy_service_provider.h"

#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_rules_manager.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_rules_manager_factory.h"
#include "chrome/browser/chromeos/policy/dlp/mock_dlp_rules_manager.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/ash/components/dbus/services/service_provider_test_helper.h"
#include "chromeos/dbus/dlp/dlp_service.pb.h"
#include "components/user_manager/scoped_user_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "dbus/object_path.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/dlp/dbus-constants.h"

namespace ash {

namespace {
constexpr char kEmailId[] = "test@example.com";
constexpr char kGaiaId[] = "12345";

constexpr char kExampleUrl[] = "https://example.com";
constexpr char kExampleUrl2[] = "https://example2.com";
}  // namespace

class DlpFilesPolicyServiceProviderTest
    : public testing::TestWithParam<policy::DlpRulesManager::Level> {
 protected:
  DlpFilesPolicyServiceProviderTest()
      : profile_(std::make_unique<TestingProfile>()),
        user_manager_(new ash::FakeChromeUserManager()),
        scoped_user_manager_(base::WrapUnique(user_manager_)),
        dlp_policy_service_(std::make_unique<DlpFilesPolicyServiceProvider>()) {
  }

  DlpFilesPolicyServiceProviderTest(const DlpFilesPolicyServiceProviderTest&) =
      delete;
  DlpFilesPolicyServiceProviderTest& operator=(
      const DlpFilesPolicyServiceProviderTest&) = delete;

  ~DlpFilesPolicyServiceProviderTest() override {
    dbus_service_test_helper_.TearDown();
  }

  void SetUp() override {
    AccountId account_id = AccountId::FromUserEmailGaiaId(kEmailId, kGaiaId);
    profile_->SetIsNewProfile(true);
    user_manager::User* user =
        user_manager_->AddUserWithAffiliationAndTypeAndProfile(
            account_id, /*is_affiliated=*/false,
            user_manager::USER_TYPE_REGULAR, profile_.get());
    user_manager_->UserLoggedIn(account_id, user->username_hash(),
                                /*browser_restart=*/false,
                                /*is_child=*/false);

    policy::DlpRulesManagerFactory::GetInstance()->SetTestingFactory(
        profile_.get(),
        base::BindRepeating(
            &DlpFilesPolicyServiceProviderTest::SetDlpRulesManager,
            base::Unretained(this)));
    ASSERT_TRUE(policy::DlpRulesManagerFactory::GetForPrimaryProfile());
    ASSERT_TRUE(mock_rules_manager_);
  }

  template <class ResponseProtoType>
  absl::optional<ResponseProtoType> CallDlpFilesPolicyServiceMethod(
      const char* method_name,
      const google::protobuf::MessageLite& request) {
    dbus::MethodCall method_call(dlp::kDlpFilesPolicyServiceInterface,
                                 method_name);
    dbus::MessageWriter writer(&method_call);
    writer.AppendProtoAsArrayOfBytes(request);

    dbus_service_test_helper_.SetUp(
        dlp::kDlpFilesPolicyServiceName,
        dbus::ObjectPath(dlp::kDlpFilesPolicyServicePath),
        dlp::kDlpFilesPolicyServiceInterface, method_name,
        dlp_policy_service_.get());
    std::unique_ptr<dbus::Response> dbus_response =
        dbus_service_test_helper_.CallMethod(&method_call);

    if (!dbus_response)
      return {};

    dbus::MessageReader reader(dbus_response.get());
    ResponseProtoType response;
    if (!reader.PopArrayOfBytesAsProto(&response))
      return {};
    return response;
  }

  std::unique_ptr<KeyedService> SetDlpRulesManager(
      content::BrowserContext* context) {
    auto dlp_rules_manager =
        std::make_unique<testing::StrictMock<policy::MockDlpRulesManager>>();
    mock_rules_manager_ = dlp_rules_manager.get();
    return dlp_rules_manager;
  }

  content::BrowserTaskEnvironment task_environment_;

  policy::MockDlpRulesManager* mock_rules_manager_ = nullptr;

  const std::unique_ptr<TestingProfile> profile_;
  ash::FakeChromeUserManager* user_manager_;
  user_manager::ScopedUserManager scoped_user_manager_;

  std::unique_ptr<DlpFilesPolicyServiceProvider> dlp_policy_service_;
  ServiceProviderTestHelper dbus_service_test_helper_;
};

INSTANTIATE_TEST_SUITE_P(
    DlpFilesPolicyServiceProvider,
    DlpFilesPolicyServiceProviderTest,
    ::testing::Values(policy::DlpRulesManager::Level::kAllow,
                      policy::DlpRulesManager::Level::kBlock));

TEST_P(DlpFilesPolicyServiceProviderTest, IsRestricted) {
  dlp::IsRestrictedRequest request;
  request.add_source_urls(kExampleUrl);
  request.set_destination_url(kExampleUrl2);

  policy::DlpRulesManager::Level level = GetParam();
  EXPECT_CALL(
      *mock_rules_manager_,
      IsRestrictedDestination(GURL(kExampleUrl), GURL(kExampleUrl2),
                              policy::DlpRulesManager::Restriction::kFiles,
                              testing::_, testing::_))
      .WillOnce(testing::Return(level));

  auto response = CallDlpFilesPolicyServiceMethod<dlp::IsRestrictedResponse>(
      dlp::kDlpFilesPolicyServiceIsRestrictedMethod, request);
  ASSERT_TRUE(response.has_value());
  ASSERT_TRUE(response->has_restricted());
  EXPECT_EQ(response->restricted(),
            (level == policy::DlpRulesManager::Level::kBlock));
}

TEST_P(DlpFilesPolicyServiceProviderTest, IsDlpPolicyMatched) {
  dlp::IsDlpPolicyMatchedRequest request;
  request.set_source_url(kExampleUrl);

  policy::DlpRulesManager::Level level = GetParam();
  EXPECT_CALL(
      *mock_rules_manager_,
      IsRestrictedByAnyRule(GURL(kExampleUrl),
                            policy::DlpRulesManager::Restriction::kFiles))
      .WillOnce(testing::Return(level));

  auto response =
      CallDlpFilesPolicyServiceMethod<dlp::IsDlpPolicyMatchedResponse>(
          dlp::kDlpFilesPolicyServiceIsDlpPolicyMatchedMethod, request);
  ASSERT_TRUE(response.has_value());
  ASSERT_TRUE(response->has_restricted());
  EXPECT_EQ(response->restricted(),
            (level == policy::DlpRulesManager::Level::kBlock));
}

}  // namespace ash
