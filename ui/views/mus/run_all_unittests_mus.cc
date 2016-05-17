// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/run_all_unittests.h"

#include <memory>

#include "base/run_loop.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/simple_thread.h"
#include "base/threading/thread.h"
#include "services/shell/background/background_shell.h"
#include "services/shell/public/cpp/connector.h"
#include "services/shell/public/cpp/shell_client.h"
#include "services/shell/public/cpp/shell_connection.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/mus/window_manager_connection.h"
#include "ui/views/test/platform_test_helper.h"
#include "ui/views/views_delegate.h"

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
  PlatformTestHelperMus() {}
  ~PlatformTestHelperMus() override {}

  // PlatformTestHelper:
  bool IsMus() const override { return true; }

 private:
  DISALLOW_COPY_AND_ASSIGN(PlatformTestHelperMus);
};

std::unique_ptr<PlatformTestHelper> CreatePlatformTestHelper(
    shell::Connector* connector,
    const shell::Identity& identity) {
  if (!WindowManagerConnection::Exists())
    WindowManagerConnection::Create(connector, identity);
  return base::WrapUnique(new PlatformTestHelperMus);
}

class ShellConnection {
 public:
  ShellConnection() : thread_("Persistent shell connections") {
    base::WaitableEvent wait(false, false);
    base::Thread::Options options;
    thread_.StartWithOptions(options);
    thread_.task_runner()->PostTask(
        FROM_HERE, base::Bind(&ShellConnection::SetUpConnections,
                              base::Unretained(this), &wait));
    wait.Wait();

    // WindowManagerConnection cannot be created from here yet, although the
    // connector and identity are available at this point. This is because
    // WindowManagerConnection needs a ViewsDelegate and a MessageLoop to have
    // been installed first. So delay the creation until the necessary
    // dependencies have been met.
    PlatformTestHelper::set_factory(base::Bind(
        &CreatePlatformTestHelper, shell_connector_.get(), shell_identity_));
  }

  ~ShellConnection() {
    views::WindowManagerConnection::Reset();
    base::WaitableEvent wait(false, false);
    thread_.task_runner()->PostTask(
        FROM_HERE, base::Bind(&ShellConnection::TearDownConnections,
                              base::Unretained(this), &wait));
    wait.Wait();
  }

 private:
  void SetUpConnections(base::WaitableEvent* wait) {
    background_shell_.reset(new shell::BackgroundShell);
    background_shell_->Init(nullptr);
    shell_client_.reset(new DefaultShellClient);
    shell_connection_.reset(new shell::ShellConnection(
        shell_client_.get(),
        background_shell_->CreateShellClientRequest(kTestName)));

    // ui/views/mus requires a WindowManager running, for now use the desktop
    // one.
    shell::Connector* connector = shell_connection_->connector();
    connector->Connect("mojo:desktop_wm");
    shell_connector_ = connector->Clone();
    shell_identity_ = shell_connection_->identity();
    wait->Signal();
  }

  void TearDownConnections(base::WaitableEvent* wait) {
    shell_connection_.reset();
    wait->Signal();
  }

  base::Thread thread_;
  std::unique_ptr<shell::BackgroundShell> background_shell_;
  std::unique_ptr<shell::ShellConnection> shell_connection_;
  std::unique_ptr<DefaultShellClient> shell_client_;
  std::unique_ptr<shell::Connector> shell_connector_;
  shell::Identity shell_identity_;

  DISALLOW_COPY_AND_ASSIGN(ShellConnection);
};

class ViewMusTestSuite : public ViewTestSuite {
 public:
  ViewMusTestSuite(int argc, char** argv) : ViewTestSuite(argc, argv) {}

 protected:
  void Initialize() override {
    ViewTestSuite::Initialize();
    shell_connections_.reset(new ShellConnection);
  }

  void Shutdown() override {
    shell_connections_.reset();
    ViewTestSuite::Shutdown();
  }

 private:
  std::unique_ptr<ShellConnection> shell_connections_;

  DISALLOW_COPY_AND_ASSIGN(ViewMusTestSuite);
};

}  // namespace
}  // namespace views

int MasterProcessMain(int argc, char** argv) {
  return views::ViewMusTestSuite(argc, argv).RunTests();
}
