// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/browser/android/policy_cache_updater_android.h"

#include <jni.h>
#include <memory>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/memory/raw_ptr.h"
#include "base/test/task_environment.h"
#include "base/values.h"
#include "components/policy/android/test_jni_headers/PolicyCacheUpdaterTestSupporter_jni.h"
#include "components/policy/core/browser/configuration_policy_handler.h"
#include "components/policy/core/browser/policy_conversions_client.h"
#include "components/policy/core/browser/policy_error_map.h"
#include "components/policy/core/common/configuration_policy_provider.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_service_impl.h"
#include "components/strings/grit/components_strings.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::Return;

namespace policy {
namespace android {
namespace {
// The list of policeis that can be cached is controlled by Java library. Hence
// we use real policy name for testing.
constexpr char kPolicyName[] = "BrowserSignin";
constexpr int kPolicyValue = 1;

class StubPolicyHandler : public ConfigurationPolicyHandler {
 public:
  StubPolicyHandler(const std::string& policy_name, bool has_error)
      : policy_name_(policy_name), has_error_(has_error) {}
  StubPolicyHandler(const StubPolicyHandler&) = delete;
  StubPolicyHandler& operator=(const StubPolicyHandler&) = delete;
  ~StubPolicyHandler() override = default;

  bool CheckPolicySettings(const PolicyMap& policies,
                           PolicyErrorMap* errors) override {
    if (has_error_) {
      errors->AddError(policy_name_, IDS_POLICY_BLOCKED);
    }
    return policies.GetValue(kPolicyName) && !has_error_;
  }

 private:
  void ApplyPolicySettings(const PolicyMap& policies,
                           PrefValueMap* prefs) override {}
  std::string policy_name_;
  bool has_error_;
};

}  // namespace

class PolicyCacheUpdaterAndroidTest : public ::testing::Test {
 public:
  PolicyCacheUpdaterAndroidTest() {
    policy_provider_.SetDefaultReturns(
        true /* is_initialization_complete_return */,
        true /* is_first_policy_load_complete_return */);
    j_support_ = Java_PolicyCacheUpdaterTestSupporter_Constructor(env_);
    policy_service_ = std::make_unique<policy::PolicyServiceImpl>(
        std::vector<ConfigurationPolicyProvider*>({&policy_provider_}));
    policy_handler_list_ = std::make_unique<ConfigurationPolicyHandlerList>(
        ConfigurationPolicyHandlerList::
            PopulatePolicyHandlerParametersCallback(),
        GetChromePolicyDetailsCallback(),
        /*allow_future_policies=*/false);
  }
  ~PolicyCacheUpdaterAndroidTest() override = default;

  void SetPolicy(const std::string& policy, int policy_value) {
    policy_map_.Set(policy, PolicyLevel::POLICY_LEVEL_MANDATORY,
                    PolicyScope::POLICY_SCOPE_MACHINE,
                    PolicySource::POLICY_SOURCE_PLATFORM,
                    base::Value(policy_value),
                    /*external_data_fetcher=*/nullptr);
  }

  void UpdatePolicy() { policy_provider_.UpdateChromePolicy(policy_map_); }

  void VerifyPolicyName(const std::string& policy,
                        bool has_value,
                        int expected_value) {
    Java_PolicyCacheUpdaterTestSupporter_verifyPolicyCacheIntValue(
        env_, j_support_, base::android::ConvertUTF8ToJavaString(env_, policy),
        has_value, expected_value);
  }

  ConfigurationPolicyHandlerList* policy_handler_list() {
    return policy_handler_list_.get();
  }

  PolicyService* policy_service() { return policy_service_.get(); }

  PolicyMap* policy_map() { return &policy_map_; }

