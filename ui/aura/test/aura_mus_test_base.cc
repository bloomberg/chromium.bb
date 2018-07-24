// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/test/aura_mus_test_base.h"

namespace aura {
namespace test {

AuraMusClientTestBase::AuraMusClientTestBase() = default;

AuraMusClientTestBase::~AuraMusClientTestBase() = default;

void AuraMusClientTestBase::SetUp() {
  // Run AuraTestBase::SetUp() first because it puts an InProcessContextFactory
  // in env.
  ConfigureEnvMode(Env::Mode::MUS);
  // Don't create a WindowTreeHost by default.
  // TODO(sky): update tests so that this isn't necessary (make the test code
  // always create a host for the primary display). https://crbug.com/866634
  SetCreateHostForPrimaryDisplay(false);
  AuraTestBase::SetUp();
}

}  // namespace test
}  // namespace aura
