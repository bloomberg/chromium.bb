// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/bindings/binding_set.h"
#include "services/service_manager/background/tests/test.mojom.h"
#include "services/service_manager/public/c/main.h"
#include "services/service_manager/public/cpp/interface_registry.h"
#include "services/service_manager/public/cpp/service.h"
#include "services/service_manager/public/cpp/service_context.h"
#include "services/service_manager/public/cpp/service_runner.h"

namespace service_manager {

// A service that exports a simple interface for testing. Used to test the
// parent background service manager.
class TestClient : public Service,
                   public InterfaceFactory<mojom::TestService>,
                   public mojom::TestService {
 public:
  TestClient() {}
  ~TestClient() override {}

 private:
  // Service:
  bool OnConnect(const ServiceInfo& remote_info,
                 InterfaceRegistry* registry) override {
    registry->AddInterface(this);
    return true;
  }

  // InterfaceFactory<mojom::TestService>:
  void Create(const Identity& remote_identity,
              mojo::InterfaceRequest<mojom::TestService> request) override {
    bindings_.AddBinding(this, std::move(request));
  }

  // mojom::TestService
  void Test(const TestCallback& callback) override {
    callback.Run();
  }

  void Quit() override { context()->RequestQuit(); }

  mojo::BindingSet<mojom::TestService> bindings_;

  DISALLOW_COPY_AND_ASSIGN(TestClient);
};

}  // namespace service_manager

MojoResult ServiceMain(MojoHandle service_request_handle) {
  service_manager::ServiceRunner runner(new service_manager::TestClient);
  return runner.Run(service_request_handle);
}
