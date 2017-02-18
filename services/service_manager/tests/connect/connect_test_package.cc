// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/threading/simple_thread.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "services/service_manager/public/c/main.h"
#include "services/service_manager/public/cpp/connector.h"
#include "services/service_manager/public/cpp/interface_factory.h"
#include "services/service_manager/public/cpp/interface_registry.h"
#include "services/service_manager/public/cpp/service.h"
#include "services/service_manager/public/cpp/service_context.h"
#include "services/service_manager/public/cpp/service_runner.h"
#include "services/service_manager/public/interfaces/service_factory.mojom.h"
#include "services/service_manager/tests/connect/connect_test.mojom.h"

// Tests that multiple services can be packaged in a single service by
// implementing ServiceFactory; that these services can be specified by
// the package's manifest and are thus registered with the PackageManager.

namespace service_manager {

using GetTitleCallback = test::mojom::ConnectTestService::GetTitleCallback;

class ProvidedService
    : public Service,
      public InterfaceFactory<test::mojom::ConnectTestService>,
      public InterfaceFactory<test::mojom::BlockedInterface>,
      public InterfaceFactory<test::mojom::UserIdTest>,
      public test::mojom::ConnectTestService,
      public test::mojom::BlockedInterface,
      public test::mojom::UserIdTest,
      public base::SimpleThread {
 public:
  ProvidedService(const std::string& title, mojom::ServiceRequest request)
      : base::SimpleThread(title),
        title_(title),
        request_(std::move(request)) {
    Start();
  }

  ~ProvidedService() override {
    Join();
  }

 private:
  // service_manager::Service:
  void OnStart() override {
    bindings_.set_connection_error_handler(
        base::Bind(&ProvidedService::OnConnectionError,
                   base::Unretained(this)));
  }

  bool OnConnect(const ServiceInfo& remote_info,
                 InterfaceRegistry* registry) override {
    registry->AddInterface<test::mojom::ConnectTestService>(this);
    registry->AddInterface<test::mojom::BlockedInterface>(this);
    registry->AddInterface<test::mojom::UserIdTest>(this);

    test::mojom::ConnectionStatePtr state(test::mojom::ConnectionState::New());
    state->connection_remote_name = remote_info.identity.name();
    state->connection_remote_userid = remote_info.identity.user_id();
    state->initialize_local_name = context()->identity().name();
    state->initialize_userid = context()->identity().user_id();

    context()->connector()->BindInterface(remote_info.identity, &caller_);
    caller_->ConnectionAccepted(std::move(state));

    return true;
  }

  // InterfaceFactory<test::mojom::ConnectTestService>:
  void Create(const Identity& remote_identity,
              test::mojom::ConnectTestServiceRequest request) override {
    bindings_.AddBinding(this, std::move(request));
  }

  // InterfaceFactory<test::mojom::BlockedInterface>:
  void Create(const Identity& remote_identity,
              test::mojom::BlockedInterfaceRequest request) override {
    blocked_bindings_.AddBinding(this, std::move(request));
  }

  // InterfaceFactory<test::mojom::UserIdTest>:
  void Create(const Identity& remote_identity,
              test::mojom::UserIdTestRequest request) override {
    user_id_test_bindings_.AddBinding(this, std::move(request));
  }

  // test::mojom::ConnectTestService:
  void GetTitle(const GetTitleCallback& callback) override {
    callback.Run(title_);
  }

  void GetInstance(const GetInstanceCallback& callback) override {
    callback.Run(context()->identity().instance());
  }

  // test::mojom::BlockedInterface:
  void GetTitleBlocked(const GetTitleBlockedCallback& callback) override {
    callback.Run("Called Blocked Interface!");
  }

  // test::mojom::UserIdTest:
  void ConnectToClassAppAsDifferentUser(
      const service_manager::Identity& target,
      const ConnectToClassAppAsDifferentUserCallback& callback) override {
    std::unique_ptr<Connection> connection =
        context()->connector()->Connect(target);
    {
      base::RunLoop loop;
      connection->AddConnectionCompletedClosure(loop.QuitClosure());
      base::MessageLoop::ScopedNestableTaskAllower allow(
          base::MessageLoop::current());
      loop.Run();
    }
    callback.Run(static_cast<int32_t>(connection->GetResult()),
                 connection->GetRemoteIdentity());
  }

  // base::SimpleThread:
  void Run() override {
    ServiceRunner(new ForwardingService(this)).Run(
        request_.PassMessagePipe().release().value(), false);
    caller_.reset();
    bindings_.CloseAllBindings();
    blocked_bindings_.CloseAllBindings();
    user_id_test_bindings_.CloseAllBindings();
  }

  void OnConnectionError() {
    if (bindings_.empty())
      base::MessageLoop::current()->QuitWhenIdle();
  }

  const std::string title_;
  mojom::ServiceRequest request_;
  test::mojom::ExposedInterfacePtr caller_;
  mojo::BindingSet<test::mojom::ConnectTestService> bindings_;
  mojo::BindingSet<test::mojom::BlockedInterface> blocked_bindings_;
  mojo::BindingSet<test::mojom::UserIdTest> user_id_test_bindings_;

  DISALLOW_COPY_AND_ASSIGN(ProvidedService);
};

class ConnectTestService
    : public Service,
      public InterfaceFactory<mojom::ServiceFactory>,
      public InterfaceFactory<test::mojom::ConnectTestService>,
      public mojom::ServiceFactory,
      public test::mojom::ConnectTestService {
 public:
  ConnectTestService() {}
  ~ConnectTestService() override {}

 private:
  // service_manager::Service:
  void OnStart() override {
    base::Closure error_handler =
        base::Bind(&ConnectTestService::OnConnectionError,
                   base::Unretained(this));
    bindings_.set_connection_error_handler(error_handler);
    service_factory_bindings_.set_connection_error_handler(error_handler);
  }

  bool OnConnect(const ServiceInfo& remote_info,
                 InterfaceRegistry* registry) override {
    registry->AddInterface<ServiceFactory>(this);
    registry->AddInterface<test::mojom::ConnectTestService>(this);
    return true;
  }

  bool OnServiceManagerConnectionLost() override {
    provided_services_.clear();
    return true;
  }

  // InterfaceFactory<mojom::ServiceFactory>:
  void Create(const Identity& remote_identity,
              mojom::ServiceFactoryRequest request) override {
    service_factory_bindings_.AddBinding(this, std::move(request));
  }

  // InterfaceFactory<test::mojom::ConnectTestService>:
  void Create(const Identity& remote_identity,
              test::mojom::ConnectTestServiceRequest request) override {
    bindings_.AddBinding(this, std::move(request));
  }

  // mojom::ServiceFactory:
  void CreateService(mojom::ServiceRequest request,
                     const std::string& name) override {
    if (name == "connect_test_a") {
      provided_services_.emplace_back(
          base::MakeUnique<ProvidedService>("A", std::move(request)));
    } else if (name == "connect_test_b") {
      provided_services_.emplace_back(
          base::MakeUnique<ProvidedService>("B", std::move(request)));
    }
  }

  // test::mojom::ConnectTestService:
  void GetTitle(const GetTitleCallback& callback) override {
    callback.Run("ROOT");
  }

  void GetInstance(const GetInstanceCallback& callback) override {
    callback.Run(context()->identity().instance());
  }

  void OnConnectionError() {
    if (bindings_.empty() && service_factory_bindings_.empty())
      context()->RequestQuit();
  }

  std::vector<std::unique_ptr<Service>> delegates_;
  mojo::BindingSet<mojom::ServiceFactory> service_factory_bindings_;
  mojo::BindingSet<test::mojom::ConnectTestService> bindings_;
  std::list<std::unique_ptr<ProvidedService>> provided_services_;

  DISALLOW_COPY_AND_ASSIGN(ConnectTestService);
};

}  // namespace service_manager

MojoResult ServiceMain(MojoHandle service_request_handle) {
  service_manager::ServiceRunner runner(
      new service_manager::ConnectTestService);
  return runner.Run(service_request_handle);
}
