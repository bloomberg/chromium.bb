// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/mock_configuration_policy_provider.h"

#include <memory>
#include <string>
#include <utility>

#include "base/run_loop.h"
#include "base/task/current_thread.h"
#include "build/build_config.h"
#include "components/policy/core/common/policy_bundle.h"

using testing::Invoke;

namespace policy {

MockConfigurationPolicyProvider::MockConfigurationPolicyProvider() {}

MockConfigurationPolicyProvider::~MockConfigurationPolicyProvider() {
#if BUILDFLAG(IS_ANDROID)
  ShutdownForTesting();
#endif  // BUILDFLAG(IS_ANDROID)
}

void MockConfigurationPolicyProvider::UpdateChromePolicy(
    const PolicyMap& policy) {
  std::unique_ptr<PolicyBundle> bundle = std::make_unique<PolicyBundle>();
  bundle->Get(PolicyNamespace(POLICY_DOMAIN_CHROME, std::string())) =
      policy.Clone();
  UpdatePolicy(std::move(bundle));
  bool spin_run_loop = base::CurrentThread::IsSet();
#if BUILDFLAG(IS_IOS)
  // On iOS, the UI message loop does not support RunUntilIdle().
  spin_run_loop &= !base::CurrentUIThread::IsSet();
#endif  // BUILDFLAG(IS_IOS)
  if (spin_run_loop)
    base::RunLoop().RunUntilIdle();
}

void MockConfigurationPolicyProvider::UpdateExtensionPolicy(
    const PolicyMap& policy,
    const std::string& extension_id) {
  std::unique_ptr<PolicyBundle> bundle = std::make_unique<PolicyBundle>();
  bundle->Get(PolicyNamespace(POLICY_DOMAIN_EXTENSIONS, extension_id)) =
      policy.Clone();
  UpdatePolicy(std::move(bundle));
  if (base::CurrentThread::IsSet())
    base::RunLoop().RunUntilIdle();
}

void MockConfigurationPolicyProvider::SetAutoRefresh() {
  EXPECT_CALL(*this, RefreshPolicies()).WillRepeatedly(
      Invoke(this, &MockConfigurationPolicyProvider::RefreshWithSamePolicies));
}

void MockConfigurationPolicyProvider::RefreshWithSamePolicies() {
  std::unique_ptr<PolicyBundle> bundle = std::make_unique<PolicyBundle>();
  bundle->CopyFrom(policies());
  UpdatePolicy(std::move(bundle));
}

MockConfigurationPolicyObserver::MockConfigurationPolicyObserver() {}

MockConfigurationPolicyObserver::~MockConfigurationPolicyObserver() {}

}  // namespace policy
