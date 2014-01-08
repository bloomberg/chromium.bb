// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/test/test_suite.h"

#include "base/files/file_path.h"
#include "base/path_service.h"
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

namespace ui {
namespace test {

UITestSuite::UITestSuite(int argc, char** argv) : base::TestSuite(argc, argv) {}

void UITestSuite::Initialize() {
  base::TestSuite::Initialize();

#if defined(OS_ANDROID)
  // Register JNI bindings for android.
  gfx::android::RegisterJni(base::android::AttachCurrentThread());
  ui::android::RegisterJni(base::android::AttachCurrentThread());
#endif

  ui::RegisterPathProvider();
  gfx::RegisterPathProvider();

#if defined(OS_MACOSX) && !defined(OS_IOS)
  // Look in the framework bundle for resources.
  // TODO(port): make a resource bundle for non-app exes.  What's done here
  // isn't really right because this code needs to depend on chrome_dll
  // being built.  This is inappropriate in app.
  base::FilePath path;
  PathService::Get(base::DIR_EXE, &path);
#if defined(GOOGLE_CHROME_BUILD)
  path = path.AppendASCII("Google Chrome Framework.framework");
#elif defined(CHROMIUM_BUILD)
  path = path.AppendASCII("Chromium Framework.framework");
#else
#error Unknown branding
#endif
  base::mac::SetOverrideFrameworkBundlePath(path);
#endif  // defined(OS_MACOSX) && !defined(OS_IOS)

  // TODO(tfarina): This loads chrome_100_percent.pak and thus introduces a
  // dependency on chrome/, we don't want that here, so change this to
  // InitSharedInstanceWithPakPath().
  ui::ResourceBundle::InitSharedInstanceWithLocale("en-US", NULL);
}

void UITestSuite::Shutdown() {
  ui::ResourceBundle::CleanupSharedInstance();

#if defined(OS_MACOSX) && !defined(OS_IOS)
  base::mac::SetOverrideFrameworkBundle(NULL);
#endif
  base::TestSuite::Shutdown();
}

}  // namespace test
}  // namespace ui
