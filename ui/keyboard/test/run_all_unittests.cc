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
#include "ui/gl/gl_surface.h"

namespace {

class KeyboardTestSuite : public base::TestSuite {
 public:
  KeyboardTestSuite(int argc, char** argv) : base::TestSuite(argc, argv) {}

 protected:
  virtual void Initialize() OVERRIDE {
    base::TestSuite::Initialize();
    gfx::GLSurface::InitializeOneOffForTests();
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
  DISALLOW_COPY_AND_ASSIGN(KeyboardTestSuite);
};

}  // namespace

int main(int argc, char** argv) {
  KeyboardTestSuite test_suite(argc, argv);

  return base::LaunchUnitTests(
      argc,
      argv,
      base::Bind(&KeyboardTestSuite::Run, base::Unretained(&test_suite)));
}
