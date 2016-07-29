// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/threading/simple_thread.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "services/shell/public/c/main.h"
#include "services/shell/public/cpp/connector.h"
#include "services/shell/public/cpp/interface_factory.h"
#include "services/shell/public/cpp/service.h"
#include "services/shell/public/cpp/service_runner.h"
#include "services/shell/public/interfaces/service_factory.mojom.h"
#include "services/shell/tests/connect/connect_test.mojom.h"

// Tests that multiple applications can be packaged in a single Mojo application
// implementing ServiceFactory; that these applications can be specified by
// the package's manifest and are thus registered with the PackageManager.

namespace shell {

namespace {

void QuitLoop(base::RunLoop* loop) {
  loop->Quit();
}

}  // namespace

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
  ProvidedService(const std::string& title,
                      mojom::ServiceRequest request)
      : base::SimpleThread(title),
        title_(title),
        request_(std::move(request)) {
    Start();
  }
  ~ProvidedService() override {
    Join();
  }

 private:
  // shell::Service:
  void OnStart(const Identity& identity) override {
    identity_ = identity;
    bindings_.set_connection_error_handler(
        base::Bind(&ProvidedService::OnConnectionError,
                   base::Unretained(this)));
  }
  bool OnConnect(Connection* connection) override {
    connection->AddInterface<test::mojom::ConnectTestService>(this);
    connection->AddInterface<test::mojom::BlockedInterface>(this);
    connection->AddInterface<test::mojom::UserIdTest>(this);

    uint32_t remote_id = connection->GetRemoteInstanceID();
    test::mojom::ConnectionStatePtr state(test::mojom::ConnectionState::New());
    state->connection_local_name = connection->GetConnectionName();
    state->connection_remote_name = connection->GetRemoteIdentity().name();
    state->connection_remote_userid = connection->GetRemoteIdentity().user_id();
    state->connection_remote_id = remote_id;
    state->initialize_local_name = identity_.name();
    state->initialize_userid = identity_.user_id();
    connection->GetInterface(&caller_);
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
    callback.Run(identity_.instance());
  }

  // test::mojom::BlockedInterface:
  void GetTitleBlocked(const GetTitleBlockedCallback& callback) override {
    callback.Run("Called Blocked Interface!");
  }

  // test::mojom::UserIdTest:
  void ConnectToClassAppAsDifferentUser(
      mojom::IdentityPtr target,
      const ConnectToClassAppAsDifferentUserCallback& callback) override {
    Connector::ConnectParams params(target.To<Identity>());
    std::unique_ptr<Connection> connection =
        connector()->Connect(&params);
    {
      base::RunLoop loop;
      connection->AddConnectionCompletedClosure(base::Bind(&QuitLoop, &loop));
      base::MessageLoop::ScopedNestableTaskAllower allow(
          base::MessageLoop::current());
      loop.Run();
    }
    callback.Run(static_cast<int32_t>(connection->GetResult()),
                 mojom::Identity::From(connection->GetRemoteIdentity()));
  }

  // base::SimpleThread:
  void Run() override {
    ServiceRunner(this).Run(request_.PassMessagePipe().release().value(),
                            false);
    delete this;
  }

  void OnConnectionError() {
    if (bindings_.empty())
      base::MessageLoop::current()->QuitWhenIdle();
  }

  Identity identity_;
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
  // shell::Service:
  void OnStart(const Identity& identity) override {
    identity_ = identity;
    bindings_.set_connection_error_handler(
        base::Bind(&ConnectTestService::OnConnectionError,
                   base::Unretained(this)));
  }
  bool OnConnect(Connection* connection) override {
    connection->AddInterface<ServiceFactory>(this);
    connection->AddInterface<test::mojom::ConnectTestService>(this);
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
                         const mojo::String& name) override {
    if (name == "mojo:connect_test_a")
      new ProvidedService("A", std::move(request));
    else if (name == "mojo:connect_test_b")
      new ProvidedService("B", std::move(request));
  }

  // test::mojom::ConnectTestService:
  void GetTitle(const GetTitleCallback& callback) override {
    callback.Run("ROOT");
  }
  void GetInstance(const GetInstanceCallback& callback) override {
    callback.Run(identity_.instance());
  }

  void OnConnectionError() {
    if (bindings_.empty())
      base::MessageLoop::current()->QuitWhenIdle();
  }

  Identity identity_;
  std::vector<std::unique_ptr<Service>> delegates_;
  mojo::BindingSet<mojom::ServiceFactory> service_factory_bindings_;
  mojo::BindingSet<test::mojom::ConnectTestService> bindings_;

  DISALLOW_COPY_AND_ASSIGN(ConnectTestService);
};

}  // namespace shell

MojoResult ServiceMain(MojoHandle service_request_handle) {
  shell::ServiceRunner runner(new shell::ConnectTestService);
  return runner.Run(service_request_handle);
}
