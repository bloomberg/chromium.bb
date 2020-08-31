// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/policy/test_platform_policy_provider.h"

#include "base/no_destructor.h"

policy::MockConfigurationPolicyProvider* GetTestPlatformPolicyProvider() {
  static base::NoDestructor<policy::MockConfigurationPolicyProvider> provider;
  provider->SetAutoRefresh();
  return provider.get();
}
