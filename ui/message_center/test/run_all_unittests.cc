// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/path_service.h"
#include "base/test/launcher/unit_test_launcher.h"
#include "base/test/test_suite.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/ui_base_paths.h"

#if !defined(OS_MACOSX)
#include "ui/gl/gl_surface.h"
#endif

namespace {

class MessageCenterTestSuite : public base::TestSuite {
 public:
  MessageCenterTestSuite(int argc, char** argv) : base::TestSuite(argc, argv) {}

 protected:
  virtual void Initialize() OVERRIDE {
#if !defined(OS_MACOSX)
    gfx::GLSurface::InitializeOneOffForTests();
#endif
    base::TestSuite::Initialize();
    ui::RegisterPathProvider();

    base::FilePath pak_dir;
    PathService::Get(base::DIR_MODULE, &pak_dir);

    base::FilePath pak_file;
    pak_file = pak_dir.Append(FILE_PATH_LITERAL("ui_test.pak"));

    ui::ResourceBundle::InitSharedInstanceWithPakPath(pak_file);
  }

  virtual void Shutdown() OVERRIDE {
    ui::ResourceBundle::CleanupSharedInstance();
    base::TestSuite::Shutdown();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(MessageCenterTestSuite);
};

}  // namespace

int main(int argc, char** argv) {
  MessageCenterTestSuite test_suite(argc, argv);

  return base::LaunchUnitTests(
      argc,
      argv,
      base::Bind(&MessageCenterTestSuite::Run, base::Unretained(&test_suite)));
}
