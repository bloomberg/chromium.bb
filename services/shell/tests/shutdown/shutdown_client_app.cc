// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/macros.h"
#include "mojo/public/c/system/main.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "services/shell/public/cpp/application_runner.h"
#include "services/shell/public/cpp/connector.h"
#include "services/shell/public/cpp/interface_factory.h"
#include "services/shell/public/cpp/shell_client.h"
#include "services/shell/tests/shutdown/shutdown_unittest.mojom.h"

namespace shell {

class ShutdownClientApp
    : public ShellClient,
      public InterfaceFactory<mojom::ShutdownTestClientController>,
      public mojom::ShutdownTestClientController,
      public mojom::ShutdownTestClient {
 public:
  ShutdownClientApp() {}
  ~ShutdownClientApp() override {}

 private:
  // shell::ShellClient:
  void Initialize(Connector* connector, const Identity& identity,
                  uint32_t id) override {
    connector_ = connector;
  }

  bool AcceptConnection(Connection* connection) override {
    connection->AddInterface<mojom::ShutdownTestClientController>(this);
    return true;
  }

  // InterfaceFactory<mojom::ShutdownTestClientController>:
  void Create(Connection* connection,
              mojom::ShutdownTestClientControllerRequest request) override {
    bindings_.AddBinding(this, std::move(request));
  }

  // mojom::ShutdownTestClientController:
  void ConnectAndWait(const ConnectAndWaitCallback& callback) override {
    mojom::ShutdownTestServicePtr service;
    connector_->ConnectToInterface("mojo:shutdown_service", &service);

    mojo::Binding<mojom::ShutdownTestClient> client_binding(this);

    mojom::ShutdownTestClientPtr client_ptr =
        client_binding.CreateInterfacePtrAndBind();

    service->SetClient(std::move(client_ptr));

    client_binding.WaitForIncomingMethodCall();
    callback.Run();
  }

  Connector* connector_ = nullptr;
  mojo::BindingSet<mojom::ShutdownTestClientController> bindings_;

  DISALLOW_COPY_AND_ASSIGN(ShutdownClientApp);
};

}  // namespace shell


MojoResult MojoMain(MojoHandle shell_handle) {
  return shell::ApplicationRunner(new shell::ShutdownClientApp)
      .Run(shell_handle);
}
