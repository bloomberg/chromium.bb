// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/test/launcher/unit_test_launcher.h"
#include "base/test/test_suite.h"
#include "build/build_config.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/ui_base_paths.h"
#include "ui/gfx/gfx_paths.h"

#if defined(OS_ANDROID)
#include "base/android/jni_android.h"
#include "ui/base/android/ui_base_jni_registrar.h"
#include "ui/gfx/android/gfx_jni_registrar.h"
#endif

#if defined(OS_MACOSX) && !defined(OS_IOS)
#include "base/mac/bundle_locations.h"
#endif

#if defined(OS_WIN)
#include "ui/gfx/win/dpi.h"
#endif

namespace {

class UIBaseTestSuite : public base::TestSuite {
 public:
  UIBaseTestSuite(int argc, char** argv);

 protected:
  // base::TestSuite:
  virtual void Initialize() OVERRIDE;
  virtual void Shutdown() OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(UIBaseTestSuite);
};

UIBaseTestSuite::UIBaseTestSuite(int argc, char** argv)
    : base::TestSuite(argc, argv) {}

void UIBaseTestSuite::Initialize() {
  base::TestSuite::Initialize();

#if defined(OS_WIN)
  gfx::ForceHighDPISupportForTesting(1.0);
#endif

#if defined(OS_ANDROID)
  // Register JNI bindings for android.
  gfx::android::RegisterJni(base::android::AttachCurrentThread());
  ui::android::RegisterJni(base::android::AttachCurrentThread());
#endif

  ui::RegisterPathProvider();
  gfx::RegisterPathProvider();

  base::FilePath exe_path;
  PathService::Get(base::DIR_EXE, &exe_path);

#if defined(OS_MACOSX) && !defined(OS_IOS)
  // On Mac, a test Framework bundle is created that links locale.pak and
  // chrome_100_percent.pak at the appropriate places to ui_test.pak.
  base::mac::SetOverrideFrameworkBundlePath(
      exe_path.AppendASCII("ui_unittests Framework.framework"));
  ui::ResourceBundle::InitSharedInstanceWithLocale("en-US", NULL);

#elif defined(OS_IOS) || defined(OS_ANDROID)
  // On iOS, the ui_unittests binary is itself a mini bundle, with resources
  // built in. On Android, ui_unittests_apk provides the necessary framework.
  ui::ResourceBundle::InitSharedInstanceWithLocale("en-US", NULL);

#else
  // On other platforms, the (hardcoded) paths for chrome_100_percent.pak and
  // locale.pak get populated by later build steps. To avoid clobbering them,
  // load the test .pak files directly.
  ui::ResourceBundle::InitSharedInstanceWithPakPath(
      exe_path.AppendASCII("ui_test.pak"));

  // ui_unittests can't depend on the locales folder which Chrome will make
  // later, so use the path created by ui_test_pak.
  PathService::Override(ui::DIR_LOCALES, exe_path.AppendASCII("ui"));
#endif
}

void UIBaseTestSuite::Shutdown() {
  ui::ResourceBundle::CleanupSharedInstance();

#if defined(OS_MACOSX) && !defined(OS_IOS)
  base::mac::SetOverrideFrameworkBundle(NULL);
#endif
  base::TestSuite::Shutdown();
}

}  // namespace

int main(int argc, char** argv) {
  UIBaseTestSuite test_suite(argc, argv);

  return base::LaunchUnitTests(argc,
                               argv,
                               base::Bind(&UIBaseTestSuite::Run,
                                          base::Unretained(&test_suite)));
}
