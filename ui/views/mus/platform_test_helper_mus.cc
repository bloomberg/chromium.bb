// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/test/platform_test_helper.h"

#include "base/path_service.h"
#include "mojo/shell/public/cpp/application_impl.h"
#include "mojo/shell/public/cpp/application_test_base.h"
#include "ui/aura/env.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/ui_base_paths.h"
#include "ui/gl/test/gl_surface_test_support.h"
#include "ui/views/mus/window_manager_connection.h"

namespace views {
namespace {

class PlatformTestHelperMus : public PlatformTestHelper {
 public:
  PlatformTestHelperMus() {
    gfx::GLSurfaceTestSupport::InitializeOneOff();

    // TODO(sky): We really shouldn't need to configure ResourceBundle.
    ui::RegisterPathProvider();
    base::FilePath ui_test_pak_path;
    CHECK(PathService::Get(ui::UI_TEST_PAK, &ui_test_pak_path));
    ui::ResourceBundle::InitSharedInstanceWithPakPath(ui_test_pak_path);
    aura::Env::CreateInstance(true);

    mojo_test_helper_.reset(new mojo::test::TestHelper(nullptr));
    // ui/views/mus requires a WindowManager running, for now use the desktop
    // one.
    mojo_test_helper_->application_impl()->ConnectToApplication(
        "mojo:desktop_wm");
    WindowManagerConnection::Create(mojo_test_helper_->application_impl());
  }

  ~PlatformTestHelperMus() override {
    mojo_test_helper_.reset(nullptr);
    aura::Env::DeleteInstance();
    ui::ResourceBundle::CleanupSharedInstance();
  }

 private:
  scoped_ptr<mojo::test::TestHelper> mojo_test_helper_;

  DISALLOW_COPY_AND_ASSIGN(PlatformTestHelperMus);
};

}  // namespace

// static
scoped_ptr<PlatformTestHelper> PlatformTestHelper::Create() {
  return make_scoped_ptr(new PlatformTestHelperMus);
}

}  // namespace views
