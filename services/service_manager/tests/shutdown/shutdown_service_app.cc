// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/macros.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "services/service_manager/public/c/main.h"
#include "services/service_manager/public/cpp/interface_factory.h"
#include "services/service_manager/public/cpp/interface_registry.h"
#include "services/service_manager/public/cpp/service.h"
#include "services/service_manager/public/cpp/service_runner.h"
#include "services/service_manager/tests/shutdown/shutdown_unittest.mojom.h"

namespace service_manager {
namespace {

service_manager::ServiceRunner* g_app = nullptr;

class ShutdownServiceApp
    : public Service,
      public InterfaceFactory<mojom::ShutdownTestService>,
      public mojom::ShutdownTestService {
 public:
  ShutdownServiceApp() {}
  ~ShutdownServiceApp() override {}

 private:
  // service_manager::Service:
  bool OnConnect(const ServiceInfo& remote_info,
                 InterfaceRegistry* registry) override {
    registry->AddInterface<mojom::ShutdownTestService>(this);
    return true;
  }

  // InterfaceFactory<mojom::ShutdownTestService>:
  void Create(const Identity& remote_identity,
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
}  // namespace service_manager

MojoResult ServiceMain(MojoHandle service_request_handle) {
  service_manager::ServiceRunner runner(
      new service_manager::ShutdownServiceApp);
  service_manager::g_app = &runner;
  return runner.Run(service_request_handle);
}
