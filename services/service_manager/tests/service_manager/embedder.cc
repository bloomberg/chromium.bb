// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/message_loop/message_loop.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "services/service_manager/public/c/main.h"
#include "services/service_manager/public/cpp/interface_factory.h"
#include "services/service_manager/public/cpp/interface_registry.h"
#include "services/service_manager/public/cpp/service.h"
#include "services/service_manager/public/cpp/service_context.h"
#include "services/service_manager/public/cpp/service_runner.h"
#include "services/service_manager/public/interfaces/service_factory.mojom.h"
#include "services/service_manager/public/interfaces/service_manager.mojom.h"

namespace {

class Singleton : public service_manager::Service {
 public:
  explicit Singleton() {}
  ~Singleton() override {}

 private:
  // service_manager::Service:
  bool OnConnect(const service_manager::ServiceInfo& remote_info,
                 service_manager::InterfaceRegistry* registry) override {
    return false;
  }

  DISALLOW_COPY_AND_ASSIGN(Singleton);
};

class Embedder : public service_manager::Service,
                 public service_manager::InterfaceFactory<
                     service_manager::mojom::ServiceFactory>,
                 public service_manager::mojom::ServiceFactory {
 public:
  Embedder() {}
  ~Embedder() override {}

 private:
  // service_manager::Service:
  bool OnConnect(const service_manager::ServiceInfo& remote_info,
                 service_manager::InterfaceRegistry* registry) override {
    registry->AddInterface<service_manager::mojom::ServiceFactory>(this);
    return true;
  }

  bool OnServiceManagerConnectionLost() override {
    base::MessageLoop::current()->QuitWhenIdle();
    return true;
  }

  // service_manager::InterfaceFactory<ServiceFactory>:
  void Create(const service_manager::Identity& remote_identity,
              service_manager::mojom::ServiceFactoryRequest request) override {
    service_factory_bindings_.AddBinding(this, std::move(request));
  }

  // mojom::ServiceFactory:
  void CreateService(service_manager::mojom::ServiceRequest request,
                     const std::string& name) override {
    if (name == "service_manager_unittest_singleton") {
      context_.reset(new service_manager::ServiceContext(
          base::MakeUnique<Singleton>(), std::move(request)));
    }
  }

  std::unique_ptr<service_manager::ServiceContext> context_;
  mojo::BindingSet<service_manager::mojom::ServiceFactory>
      service_factory_bindings_;

  DISALLOW_COPY_AND_ASSIGN(Embedder);
};

}  // namespace

MojoResult ServiceMain(MojoHandle service_request_handle) {
  service_manager::ServiceRunner runner(new Embedder);
  return runner.Run(service_request_handle);
}
