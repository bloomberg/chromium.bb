// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/test/aura_mus_test_base.h"

namespace aura {
namespace test {

AuraMusClientTestBase::AuraMusClientTestBase() = default;

AuraMusClientTestBase::~AuraMusClientTestBase() = default;

void AuraMusClientTestBase::SetUp() {
  ConfigureEnvMode(Env::Mode::MUS);
  AuraTestBase::SetUp();
}

}  // namespace test
}  // namespace aura
