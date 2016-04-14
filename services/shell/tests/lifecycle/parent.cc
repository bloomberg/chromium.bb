// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "mojo/public/c/system/main.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "services/shell/public/cpp/application_runner.h"
#include "services/shell/public/cpp/connector.h"
#include "services/shell/public/cpp/shell_client.h"
#include "services/shell/tests/lifecycle/lifecycle_unittest.mojom.h"

namespace {

void QuitLoop(base::RunLoop* loop) {
  loop->Quit();
}

class Parent : public shell::ShellClient,
               public shell::InterfaceFactory<shell::test::mojom::Parent>,
               public shell::test::mojom::Parent {
 public:
  Parent() {}
  ~Parent() override {
    connector_ = nullptr;
    child_connection_.reset();
    parent_bindings_.CloseAllBindings();
  }

 private:
  // ShellClient:
  void Initialize(shell::Connector* connector,
                  const shell::Identity& identity,
                  uint32_t id) override {
    connector_ = connector;
  }
  bool AcceptConnection(shell::Connection* connection) override {
    connection->AddInterface<shell::test::mojom::Parent>(this);
    return true;
  }

  // InterfaceFactory<shell::test::mojom::Parent>:
  void Create(shell::Connection* connection,
              shell::test::mojom::ParentRequest request) override {
    parent_bindings_.AddBinding(this, std::move(request));
  }

  // Parent:
  void ConnectToChild(const ConnectToChildCallback& callback) override {
    child_connection_ = connector_->Connect("mojo:lifecycle_unittest_app");
    shell::test::mojom::LifecycleControlPtr lifecycle;
    child_connection_->GetInterface(&lifecycle);
    {
      base::RunLoop loop;
      lifecycle->Ping(base::Bind(&QuitLoop, &loop));
      base::MessageLoop::ScopedNestableTaskAllower allow(
          base::MessageLoop::current());
      loop.Run();
    }
    callback.Run();
  }
  void Quit() override {
    base::MessageLoop::current()->QuitWhenIdle();
  }

  shell::Connector* connector_;
  std::unique_ptr<shell::Connection> child_connection_;
  mojo::BindingSet<shell::test::mojom::Parent> parent_bindings_;

  DISALLOW_COPY_AND_ASSIGN(Parent);
};

}  // namespace

MojoResult MojoMain(MojoHandle shell_handle) {
  Parent* parent = new Parent;
  return shell::ApplicationRunner(parent).Run(shell_handle);
}
