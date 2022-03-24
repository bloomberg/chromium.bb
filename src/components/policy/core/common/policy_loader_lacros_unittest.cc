// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/policy_loader_lacros.h"

#include <stdint.h>
#include <vector>

#include "chromeos/crosapi/mojom/crosapi.mojom.h"
#include "chromeos/lacros/lacros_service.h"
#include "chromeos/lacros/lacros_test_helper.h"
#include "components/policy/core/common/async_policy_provider.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/core/common/configuration_policy_provider_test.h"
#include "components/policy/core/common/policy_bundle.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_namespace.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/core/common/schema_map.h"
#include "components/policy/policy_constants.h"
#include "components/policy/proto/cloud_policy.pb.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace em = enterprise_management;

namespace policy {

namespace {

std::vector<uint8_t> GetValidPolicyFetchResponse(
    const em::CloudPolicySettings& policy_proto) {
  em::PolicyData policy_data;
  policy_proto.SerializeToString(policy_data.mutable_policy_value());
  policy_data.set_policy_type(dm_protocol::kChromeUserPolicyType);
  em::PolicyFetchResponse policy_response;
  policy_data.SerializeToString(policy_response.mutable_policy_data());
  std::vector<uint8_t> data;
  size_t size = policy_response.ByteSizeLong();
  data.resize(size);
  policy_response.SerializeToArray(data.data(), size);
  return data;
}

const PolicyMap& GetChromePolicyMap(PolicyBundle* bundle) {
  PolicyNamespace ns = PolicyNamespace(POLICY_DOMAIN_CHROME, std::string());
  return bundle->Get(ns);
}

std::vector<uint8_t> GetValidPolicyFetchResponseWithAllPolicy() {
  em::CloudPolicySettings policy_proto;
  // TaskManagerEndProcessEnabled is a per_profile:True policy. See
  // policy_templates.json for details.
  policy_proto.mutable_taskmanagerendprocessenabled()->set_value(false);
  // HomepageLocation is a per_profile:True policy. See policy_templates.json
  // for details.
  policy_proto.mutable_homepagelocation()->set_value("http://chromium.org");
  return GetValidPolicyFetchResponse(policy_proto);
}

}  // namespace

// Test cases for lacros policy provider specific functionality.
class PolicyLoaderLacrosTest : public PolicyTestBase {
 protected:
  PolicyLoaderLacrosTest() = default;
  ~PolicyLoaderLacrosTest() override {}

  void SetPolicy() {
    std::vector<uint8_t> data = GetValidPolicyFetchResponseWithAllPolicy();
    auto init_params = crosapi::mojom::BrowserInitParams::New();
    init_params->device_account_policy = data;
    chromeos::LacrosService::Get()->SetInitParamsForTests(
        std::move(init_params));
  }

  void CheckProfilePolicies(const PolicyMap& policy_map) const {
    if (per_profile_ == PolicyPerProfileFilter::kFalse) {
      EXPECT_EQ(nullptr, policy_map.GetValue(key::kHomepageLocation));
      EXPECT_EQ(nullptr, policy_map.GetValue(key::kAllowDinosaurEasterEgg));
    } else {
      EXPECT_EQ("http://chromium.org",
                policy_map.GetValue(key::kHomepageLocation)->GetString());
      // Enterprise default.
      EXPECT_EQ(false,
                policy_map.GetValue(key::kAllowDinosaurEasterEgg)->GetBool());
    }
  }

  void CheckSystemWidePolicies(const PolicyMap& policy_map) const {
    if (per_profile_ == PolicyPerProfileFilter::kTrue) {
      EXPECT_EQ(nullptr,
                policy_map.GetValue(key::kTaskManagerEndProcessEnabled));
      EXPECT_EQ(nullptr, policy_map.GetValue(key::kPinUnlockAutosubmitEnabled));
    } else {
      EXPECT_FALSE(
          policy_map.GetValue(key::kTaskManagerEndProcessEnabled)->GetBool());
      // Enterprise default.
      EXPECT_FALSE(
          policy_map.GetValue(key::kPinUnlockAutosubmitEnabled)->GetBool());
    }
  }

  void CheckCorrectPoliciesAreSet(PolicyBundle* bundle) const {
    const PolicyMap& policy_map = GetChromePolicyMap(bundle);
    CheckProfilePolicies(policy_map);
    CheckSystemWidePolicies(policy_map);
  }

