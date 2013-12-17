// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/test/test_suite.h"

#include "ui/base/resource/resource_bundle.h"

namespace aura {
namespace test {

AuraTestSuite::AuraTestSuite(int argc, char** argv)
    : base::TestSuite(argc, argv) {}

void AuraTestSuite::Initialize() {
  base::TestSuite::Initialize();

  // Force unittests to run using en-US so if we test against string
  // output, it'll pass regardless of the system language.
  ui::ResourceBundle::InitSharedInstanceWithLocale("en-US", NULL);
}

void AuraTestSuite::Shutdown() {
  ui::ResourceBundle::CleanupSharedInstance();
  base::TestSuite::Shutdown();
}

}  // namespace test
}  // namespace aura
