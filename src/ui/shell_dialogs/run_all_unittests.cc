// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/path_service.h"
#include "base/test/launcher/unit_test_launcher.h"
#include "base/test/test_suite.h"
#include "build/build_config.h"
#include "mojo/core/embedder/embedder.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/ui_base_paths.h"

#if defined(OS_APPLE)
#include "base/test/mock_chrome_application_mac.h"
#endif

namespace {

class ShellDialogsTestSuite : public base::TestSuite {
 public:
  ShellDialogsTestSuite(int argc, char** argv);

  ShellDialogsTestSuite(const ShellDialogsTestSuite&) = delete;
  ShellDialogsTestSuite& operator=(const ShellDialogsTestSuite&) = delete;

 protected:
  // base::TestSuite:
  void Initialize() override;
  void Shutdown() override;
};

ShellDialogsTestSuite::ShellDialogsTestSuite(int argc, char** argv)
    : base::TestSuite(argc, argv) {}

void ShellDialogsTestSuite::Initialize() {
  base::TestSuite::Initialize();

#if defined(OS_APPLE)
  mock_cr_app::RegisterMockCrApp();
#endif

  // Setup resource bundle.
  ui::RegisterPathProvider();

  base::FilePath ui_test_pak_path;
  base::PathService::Get(ui::UI_TEST_PAK, &ui_test_pak_path);
  ui::ResourceBundle::InitSharedInstanceWithPakPath(ui_test_pak_path);
}

void ShellDialogsTestSuite::Shutdown() {
  ui::ResourceBundle::CleanupSharedInstance();

  base::TestSuite::Shutdown();
}

}  // namespace

int main(int argc, char** argv) {
  ShellDialogsTestSuite test_suite(argc, argv);

  mojo::core::Init();

  return base::LaunchUnitTests(argc, argv,
                               base::BindOnce(&ShellDialogsTestSuite::Run,
                                              base::Unretained(&test_suite)));
}
