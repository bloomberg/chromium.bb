// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/test/launcher/unit_test_launcher.h"
#include "base/test/test_suite.h"

#if defined(OS_MACOSX) && !defined(OS_IOS)
#include "base/test/mock_chrome_application_mac.h"
#endif

namespace {

class GlTestSuite : public base::TestSuite {
 public:
  GlTestSuite(int argc, char** argv) : base::TestSuite(argc, argv) {
  }

 protected:
  void Initialize() override {
    base::TestSuite::Initialize();

#if defined(OS_MACOSX) && !defined(OS_IOS)
    mock_cr_app::RegisterMockCrApp();
#endif
  }

  void Shutdown() override {
    base::TestSuite::Shutdown();
  }

 private:
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
