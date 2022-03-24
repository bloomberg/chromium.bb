// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include "base/bind.h"
#include "base/debug/leak_annotations.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "content/public/test/browser_test.h"
#include "extensions/browser/api/system_display/display_info_provider.h"
#include "extensions/browser/api/system_display/system_display_api.h"
#include "extensions/browser/api_test_utils.h"
#include "extensions/browser/mock_display_info_provider.h"
#include "extensions/browser/mock_screen.h"
#include "extensions/common/api/system_display.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/display/test/scoped_screen_override.h"

namespace extensions {

using display::Screen;
using display::test::ScopedScreenOverride;
using ContextType = ExtensionBrowserTest::ContextType;

class SystemDisplayExtensionApiTest
    : public ExtensionApiTest,
      public testing::WithParamInterface<ContextType> {
 public:
  SystemDisplayExtensionApiTest() : ExtensionApiTest(GetParam()) {}
  ~SystemDisplayExtensionApiTest() override = default;
  SystemDisplayExtensionApiTest(const SystemDisplayExtensionApiTest&) = delete;
  SystemDisplayExtensionApiTest& operator=(
      const SystemDisplayExtensionApiTest&) = delete;

  void SetUpOnMainThread() override {
    ExtensionApiTest::SetUpOnMainThread();
    ANNOTATE_LEAKING_OBJECT_PTR(Screen::GetScreen());
    scoped_screen_override_ =
        std::make_unique<ScopedScreenOverride>(screen_.get());
    DisplayInfoProvider::InitializeForTesting(provider_.get());
  }

  void TearDownOnMainThread() override {
    ExtensionApiTest::TearDownOnMainThread();
    scoped_screen_override_.reset();
  }

 protected:
  std::unique_ptr<MockDisplayInfoProvider> provider_ =
      std::make_unique<MockDisplayInfoProvider>();

 private:
  std::unique_ptr<Screen> screen_ = std::make_unique<MockScreen>();
  std::unique_ptr<ScopedScreenOverride> scoped_screen_override_;
};

// TODO(crbug.com/1231357): MockScreen causes random failures on Windows.
#if !defined(OS_WIN)

INSTANTIATE_TEST_SUITE_P(PersistentBackground,
                         SystemDisplayExtensionApiTest,
                         ::testing::Values(ContextType::kPersistentBackground));
INSTANTIATE_TEST_SUITE_P(ServiceWorker,
                         SystemDisplayExtensionApiTest,
                         ::testing::Values(ContextType::kServiceWorker));

IN_PROC_BROWSER_TEST_P(SystemDisplayExtensionApiTest, GetDisplayInfo) {
  ASSERT_TRUE(RunExtensionTest("system_display/info")) << message_;
}

#endif  // defined(OS_WIN)

#if !BUILDFLAG(IS_CHROMEOS_ASH)

using SystemDisplayExtensionApiFunctionTest = SystemDisplayExtensionApiTest;

INSTANTIATE_TEST_SUITE_P(PersistentBackground,
                         SystemDisplayExtensionApiFunctionTest,
                         ::testing::Values(ContextType::kPersistentBackground));

IN_PROC_BROWSER_TEST_P(SystemDisplayExtensionApiFunctionTest, SetDisplay) {
  scoped_refptr<SystemDisplaySetDisplayPropertiesFunction> set_info_function(
      new SystemDisplaySetDisplayPropertiesFunction());

  set_info_function->set_has_callback(true);

  EXPECT_EQ(SystemDisplayCrOSRestrictedFunction::kCrosOnlyError,
            api_test_utils::RunFunctionAndReturnError(
                set_info_function.get(), "[\"display_id\", {}]", profile()));

  std::unique_ptr<base::DictionaryValue> set_info =
      provider_->GetSetInfoValue();
  EXPECT_FALSE(set_info);
}

#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

}  // namespace extensions
