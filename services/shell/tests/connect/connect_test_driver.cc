// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/macros.h"
#include "base/process/process.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "services/shell/public/cpp/connection.h"
#include "services/shell/public/cpp/connector.h"
#include "services/shell/public/cpp/shell_client.h"
#include "services/shell/runner/child/test_native_main.h"
#include "services/shell/runner/init.h"
#include "services/shell/tests/connect/connect_test.mojom.h"
#include "services/shell/tests/util.h"

using shell::test::mojom::ClientProcessTest;
using shell::test::mojom::ClientProcessTestRequest;

namespace {

class Driver : public shell::ShellClient,
               public shell::InterfaceFactory<ClientProcessTest>,
               public ClientProcessTest {
 public:
  Driver() {}
  ~Driver() override {}

 private:
  // shell::ShellClient:
  void Initialize(shell::Connector* connector,
                  const shell::Identity& identity,
                  uint32_t id) override {
    connector_ = connector;
  }
  bool AcceptConnection(shell::Connection* connection) override {
    connection->AddInterface<ClientProcessTest>(this);
    return true;
  }
  bool ShellConnectionLost() override {
    // TODO(rockot): http://crbug.com/596621. Should be able to remove this
    // override entirely.
    _exit(1);
  }

  // shell::InterfaceFactory<ConnectTestService>:
  void Create(shell::Connection* connection,
              ClientProcessTestRequest request) override {
    bindings_.AddBinding(this, std::move(request));
  }

  // test::mojom::ClientProcessTest:
  void LaunchAndConnectToProcess(
      const LaunchAndConnectToProcessCallback& callback) override {
    base::Process process;
    std::unique_ptr<shell::Connection> connection =
        shell::test::LaunchAndConnectToProcess(
#if defined(OS_WIN)
            "connect_test_exe.exe",
#else
            "connect_test_exe",
#endif
            shell::Identity("exe:connect_test_exe",
                            shell::mojom::kInheritUserID),
            connector_, &process);
    callback.Run(static_cast<int32_t>(connection->GetResult()),
                 shell::mojom::Identity::From(connection->GetRemoteIdentity()));
  }

  shell::Connector* connector_ = nullptr;
  mojo::BindingSet<ClientProcessTest> bindings_;

  DISALLOW_COPY_AND_ASSIGN(Driver);
};

}  // namespace

int main(int argc, char** argv) {
  base::AtExitManager at_exit;
  base::CommandLine::Init(argc, argv);

  shell::InitializeLogging();

  Driver driver;
  return shell::TestNativeMain(&driver);
}
