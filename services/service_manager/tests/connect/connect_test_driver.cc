// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/macros.h"
#include "base/process/process.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "services/service_manager/public/cpp/connection.h"
#include "services/service_manager/public/cpp/connector.h"
#include "services/service_manager/public/cpp/service.h"
#include "services/service_manager/runner/child/test_native_main.h"
#include "services/service_manager/runner/init.h"
#include "services/service_manager/tests/connect/connect_test.mojom.h"
#include "services/service_manager/tests/util.h"

using service_manager::test::mojom::ClientProcessTest;
using service_manager::test::mojom::ClientProcessTestRequest;

namespace {

class Driver : public service_manager::Service,
               public service_manager::InterfaceFactory<ClientProcessTest>,
               public ClientProcessTest {
 public:
  Driver() {}
  ~Driver() override {}

 private:
  // service_manager::Service:
  bool OnConnect(const service_manager::ServiceInfo& remote_info,
                 service_manager::InterfaceRegistry* registry) override {
    registry->AddInterface<ClientProcessTest>(this);
    return true;
  }
  bool OnStop() override {
    // TODO(rockot): http://crbug.com/596621. Should be able to remove this
    // override entirely.
    _exit(1);
  }

  // service_manager::InterfaceFactory<ConnectTestService>:
  void Create(const service_manager::Identity& remote_identity,
              ClientProcessTestRequest request) override {
    bindings_.AddBinding(this, std::move(request));
  }

  // test::mojom::ClientProcessTest:
  void LaunchAndConnectToProcess(
      const LaunchAndConnectToProcessCallback& callback) override {
    base::Process process;
    std::unique_ptr<service_manager::Connection> connection =
        service_manager::test::LaunchAndConnectToProcess(
#if defined(OS_WIN)
            "connect_test_exe.exe",
#else
            "connect_test_exe",
#endif
            service_manager::Identity("exe:connect_test_exe",
                                      service_manager::mojom::kInheritUserID),
            connector(), &process);
    callback.Run(static_cast<int32_t>(connection->GetResult()),
                 connection->GetRemoteIdentity());
  }

  mojo::BindingSet<ClientProcessTest> bindings_;

  DISALLOW_COPY_AND_ASSIGN(Driver);
};

}  // namespace

int main(int argc, char** argv) {
  base::AtExitManager at_exit;
  base::CommandLine::Init(argc, argv);

  service_manager::InitializeLogging();

  Driver driver;
  return service_manager::TestNativeMain(&driver);
}
