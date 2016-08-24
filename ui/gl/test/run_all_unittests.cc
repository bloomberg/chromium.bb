// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/macros.h"
#include "base/test/launcher/unit_test_launcher.h"
#include "base/test/test_suite.h"
#include "build/build_config.h"

#if defined(OS_MACOSX) && !defined(OS_IOS)
#include "base/test/mock_chrome_application_mac.h"
#endif

#if defined(USE_OZONE)
#include "base/command_line.h"
#include "base/message_loop/message_loop.h"
#include "ui/ozone/public/ozone_platform.h"
#endif

namespace {

class GlTestSuite : public base::TestSuite {
 public:
  GlTestSuite(int argc, char** argv) : base::TestSuite(argc, argv) {
  }

 protected:
  void Initialize() override {
    base::TestSuite::Initialize();
#if defined(USE_OZONE)
    main_loop_.reset(new base::MessageLoopForUI());
    // Make Ozone run in single-process mode, where it doesn't expect a GPU
    // process and it spawns and starts its own DRM thread.
    ui::OzonePlatform::InitParams params;
    params.single_process = true;
    ui::OzonePlatform::InitializeForUI(params);
#endif

#if defined(OS_MACOSX) && !defined(OS_IOS)
    mock_cr_app::RegisterMockCrApp();
#endif
  }

  void Shutdown() override {
    base::TestSuite::Shutdown();
  }

 private:
#if defined(USE_OZONE)
  // On Ozone, the backend initializes the event system using a UI thread.
  std::unique_ptr<base::MessageLoopForUI> main_loop_;
#endif

  DISALLOW_COPY_AND_ASSIGN(GlTestSuite);
};

}  // namespace

int main(int argc, char** argv) {
  GlTestSuite test_suite(argc, argv);

  return base::LaunchUnitTests(
      argc,
      argv,
      base::Bind(&GlTestSuite::Run, base::Unretained(&test_suite)));
}
