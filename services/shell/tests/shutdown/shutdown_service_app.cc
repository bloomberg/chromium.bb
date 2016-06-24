// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/macros.h"
#include "mojo/public/c/system/main.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "services/shell/public/cpp/application_runner.h"
#include "services/shell/public/cpp/shell_client.h"
#include "services/shell/tests/shutdown/shutdown_unittest.mojom.h"

namespace shell {
namespace {

shell::ApplicationRunner* g_app = nullptr;

class ShutdownServiceApp
    : public ShellClient,
      public InterfaceFactory<mojom::ShutdownTestService>,
      public mojom::ShutdownTestService {
 public:
  ShutdownServiceApp() {}
  ~ShutdownServiceApp() override {}

 private:
  // shell::ShellClient:
  void Initialize(Connector* connector, const Identity& identity,
                  uint32_t id) override {}
  bool AcceptConnection(Connection* connection) override {
    connection->AddInterface<mojom::ShutdownTestService>(this);
    return true;
  }

  // InterfaceFactory<mojom::ShutdownTestService>:
  void Create(Connection* connection,
              mojom::ShutdownTestServiceRequest request) override {
    bindings_.AddBinding(this, std::move(request));
  }

  // mojom::ShutdownTestService:
  void SetClient(mojom::ShutdownTestClientPtr client) override {}
  void ShutDown() override { g_app->Quit(); }

  mojo::BindingSet<mojom::ShutdownTestService> bindings_;

  DISALLOW_COPY_AND_ASSIGN(ShutdownServiceApp);
};

}  // namespace
}  // namespace shell


MojoResult MojoMain(MojoHandle shell_handle) {
  shell::ApplicationRunner runner(new shell::ShutdownServiceApp);
  shell::g_app = &runner;
  return runner.Run(shell_handle);
}
