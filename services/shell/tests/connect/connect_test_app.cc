// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/bind.h"
#include "base/guid.h"
#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "mojo/public/c/system/main.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "services/shell/public/cpp/application_runner.h"
#include "services/shell/public/cpp/connector.h"
#include "services/shell/public/cpp/interface_factory.h"
#include "services/shell/public/cpp/shell_client.h"
#include "services/shell/public/interfaces/connector.mojom.h"
#include "services/shell/tests/connect/connect_test.mojom.h"

namespace shell {

namespace {

void QuitLoop(base::RunLoop* loop) {
  loop->Quit();
}

void ReceiveString(std::string* string,
                   base::RunLoop* loop,
                   mojo::String response) {
  *string = response;
  loop->Quit();
}

}  // namespace

using GetTitleCallback = test::mojom::ConnectTestService::GetTitleCallback;

class ConnectTestApp : public ShellClient,
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
  // shell::ShellClient:
  void Initialize(Connector* connector, const Identity& identity,
                  uint32_t id) override {
    connector_ = connector;
    identity_ = identity;
    id_ = id;
    bindings_.set_connection_error_handler(
        base::Bind(&ConnectTestApp::OnConnectionError,
                   base::Unretained(this)));
    standalone_bindings_.set_connection_error_handler(
        base::Bind(&ConnectTestApp::OnConnectionError,
                   base::Unretained(this)));
  }
  bool AcceptConnection(Connection* connection) override {
    connection->AddInterface<test::mojom::ConnectTestService>(this);
    connection->AddInterface<test::mojom::StandaloneApp>(this);
    connection->AddInterface<test::mojom::BlockedInterface>(this);
    connection->AddInterface<test::mojom::UserIdTest>(this);

    uint32_t remote_id = connection->GetRemoteInstanceID();
    test::mojom::ConnectionStatePtr state(test::mojom::ConnectionState::New());
    state->connection_local_name = connection->GetConnectionName();
    state->connection_remote_name = connection->GetRemoteIdentity().name();
    state->connection_remote_userid = connection->GetRemoteIdentity().user_id();
    state->connection_remote_id = remote_id;
    state->initialize_local_name = identity_.name();
    state->initialize_id = id_;
    state->initialize_userid = identity_.user_id();
    connection->GetInterface(&caller_);
    caller_->ConnectionAccepted(std::move(state));

    return true;
  }

  // InterfaceFactory<test::mojom::ConnectTestService>:
  void Create(Connection* connection,
              test::mojom::ConnectTestServiceRequest request) override {
    bindings_.AddBinding(this, std::move(request));
  }

  // InterfaceFactory<test::mojom::StandaloneApp>:
  void Create(Connection* connection,
              test::mojom::StandaloneAppRequest request) override {
    standalone_bindings_.AddBinding(this, std::move(request));
  }

  // InterfaceFactory<test::mojom::BlockedInterface>:
  void Create(Connection* connection,
              test::mojom::BlockedInterfaceRequest request) override {
    blocked_bindings_.AddBinding(this, std::move(request));
  }

  // InterfaceFactory<test::mojom::UserIdTest>:
  void Create(Connection* connection,
              test::mojom::UserIdTestRequest request) override {
    user_id_test_bindings_.AddBinding(this, std::move(request));
  }

  // test::mojom::ConnectTestService:
  void GetTitle(const GetTitleCallback& callback) override {
    callback.Run("APP");
  }
  void GetInstance(const GetInstanceCallback& callback) override {
    callback.Run(identity_.instance());
  }

  // test::mojom::StandaloneApp:
  void ConnectToAllowedAppInBlockedPackage(
      const ConnectToAllowedAppInBlockedPackageCallback& callback) override {
    base::RunLoop run_loop;
    std::unique_ptr<Connection> connection =
        connector_->Connect("mojo:connect_test_a");
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
        connector_->Connect("mojo:connect_test_class_app");
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
      mojom::IdentityPtr target,
      const ConnectToClassAppAsDifferentUserCallback& callback) override {
    Connector::ConnectParams params(target.To<Identity>());
    std::unique_ptr<Connection> connection = connector_->Connect(&params);
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

  void OnConnectionBlocked(
      const ConnectToAllowedAppInBlockedPackageCallback& callback,
      base::RunLoop* run_loop) {
    callback.Run("uninitialized");
    run_loop->Quit();
  }

  void OnGotTitle(
      const ConnectToAllowedAppInBlockedPackageCallback& callback,
      base::RunLoop* run_loop,
      mojo::String title) {
    callback.Run(title);
    run_loop->Quit();
  }

  void OnConnectionError() {
    if (bindings_.empty() && standalone_bindings_.empty())
      base::MessageLoop::current()->QuitWhenIdle();
  }

  Connector* connector_ = nullptr;
  Identity identity_;
  uint32_t id_ = mojom::kInvalidInstanceID;
  mojo::BindingSet<test::mojom::ConnectTestService> bindings_;
  mojo::BindingSet<test::mojom::StandaloneApp> standalone_bindings_;
  mojo::BindingSet<test::mojom::BlockedInterface> blocked_bindings_;
  mojo::BindingSet<test::mojom::UserIdTest> user_id_test_bindings_;
  test::mojom::ExposedInterfacePtr caller_;

  DISALLOW_COPY_AND_ASSIGN(ConnectTestApp);
};

}  // namespace shell

MojoResult MojoMain(MojoHandle shell_handle) {
  MojoResult rv =
      shell::ApplicationRunner(new shell::ConnectTestApp).Run(shell_handle);
  return rv;
}
