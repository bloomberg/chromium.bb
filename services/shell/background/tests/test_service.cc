// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/c/system/main.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "services/shell/background/tests/test.mojom.h"
#include "services/shell/public/cpp/application_runner.h"
#include "services/shell/public/cpp/connection.h"
#include "services/shell/public/cpp/shell_client.h"

namespace shell {

class TestClient : public ShellClient,
                   public InterfaceFactory<mojom::TestService>,
                   public mojom::TestService {
 public:
  TestClient() {}
  ~TestClient() override {}

 private:
  // ShellClient:
  bool AcceptConnection(Connection* connection) override {
    connection->AddInterface(this);
    return true;
  }
  bool ShellConnectionLost() override {
    return true;
  }

  // InterfaceFactory<mojom::TestService>:
  void Create(Connection* connection,
              mojo::InterfaceRequest<mojom::TestService> request) override {
    bindings_.AddBinding(this, std::move(request));
  }

  // mojom::TestService
  void Test(const TestCallback& callback) override {
    callback.Run();
  }

  mojo::BindingSet<mojom::TestService> bindings_;

  DISALLOW_COPY_AND_ASSIGN(TestClient);
};

}  // namespace shell

MojoResult MojoMain(MojoHandle shell_handle) {
  shell::ApplicationRunner runner(new shell::TestClient);
  return runner.Run(shell_handle);
}
