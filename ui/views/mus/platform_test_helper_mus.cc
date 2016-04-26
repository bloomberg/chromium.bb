// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "services/shell/background/background_shell.h"
#include "services/shell/background/tests/test_catalog_store.h"
#include "services/shell/public/cpp/connector.h"
#include "services/shell/public/cpp/shell_client.h"
#include "services/shell/public/cpp/shell_connection.h"
#include "ui/aura/env.h"
#include "ui/views/mus/window_manager_connection.h"
#include "ui/views/test/platform_test_helper.h"
#include "ui/views/views_delegate.h"

using shell::BackgroundShell;

namespace views {
namespace {

const char kTestName[] = "exe:views_mus_unittests";

class DefaultShellClient : public shell::ShellClient {
 public:
  DefaultShellClient() {}
  ~DefaultShellClient() override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(DefaultShellClient);
};

class PlatformTestHelperMus : public PlatformTestHelper {
 public:
  PlatformTestHelperMus() {
    background_shell_.reset(new BackgroundShell);
    background_shell_->Init(nullptr);
    shell_client_.reset(new DefaultShellClient);
    shell_connection_.reset(new shell::ShellConnection(
        shell_client_.get(),
        background_shell_->CreateShellClientRequest(kTestName)));

    // TODO(rockot): Remove this RunLoop. http://crbug.com/594852.
    base::RunLoop wait_loop;
    shell_connection_->set_initialize_handler(wait_loop.QuitClosure());
    wait_loop.Run();

    // ui/views/mus requires a WindowManager running, for now use the desktop
    // one.
    shell::Connector* connector = shell_connection_->connector();
    connector->Connect("mojo:desktop_wm");
    WindowManagerConnection::Create(connector, shell_connection_->identity());

    // On X we need to reset the ContextFactory before every NativeWidgetMus
    // is created.
    // TODO(sad): this is a hack, figure out a better solution.
    ViewsDelegate::GetInstance()->set_native_widget_factory(base::Bind(
        &PlatformTestHelperMus::CreateNativeWidgetMus, base::Unretained(this),
        std::map<std::string, std::vector<uint8_t>>()));
  }

  ~PlatformTestHelperMus() override {
    WindowManagerConnection::Reset();
    // |app_| has a reference to us, destroy it while we are still valid.
    shell_connection_.reset();
  }

  bool IsMus() const override { return true; }

 private:
  NativeWidget* CreateNativeWidgetMus(
      const std::map<std::string, std::vector<uint8_t>>& props,
      const Widget::InitParams& init_params,
      internal::NativeWidgetDelegate* delegate) {
    ui::ContextFactory* factory = aura::Env::GetInstance()->context_factory();
    aura::Env::GetInstance()->set_context_factory(nullptr);
    NativeWidget* result =
        WindowManagerConnection::Get()->CreateNativeWidgetMus(
            props, init_params, delegate);
    aura::Env::GetInstance()->set_context_factory(factory);
    return result;
  }

  std::unique_ptr<BackgroundShell> background_shell_;
  std::unique_ptr<shell::ShellConnection> shell_connection_;
  std::unique_ptr<DefaultShellClient> shell_client_;

  DISALLOW_COPY_AND_ASSIGN(PlatformTestHelperMus);
};

}  // namespace

// static
std::unique_ptr<PlatformTestHelper> PlatformTestHelper::Create() {
  return base::WrapUnique(new PlatformTestHelperMus);
}

}  // namespace views
