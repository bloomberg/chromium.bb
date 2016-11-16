// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/bind.h"
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
#include "services/service_manager/tests/lifecycle/lifecycle_unittest.mojom.h"

namespace {

void QuitLoop(base::RunLoop* loop) {
  loop->Quit();
}

class Parent : public service_manager::Service,
               public service_manager::InterfaceFactory<
                   service_manager::test::mojom::Parent>,
               public service_manager::test::mojom::Parent {
 public:
  Parent() {}
  ~Parent() override {
    child_connection_.reset();
    parent_bindings_.CloseAllBindings();
  }

 private:
  // Service:
  bool OnConnect(const service_manager::ServiceInfo& remote_info,
                 service_manager::InterfaceRegistry* registry) override {
    registry->AddInterface<service_manager::test::mojom::Parent>(this);
    return true;
  }

  // InterfaceFactory<service_manager::test::mojom::Parent>:
  void Create(const service_manager::Identity& remote_identity,
              service_manager::test::mojom::ParentRequest request) override {
    parent_bindings_.AddBinding(this, std::move(request));
  }

  // service_manager::test::mojom::Parent:
  void ConnectToChild(const ConnectToChildCallback& callback) override {
    child_connection_ =
        context()->connector()->Connect("lifecycle_unittest_app");
    service_manager::test::mojom::LifecycleControlPtr lifecycle;
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

  std::unique_ptr<service_manager::Connection> child_connection_;
  mojo::BindingSet<service_manager::test::mojom::Parent> parent_bindings_;

  DISALLOW_COPY_AND_ASSIGN(Parent);
};

}  // namespace

MojoResult ServiceMain(MojoHandle service_request_handle) {
  Parent* parent = new Parent;
  return service_manager::ServiceRunner(parent).Run(service_request_handle);
}
