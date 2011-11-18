// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura_shell/test_suite.h"

#include "base/file_path.h"
#include "base/path_service.h"
#include "build/build_config.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/ui_base_paths.h"
#include "ui/gfx/compositor/test/compositor_test_support.h"
#include "ui/gfx/gfx_paths.h"
#include "ui/gfx/test/gfx_test_utils.h"

namespace aura_shell {
namespace test {

AuraShellTestSuite::AuraShellTestSuite(int argc, char** argv)
    : TestSuite(argc, argv) {}

void AuraShellTestSuite::Initialize() {
  base::TestSuite::Initialize();

  gfx::RegisterPathProvider();
  ui::RegisterPathProvider();

  // Force unittests to run using en-US so if we test against string
  // output, it'll pass regardless of the system language.
  ui::ResourceBundle::InitSharedInstance("en-US");
  ui::CompositorTestSupport::Initialize();
  ui::gfx_test_utils::SetupTestCompositor();
}

void AuraShellTestSuite::Shutdown() {
  ui::CompositorTestSupport::Terminate();
  ui::ResourceBundle::CleanupSharedInstance();

  base::TestSuite::Shutdown();
}

}  // namespace test
}  // namespace aura_shell
