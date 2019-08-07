// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gls_runner_test_base.h"

namespace credential_provider {

namespace testing {

GlsRunnerTestBase::GlsRunnerTestBase() : run_helper_(&fake_os_user_manager_) {}

GlsRunnerTestBase::~GlsRunnerTestBase() = default;

void GlsRunnerTestBase::SetUp() {
  // Make sure not to read random GCPW settings from the machine that is running
  // the tests.
  InitializeRegistryOverrideForTesting(&registry_override_);
}

}  // namespace testing

}  // namespace credential_provider
