// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_POLICY_WIN_SANDBOX_POLICY_FEATURE_TEST_H_
#define SANDBOX_POLICY_WIN_SANDBOX_POLICY_FEATURE_TEST_H_

#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "sandbox/policy/features.h"
#include "sandbox/policy/switches.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_WIN)
#include "base/win/windows_version.h"
#include "sandbox/policy/win/sandbox_test_utils.h"
#include "sandbox/policy/win/sandbox_win.h"
#include "sandbox/win/src/app_container_base.h"
#include "sandbox/win/src/sandbox_factory.h"
#include "sandbox/win/src/sandbox_policy.h"
#include "sandbox/win/src/sandbox_policy_base.h"
#endif

namespace sandbox {
namespace policy {

class SandboxFeatureTest
    : public ::testing::Test,
      public ::testing::WithParamInterface<
          ::testing::tuple</* renderer app container feature */ bool,
                           /* ktm mitigation feature */ bool>> {
 public:
  SandboxFeatureTest();

  // App Containers are only available in Windows 8 and up
  virtual AppContainerType GetExpectedAppContainerType();

  virtual MitigationFlags GetExpectedMitigationFlags();

  virtual MitigationFlags GetExpectedDelayedMitigationFlags();

  base::test::ScopedFeatureList feature_list_;
};
}  // namespace policy
}  // namespace sandbox

#endif  // SANDBOX_POLICY_WIN_SANDBOX_POLICY_FEATURE_TEST_H_