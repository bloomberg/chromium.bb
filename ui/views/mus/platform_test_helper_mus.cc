// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/test/platform_test_helper.h"

#include "base/command_line.h"
#include "mojo/shell/background/background_shell.h"
#include "mojo/shell/public/cpp/application_impl.h"
#include "mojo/shell/public/cpp/shell_client.h"
#include "ui/views/mus/window_manager_connection.h"
#include "url/gurl.h"

namespace views {
namespace {

class DefaultShellClient : public mojo::ShellClient {
 public:
  DefaultShellClient() {}
  ~DefaultShellClient() override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(DefaultShellClient);
};

class PlatformTestHelperMus : public PlatformTestHelper {
 public:
  PlatformTestHelperMus() {
    // Force the new edk.
    base::CommandLine::ForCurrentProcess()->AppendSwitch("use-new-edk");

    background_shell_.reset(new mojo::shell::BackgroundShell);
    background_shell_->Init();
    shell_client_.reset(new DefaultShellClient);
    app_.reset(new mojo::ApplicationImpl(
        shell_client_.get(),
        background_shell_->CreateApplication(GURL("mojo://test-app"))));
    app_->WaitForInitialize();
    // ui/views/mus requires a WindowManager running, for now use the desktop
    // one.
    app_->Connect("mojo:desktop_wm");
    WindowManagerConnection::Create(app_.get());
  }

  ~PlatformTestHelperMus() override {
    WindowManagerConnection::Reset();
    // |app_| has a reference to us, destroy it while we are still valid.
    app_.reset();
  }

 private:
  scoped_ptr<mojo::shell::BackgroundShell> background_shell_;
  scoped_ptr<mojo::ApplicationImpl> app_;
  scoped_ptr<DefaultShellClient> shell_client_;

  DISALLOW_COPY_AND_ASSIGN(PlatformTestHelperMus);
};

}  // namespace

// static
scoped_ptr<PlatformTestHelper> PlatformTestHelper::Create() {
  return make_scoped_ptr(new PlatformTestHelperMus);
}

}  // namespace views
