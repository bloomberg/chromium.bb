// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "services/shell/public/c/main.h"
#include "services/shell/public/cpp/connector.h"
#include "services/shell/public/cpp/interface_factory.h"
#include "services/shell/public/cpp/interface_registry.h"
#include "services/shell/public/cpp/service.h"
#include "services/shell/public/cpp/service_runner.h"
#include "services/shell/tests/lifecycle/lifecycle_unittest.mojom.h"

namespace {

void QuitLoop(base::RunLoop* loop) {
  loop->Quit();
}

class Parent : public shell::Service,
               public shell::InterfaceFactory<shell::test::mojom::Parent>,
               public shell::test::mojom::Parent {
 public:
  Parent() {}
  ~Parent() override {
    child_connection_.reset();
    parent_bindings_.CloseAllBindings();
  }

 private:
  // Service:
  bool OnConnect(const shell::Identity& remote_identity,
                 shell::InterfaceRegistry* registry) override {
    registry->AddInterface<shell::test::mojom::Parent>(this);
    return true;
  }

  // InterfaceFactory<shell::test::mojom::Parent>:
  void Create(const shell::Identity& remote_identity,
              shell::test::mojom::ParentRequest request) override {
    parent_bindings_.AddBinding(this, std::move(request));
  }

  // Parent:
  void ConnectToChild(const ConnectToChildCallback& callback) override {
    child_connection_ = connector()->Connect("service:lifecycle_unittest_app");
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

  std::unique_ptr<shell::Connection> child_connection_;
  mojo::BindingSet<shell::test::mojom::Parent> parent_bindings_;

  DISALLOW_COPY_AND_ASSIGN(Parent);
};

}  // namespace

MojoResult ServiceMain(MojoHandle service_request_handle) {
  Parent* parent = new Parent;
  return shell::ServiceRunner(parent).Run(service_request_handle);
}
