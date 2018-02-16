// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/macros.h"
#include "base/test/launcher/unit_test_launcher.h"
#include "base/test/test_suite.h"
#include "build/build_config.h"

#if defined(OS_MACOSX)
#include "base/files/file_path.h"
#include "base/mac/bundle_locations.h"
#include "base/path_service.h"
#include "base/test/mock_chrome_application_mac.h"
#include "ui/base/material_design/material_design_controller.h"
#include "ui/base/resource/resource_bundle.h"
#endif

namespace {

class ShellDialogsTestSuite : public base::TestSuite {
 public:
  ShellDialogsTestSuite(int argc, char** argv);

 protected:
  // base::TestSuite:
  void Initialize() override;
  void Shutdown() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(ShellDialogsTestSuite);
};

ShellDialogsTestSuite::ShellDialogsTestSuite(int argc, char** argv)
    : base::TestSuite(argc, argv) {}

void ShellDialogsTestSuite::Initialize() {
  base::TestSuite::Initialize();

#if defined(OS_MACOSX)
  mock_cr_app::RegisterMockCrApp();

  // Set up framework bundle so that tests on Mac can access nib files.
  base::FilePath path;
  PathService::Get(base::DIR_EXE, &path);
  // The three DirName() calls strip "Contents/MacOS/<binary>" from the path.
  path = path.DirName().DirName().DirName();
  path = path.Append(FILE_PATH_LITERAL("shell_dialogs_unittests.app"));
  base::mac::SetOverrideFrameworkBundlePath(path);

  // Setup resource bundle.
  ui::MaterialDesignController::Initialize();
  ui::ResourceBundle::InitSharedInstanceWithLocale(
      "en-US", nullptr, ui::ResourceBundle::LOAD_COMMON_RESOURCES);
#endif
}

void ShellDialogsTestSuite::Shutdown() {
#if defined(OS_MACOSX)
  ui::ResourceBundle::CleanupSharedInstance();
  base::mac::SetOverrideFrameworkBundle(NULL);
#endif
  base::TestSuite::Shutdown();
}

}  // namespace

int main(int argc, char** argv) {
  ShellDialogsTestSuite test_suite(argc, argv);

  return base::LaunchUnitTests(
      argc, argv,
      base::Bind(&ShellDialogsTestSuite::Run, base::Unretained(&test_suite)));
}
