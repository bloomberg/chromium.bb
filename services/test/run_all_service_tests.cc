// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/files/file.h"
#include "base/i18n/icu_util.h"
#include "base/macros.h"
#include "base/path_service.h"
#include "base/test/launcher/unit_test_launcher.h"
#include "base/test/test_suite.h"
#include "build/build_config.h"
#include "services/service_manager/public/cpp/test/common_initialization.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/resource/scale_factor.h"
#include "ui/base/ui_base_paths.h"

namespace {

class ServiceTestSuite : public base::TestSuite {
 public:
  ServiceTestSuite(int argc, char** argv) : base::TestSuite(argc, argv) {}
  ~ServiceTestSuite() override = default;

 protected:
  void Initialize() override {
    base::TestSuite::Initialize();
    ui::RegisterPathProvider();

    base::FilePath ui_test_pak_path;
    ASSERT_TRUE(base::PathService::Get(ui::UI_TEST_PAK, &ui_test_pak_path));
    ui::ResourceBundle::InitSharedInstanceWithPakPath(ui_test_pak_path);

    base::FilePath path;
#if defined(OS_ANDROID)
    ASSERT_TRUE(base::PathService::Get(ui::DIR_RESOURCE_PAKS_ANDROID, &path));
#else
    ASSERT_TRUE(base::PathService::Get(base::DIR_MODULE, &path));
#endif
    base::FilePath bluetooth_test_strings =
        path.Append(FILE_PATH_LITERAL("bluetooth_test_strings.pak"));
    ui::ResourceBundle::GetSharedInstance().AddDataPackFromPath(
        bluetooth_test_strings, ui::SCALE_FACTOR_NONE);

    // base::TestSuite and ViewsInit both try to load icu. That's ok for tests.
    base::i18n::AllowMultipleInitializeCallsForTesting();
  }

  void Shutdown() override {
    ui::ResourceBundle::CleanupSharedInstance();
    base::TestSuite::Shutdown();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ServiceTestSuite);
};

}  // namespace

int main(int argc, char** argv) {
  ServiceTestSuite test_suite(argc, argv);

  return service_manager::InitializeAndLaunchUnitTests(
      argc, argv,
      base::Bind(&ServiceTestSuite::Run, base::Unretained(&test_suite)));
}
