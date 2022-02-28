// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/renderer_sandboxed_process_launcher_delegate.h"

#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "sandbox/policy/features.h"
#include "sandbox/policy/switches.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_WIN)
#include "base/win/windows_version.h"
#include "sandbox/policy/win/sandbox_policy_feature_test.h"
#include "sandbox/policy/win/sandbox_test_utils.h"
#include "sandbox/policy/win/sandbox_win.h"
#include "sandbox/win/src/app_container_base.h"
#include "sandbox/win/src/sandbox_factory.h"
#include "sandbox/win/src/sandbox_policy.h"
#include "sandbox/win/src/sandbox_policy_base.h"
#endif

using ::testing::_;
using ::testing::Return;

using ::testing::ElementsAre;
using ::testing::Pair;

namespace content {
namespace sandbox {
namespace policy {

#if defined(OS_WIN)
class RendererFeatureSandboxWinTest
    : public ::sandbox::policy::SandboxFeatureTest {
 public:
  RendererFeatureSandboxWinTest() = default;

  // App Containers are only available in Windows 8 and up
  ::sandbox::AppContainerType GetExpectedAppContainerType() override {
    if (base::win::GetVersion() >= base::win::Version::WIN8 &&
        ::testing::get<0>(GetParam()))
      return ::sandbox::AppContainerType::kLowbox;

    return ::sandbox::AppContainerType::kNone;
  }

  ::sandbox::MitigationFlags GetExpectedMitigationFlags() override {
    // Mitigation flags are set on the policy regardless of the OS version
    return SandboxFeatureTest::GetExpectedMitigationFlags() |
           ::sandbox::MITIGATION_CET_DISABLED;
  }

  base::test::ScopedFeatureList feature_list_;
};

TEST_P(RendererFeatureSandboxWinTest, RendererGeneratedPolicyTest) {
  base::CommandLine cmd_line(base::CommandLine::NO_PROGRAM);
  base::HandlesToInheritVector handles_to_inherit;
  ::sandbox::BrokerServices* broker =
      ::sandbox::SandboxFactory::GetBrokerServices();
  scoped_refptr<::sandbox::TargetPolicy> policy = broker->CreatePolicy();

  content::RendererSandboxedProcessLauncherDelegateWin test_renderer_delegate(
      &cmd_line, /* is_jit_disabled */ false);

  // PreSpawn
  ::sandbox::ResultCode result =
      ::sandbox::policy::SandboxWin::GeneratePolicyForSandboxedProcess(
          cmd_line, ::sandbox::policy::switches::kRendererProcess,
          handles_to_inherit, &test_renderer_delegate, policy);
  ASSERT_EQ(::sandbox::ResultCode::SBOX_ALL_OK, result);

  EXPECT_EQ(policy->GetIntegrityLevel(),
            ::sandbox::IntegrityLevel::INTEGRITY_LEVEL_LOW);
  EXPECT_EQ(policy->GetLockdownTokenLevel(),
            ::sandbox::TokenLevel::USER_LOCKDOWN);
  EXPECT_EQ(policy->GetInitialTokenLevel(),
            ::sandbox::TokenLevel::USER_RESTRICTED_SAME_ACCESS);
  EXPECT_EQ(policy->GetProcessMitigations(), GetExpectedMitigationFlags());
  EXPECT_EQ(policy->GetDelayedProcessMitigations(),
            GetExpectedDelayedMitigationFlags());

  if (GetExpectedAppContainerType() == ::sandbox::AppContainerType::kLowbox) {
    EXPECT_EQ(GetExpectedAppContainerType(),
              policy->GetAppContainer()->GetAppContainerType());

    ::sandbox::policy::EqualSidList(
        static_cast<::sandbox::PolicyBase*>(policy.get())
            ->GetAppContainerBase()
            ->GetCapabilities(),
        {});
  } else {
    EXPECT_EQ(policy->GetAppContainer().get(), nullptr);
  }
}

INSTANTIATE_TEST_SUITE_P(
    RendererSandboxSettings,
    RendererFeatureSandboxWinTest,
    ::testing::Combine(
        /* renderer app container feature */ ::testing::Bool(),
        /* ktm mitigation feature */ ::testing::Bool()));
#endif

}  // namespace policy
}  // namespace sandbox
}  // namespace content
