// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "services/service_manager/public/c/main.h"
#include "services/service_manager/public/cpp/binder_registry.h"
#include "services/service_manager/public/cpp/connector.h"
#include "services/service_manager/public/cpp/interface_factory.h"
#include "services/service_manager/public/cpp/service.h"
#include "services/service_manager/public/cpp/service_context.h"
#include "services/service_manager/public/cpp/service_context_ref.h"
#include "services/service_manager/public/cpp/service_runner.h"
#include "services/service_manager/public/interfaces/connector.mojom.h"
#include "services/service_manager/tests/connect/connect_test.mojom.h"

namespace service_manager {

using GetTitleCallback = test::mojom::ConnectTestService::GetTitleCallback;

class ConnectTestClassApp
    : public Service,
      public InterfaceFactory<test::mojom::ConnectTestService>,
      public InterfaceFactory<test::mojom::ClassInterface>,
      public test::mojom::ConnectTestService,
      public test::mojom::ClassInterface {
 public:
  ConnectTestClassApp()
      : ref_factory_(base::Bind(&ConnectTestClassApp::HandleQuit,
                                base::Unretained(this))) {
    bindings_.set_connection_error_handler(base::Bind(
        &ConnectTestClassApp::HandleInterfaceClose, base::Unretained(this)));
    class_interface_bindings_.set_connection_error_handler(base::Bind(
        &ConnectTestClassApp::HandleInterfaceClose, base::Unretained(this)));
    registry_.AddInterface<test::mojom::ConnectTestService>(this);
    registry_.AddInterface<test::mojom::ClassInterface>(this);
  }
  ~ConnectTestClassApp() override {}

 private:
  // service_manager::Service:
  void OnBindInterface(const ServiceInfo& source_info,
                       const std::string& interface_name,
                       mojo::ScopedMessagePipeHandle interface_pipe) override {
    registry_.BindInterface(source_info.identity, interface_name,
                            std::move(interface_pipe));
  }

  // InterfaceFactory<test::mojom::ConnectTestService>:
  void Create(const Identity& remote_identity,
              test::mojom::ConnectTestServiceRequest request) override {
    refs_.push_back(ref_factory_.CreateRef());
    bindings_.AddBinding(this, std::move(request));
  }

  // InterfaceFactory<test::mojom::ClassInterface>:
  void Create(const Identity& remote_identity,
              test::mojom::ClassInterfaceRequest request) override {
    refs_.push_back(ref_factory_.CreateRef());
    class_interface_bindings_.AddBinding(this, std::move(request));
  }

  // test::mojom::ConnectTestService:
  void GetTitle(const GetTitleCallback& callback) override {
    callback.Run("CLASS APP");
  }
  void GetInstance(const GetInstanceCallback& callback) override {
    callback.Run(context()->identity().instance());
  }

  // test::mojom::ClassInterface:
  void Ping(const PingCallback& callback) override {
    callback.Run("PONG");
  }

  void HandleQuit() { context()->QuitNow(); }

  void HandleInterfaceClose() { refs_.pop_back(); }

  BinderRegistry registry_;
  mojo::BindingSet<test::mojom::ConnectTestService> bindings_;
  mojo::BindingSet<test::mojom::ClassInterface> class_interface_bindings_;
  ServiceContextRefFactory ref_factory_;
  std::vector<std::unique_ptr<ServiceContextRef>> refs_;

  DISALLOW_COPY_AND_ASSIGN(ConnectTestClassApp);
};

}  // namespace service_manager

MojoResult ServiceMain(MojoHandle service_request_handle) {
  service_manager::ServiceRunner runner(
      new service_manager::ConnectTestClassApp);
  return runner.Run(service_request_handle);
}