 private:
  raw_ptr<JNIEnv> env_ = base::android::AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jobject> j_support_;
  PolicyMap policy_map_;
  testing::NiceMock<MockConfigurationPolicyProvider> policy_provider_;
  std::unique_ptr<PolicyService> policy_service_;
  std::unique_ptr<ConfigurationPolicyHandlerList> policy_handler_list_;
  base::test::SingleThreadTaskEnvironment task_environment_;
};

TEST_F(PolicyCacheUpdaterAndroidTest, TestCachePolicy) {
  policy_handler_list()->AddHandler(
      std::make_unique<StubPolicyHandler>(kPolicyName, /*has_error=*/false));

  PolicyCacheUpdater updater(policy_service(), policy_handler_list());
  SetPolicy(kPolicyName, kPolicyValue);
  UpdatePolicy();
  VerifyPolicyName(kPolicyName, /*has_value=*/true, kPolicyValue);
}

TEST_F(PolicyCacheUpdaterAndroidTest, TestPolicyNotExist) {
  policy_handler_list()->AddHandler(
      std::make_unique<StubPolicyHandler>(kPolicyName, /*has_error=*/false));

  PolicyCacheUpdater updater(policy_service(), policy_handler_list());
  UpdatePolicy();
  VerifyPolicyName(kPolicyName, /*has_value=*/false, kPolicyValue);
}

TEST_F(PolicyCacheUpdaterAndroidTest, TestPolicyErrorPolicy) {
  policy_handler_list()->AddHandler(
      std::make_unique<StubPolicyHandler>(kPolicyName, /*has_error=*/true));

  PolicyCacheUpdater updater(policy_service(), policy_handler_list());
  SetPolicy(kPolicyName, kPolicyValue);
  UpdatePolicy();
  VerifyPolicyName(kPolicyName, /*has_value=*/false, kPolicyValue);
}

TEST_F(PolicyCacheUpdaterAndroidTest, TestPolicyMapIgnoredPolicy) {
  policy_handler_list()->AddHandler(
      std::make_unique<StubPolicyHandler>(kPolicyName, /*has_error=*/false));

  PolicyCacheUpdater updater(policy_service(), policy_handler_list());
  SetPolicy(kPolicyName, kPolicyValue);
  policy_map()->GetMutable(kPolicyName)->SetIgnored();
  UpdatePolicy();
  VerifyPolicyName(kPolicyName, /*has_value=*/false, kPolicyValue);
}

TEST_F(PolicyCacheUpdaterAndroidTest, TestPolicyMapErrorMessagePolicy) {
  policy_handler_list()->AddHandler(
      std::make_unique<StubPolicyHandler>(kPolicyName, /*has_error=*/false));

  PolicyCacheUpdater updater(policy_service(), policy_handler_list());
  SetPolicy(kPolicyName, kPolicyValue);
  policy_map()
      ->GetMutable(kPolicyName)
      ->AddMessage(PolicyMap::MessageType::kError, IDS_POLICY_BLOCKED);
  UpdatePolicy();
  VerifyPolicyName(kPolicyName, /*has_value=*/false, kPolicyValue);
}

TEST_F(PolicyCacheUpdaterAndroidTest, TestPolicyMapWarningMessagePolicy) {
  policy_handler_list()->AddHandler(
      std::make_unique<StubPolicyHandler>(kPolicyName, /*has_error=*/false));

  PolicyCacheUpdater updater(policy_service(), policy_handler_list());
  SetPolicy(kPolicyName, kPolicyValue);
  policy_map()
      ->GetMutable(kPolicyName)
      ->AddMessage(PolicyMap::MessageType::kWarning, IDS_POLICY_BLOCKED);
  UpdatePolicy();
  VerifyPolicyName(kPolicyName, /*has_value=*/true, kPolicyValue);
}

TEST_F(PolicyCacheUpdaterAndroidTest, TestPolicUpdatedBeforeUpdaterCreated) {
  policy_handler_list()->AddHandler(
      std::make_unique<StubPolicyHandler>(kPolicyName, /*has_error=*/false));

  SetPolicy(kPolicyName, kPolicyValue);
  UpdatePolicy();
  VerifyPolicyName(kPolicyName, /*has_value=*/false, kPolicyValue);
  PolicyCacheUpdater updater(policy_service(), policy_handler_list());
  VerifyPolicyName(kPolicyName, /*has_value=*/true, kPolicyValue);
}

}  // namespace android
}  // namespace policy
