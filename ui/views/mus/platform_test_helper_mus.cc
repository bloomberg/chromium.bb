// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/test/platform_test_helper.h"

#include "base/command_line.h"
#include "mojo/shell/background/background_shell.h"
#include "mojo/shell/background/tests/test_application_catalog_store.h"
#include "mojo/shell/public/cpp/connector.h"
#include "mojo/shell/public/cpp/shell_client.h"
#include "mojo/shell/public/cpp/shell_connection.h"
#include "ui/views/mus/window_manager_connection.h"

using mojo::shell::BackgroundShell;

namespace views {
namespace {

const char kTestName[] = "mojo:test-app";

class DefaultShellClient : public mojo::ShellClient {
 public:
  DefaultShellClient() {}
  ~DefaultShellClient() override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(DefaultShellClient);
};

scoped_ptr<mojo::shell::TestApplicationCatalogStore>
BuildTestApplicationCatalogStore() {
  scoped_ptr<base::ListValue> apps(new base::ListValue);
  apps->Append(
      mojo::shell::BuildPermissiveSerializedAppInfo(kTestName, "test"));
  return make_scoped_ptr(
      new mojo::shell::TestApplicationCatalogStore(std::move(apps)));
}

class PlatformTestHelperMus : public PlatformTestHelper {
 public:
  PlatformTestHelperMus() {
    background_shell_.reset(new BackgroundShell);
    scoped_ptr<BackgroundShell::InitParams> init_params(
        new BackgroundShell::InitParams);
    init_params->app_catalog = BuildTestApplicationCatalogStore();
    background_shell_->Init(std::move(init_params));
    shell_client_.reset(new DefaultShellClient);
    shell_connection_.reset(new mojo::ShellConnection(
        shell_client_.get(),
        background_shell_->CreateShellClientRequest(kTestName)));
    shell_connection_->WaitForInitialize();
    // ui/views/mus requires a WindowManager running, for now use the desktop
    // one.
    mojo::Connector* connector = shell_connection_->connector();
    connector->Connect("mojo:desktop_wm");
    WindowManagerConnection::Create(connector);
  }

  ~PlatformTestHelperMus() override {
    WindowManagerConnection::Reset();
    // |app_| has a reference to us, destroy it while we are still valid.
    shell_connection_.reset();
  }

 private:
  scoped_ptr<BackgroundShell> background_shell_;
  scoped_ptr<mojo::ShellConnection> shell_connection_;
  scoped_ptr<DefaultShellClient> shell_client_;

  DISALLOW_COPY_AND_ASSIGN(PlatformTestHelperMus);
};

}  // namespace

// static
scoped_ptr<PlatformTestHelper> PlatformTestHelper::Create() {
  return make_scoped_ptr(new PlatformTestHelperMus);
}

}  // namespace views