  SchemaRegistry schema_registry_;
  PolicyPerProfileFilter per_profile_ = PolicyPerProfileFilter::kFalse;
  chromeos::ScopedLacrosServiceTestHelper test_helper_;
};

TEST_F(PolicyLoaderLacrosTest, BasicTestSystemWidePolicies) {
  per_profile_ = PolicyPerProfileFilter::kFalse;
  SetPolicy();

  PolicyLoaderLacros loader(task_environment_.GetMainThreadTaskRunner(),
                            per_profile_);
  base::RunLoop().RunUntilIdle();
  CheckCorrectPoliciesAreSet(loader.Load().get());
}

TEST_F(PolicyLoaderLacrosTest, BasicTestProfilePolicies) {
  per_profile_ = PolicyPerProfileFilter::kTrue;
  SetPolicy();

  PolicyLoaderLacros loader(task_environment_.GetMainThreadTaskRunner(),
                            per_profile_);
  base::RunLoop().RunUntilIdle();
  CheckCorrectPoliciesAreSet(loader.Load().get());
}

TEST_F(PolicyLoaderLacrosTest, UpdateTestProfilePolicies) {
  per_profile_ = PolicyPerProfileFilter::kTrue;
  auto init_params = crosapi::mojom::BrowserInitParams::New();

  chromeos::LacrosService::Get()->SetInitParamsForTests(std::move(init_params));

  PolicyLoaderLacros* loader = new PolicyLoaderLacros(
      task_environment_.GetMainThreadTaskRunner(), per_profile_);
  AsyncPolicyProvider provider(&schema_registry_,
                               std::unique_ptr<AsyncPolicyLoader>(loader));
  provider.Init(&schema_registry_);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(GetChromePolicyMap(loader->Load().get()).size(), (unsigned int)0);

  std::vector<uint8_t> data = GetValidPolicyFetchResponseWithAllPolicy();
  loader->OnPolicyUpdated(data);
  base::RunLoop().RunUntilIdle();
  EXPECT_GT(GetChromePolicyMap(loader->Load().get()).size(),
            static_cast<unsigned int>(0));
  provider.Shutdown();
}

TEST_F(PolicyLoaderLacrosTest, UpdateTestSystemWidePolicies) {
  per_profile_ = PolicyPerProfileFilter::kFalse;
  auto init_params = crosapi::mojom::BrowserInitParams::New();

  chromeos::LacrosService::Get()->SetInitParamsForTests(std::move(init_params));

  PolicyLoaderLacros* loader = new PolicyLoaderLacros(
      task_environment_.GetMainThreadTaskRunner(), per_profile_);
  AsyncPolicyProvider provider(&schema_registry_,
                               std::unique_ptr<AsyncPolicyLoader>(loader));
  provider.Init(&schema_registry_);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(GetChromePolicyMap(loader->Load().get()).size(), (unsigned int)0);

  std::vector<uint8_t> data = GetValidPolicyFetchResponseWithAllPolicy();
  loader->OnPolicyUpdated(data);
  base::RunLoop().RunUntilIdle();
  EXPECT_GT(GetChromePolicyMap(loader->Load().get()).size(),
            static_cast<unsigned int>(0));
  provider.Shutdown();
}

TEST_F(PolicyLoaderLacrosTest, TwoLoaders) {
  auto init_params = crosapi::mojom::BrowserInitParams::New();

  chromeos::LacrosService::Get()->SetInitParamsForTests(std::move(init_params));

  PolicyLoaderLacros* system_wide_loader =
      new PolicyLoaderLacros(task_environment_.GetMainThreadTaskRunner(),
                             PolicyPerProfileFilter::kFalse);
  AsyncPolicyProvider system_wide_provider(
      &schema_registry_,
      std::unique_ptr<AsyncPolicyLoader>(system_wide_loader));
  system_wide_provider.Init(&schema_registry_);

  PolicyLoaderLacros* per_profile_loader =
      new PolicyLoaderLacros(task_environment_.GetMainThreadTaskRunner(),
                             PolicyPerProfileFilter::kTrue);
  AsyncPolicyProvider per_profile_provider(
      &schema_registry_,
      std::unique_ptr<AsyncPolicyLoader>(per_profile_loader));
  per_profile_provider.Init(&schema_registry_);

  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(GetChromePolicyMap(system_wide_loader->Load().get()).size(),
            (unsigned int)0);
  EXPECT_EQ(GetChromePolicyMap(per_profile_loader->Load().get()).size(),
            (unsigned int)0);

  std::vector<uint8_t> data = GetValidPolicyFetchResponseWithAllPolicy();
  system_wide_loader->OnPolicyUpdated(data);
  per_profile_loader->OnPolicyUpdated(data);
  base::RunLoop().RunUntilIdle();
  EXPECT_GT(GetChromePolicyMap(system_wide_loader->Load().get()).size(),
            static_cast<unsigned int>(0));
  EXPECT_GT(GetChromePolicyMap(per_profile_loader->Load().get()).size(),
            static_cast<unsigned int>(0));
  system_wide_provider.Shutdown();
  per_profile_provider.Shutdown();
}

}  // namespace policy
