// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/bind.h"
#include "base/guid.h"
#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "services/service_manager/public/c/main.h"
#include "services/service_manager/public/cpp/connector.h"
#include "services/service_manager/public/cpp/interface_factory.h"
#include "services/service_manager/public/cpp/interface_registry.h"
#include "services/service_manager/public/cpp/service.h"
#include "services/service_manager/public/cpp/service_context.h"
#include "services/service_manager/public/cpp/service_runner.h"
#include "services/service_manager/public/interfaces/connector.mojom.h"
#include "services/service_manager/tests/connect/connect_test.mojom.h"

namespace service_manager {

namespace {

void QuitLoop(base::RunLoop* loop) {
  loop->Quit();
}

void ReceiveString(std::string* string,
                   base::RunLoop* loop,
                   const std::string& response) {
  *string = response;
  loop->Quit();
}

}  // namespace

using GetTitleCallback = test::mojom::ConnectTestService::GetTitleCallback;

class ConnectTestApp : public Service,
                       public InterfaceFactory<test::mojom::ConnectTestService>,
                       public InterfaceFactory<test::mojom::StandaloneApp>,
                       public InterfaceFactory<test::mojom::BlockedInterface>,
                       public InterfaceFactory<test::mojom::UserIdTest>,
                       public test::mojom::ConnectTestService,
                       public test::mojom::StandaloneApp,
                       public test::mojom::BlockedInterface,
                       public test::mojom::UserIdTest {
 public:
  ConnectTestApp() {}
  ~ConnectTestApp() override {}

 private:
  // service_manager::Service:
  void OnStart() override {
    bindings_.set_connection_error_handler(
        base::Bind(&ConnectTestApp::OnConnectionError,
                   base::Unretained(this)));
    standalone_bindings_.set_connection_error_handler(
        base::Bind(&ConnectTestApp::OnConnectionError,
                   base::Unretained(this)));
  }

  bool OnConnect(const ServiceInfo& remote_info,
                 InterfaceRegistry* registry) override {
    registry->AddInterface<test::mojom::ConnectTestService>(this);
    registry->AddInterface<test::mojom::StandaloneApp>(this);
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

  // InterfaceFactory<test::mojom::StandaloneApp>:
  void Create(const Identity& remote_identity,
              test::mojom::StandaloneAppRequest request) override {
    standalone_bindings_.AddBinding(this, std::move(request));
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
    callback.Run("APP");
  }
  void GetInstance(const GetInstanceCallback& callback) override {
    callback.Run(context()->identity().instance());
  }

  // test::mojom::StandaloneApp:
  void ConnectToAllowedAppInBlockedPackage(
      const ConnectToAllowedAppInBlockedPackageCallback& callback) override {
    base::RunLoop run_loop;
    std::unique_ptr<Connection> connection =
        context()->connector()->Connect("connect_test_a");
    connection->SetConnectionLostClosure(
        base::Bind(&ConnectTestApp::OnConnectionBlocked,
                   base::Unretained(this), callback, &run_loop));
    test::mojom::ConnectTestServicePtr test_service;
    connection->GetInterface(&test_service);
    test_service->GetTitle(
        base::Bind(&ConnectTestApp::OnGotTitle, base::Unretained(this),
                   callback, &run_loop));
    {
      // This message is dispatched as a task on the same run loop, so we need
      // to allow nesting in order to pump additional signals.
      base::MessageLoop::ScopedNestableTaskAllower allow(
          base::MessageLoop::current());
      run_loop.Run();
    }
  }
  void ConnectToClassInterface(
      const ConnectToClassInterfaceCallback& callback) override {
    std::unique_ptr<Connection> connection =
        context()->connector()->Connect("connect_test_class_app");
    test::mojom::ClassInterfacePtr class_interface;
    connection->GetInterface(&class_interface);
    std::string ping_response;
    {
      base::RunLoop loop;
      class_interface->Ping(base::Bind(&ReceiveString, &ping_response, &loop));
      base::MessageLoop::ScopedNestableTaskAllower allow(
          base::MessageLoop::current());
      loop.Run();
    }
    test::mojom::ConnectTestServicePtr service;
    connection->GetInterface(&service);
    std::string title_response;
    {
      base::RunLoop loop;
      service->GetTitle(base::Bind(&ReceiveString, &title_response, &loop));
      base::MessageLoop::ScopedNestableTaskAllower allow(
          base::MessageLoop::current());
      loop.Run();
    }
    callback.Run(ping_response, title_response);
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
      connection->AddConnectionCompletedClosure(base::Bind(&QuitLoop, &loop));
      base::MessageLoop::ScopedNestableTaskAllower allow(
          base::MessageLoop::current());
      loop.Run();
    }
    callback.Run(static_cast<int32_t>(connection->GetResult()),
                 connection->GetRemoteIdentity());
  }

  void OnConnectionBlocked(
      const ConnectToAllowedAppInBlockedPackageCallback& callback,
      base::RunLoop* run_loop) {
    callback.Run("uninitialized");
    run_loop->Quit();
  }

  void OnGotTitle(
      const ConnectToAllowedAppInBlockedPackageCallback& callback,
      base::RunLoop* run_loop,
      const std::string& title) {
    callback.Run(title);
    run_loop->Quit();
  }

  void OnConnectionError() {
    if (bindings_.empty() && standalone_bindings_.empty())
      base::MessageLoop::current()->QuitWhenIdle();
  }

  mojo::BindingSet<test::mojom::ConnectTestService> bindings_;
  mojo::BindingSet<test::mojom::StandaloneApp> standalone_bindings_;
  mojo::BindingSet<test::mojom::BlockedInterface> blocked_bindings_;
  mojo::BindingSet<test::mojom::UserIdTest> user_id_test_bindings_;
  test::mojom::ExposedInterfacePtr caller_;

  DISALLOW_COPY_AND_ASSIGN(ConnectTestApp);
};

}  // namespace service_manager

MojoResult ServiceMain(MojoHandle service_request_handle) {
  service_manager::ServiceRunner runner(new service_manager::ConnectTestApp);
  return runner.Run(service_request_handle);
}
