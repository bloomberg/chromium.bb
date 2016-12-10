// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <memory>

#include "base/bind.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "services/service_manager/public/c/main.h"
#include "services/service_manager/public/cpp/interface_factory.h"
#include "services/service_manager/public/cpp/interface_registry.h"
#include "services/service_manager/public/cpp/service_context.h"
#include "services/service_manager/public/cpp/service_runner.h"
#include "services/service_manager/public/interfaces/service_factory.mojom.h"
#include "services/service_manager/tests/lifecycle/app_client.h"
#include "services/service_manager/tests/lifecycle/lifecycle_unittest.mojom.h"

namespace {

class PackagedApp
    : public service_manager::Service,
      public service_manager::InterfaceFactory<
          service_manager::test::mojom::LifecycleControl>,
      public service_manager::test::mojom::LifecycleControl {
 public:
  PackagedApp(
      const base::Closure& service_manager_connection_closed_callback,
      const base::Closure& destruct_callback)
      : service_manager_connection_closed_callback_(
            service_manager_connection_closed_callback),
        destruct_callback_(destruct_callback) {
    bindings_.set_connection_error_handler(base::Bind(&PackagedApp::BindingLost,
                                                      base::Unretained(this)));
  }

  ~PackagedApp() override {}

 private:
  // service_manager::Service:
  bool OnConnect(const service_manager::ServiceInfo& remote_info,
                 service_manager::InterfaceRegistry* registry) override {
    registry->AddInterface<service_manager::test::mojom::LifecycleControl>(
        this);
    return true;
  }

  // service_manager::InterfaceFactory<LifecycleControl>
  void Create(
      const service_manager::Identity& remote_identity,
      service_manager::test::mojom::LifecycleControlRequest request) override {
    bindings_.AddBinding(this, std::move(request));
  }

  // LifecycleControl:
  void Ping(const PingCallback& callback) override {
    callback.Run();
  }

  void GracefulQuit() override {
    service_manager_connection_closed_callback_.Run();

    // Deletes |this|.
    destruct_callback_.Run();
  }

  void Crash() override {
    // When multiple instances are vended from the same package instance, this
    // will cause all instances to be quit.
    exit(1);
  }

  void CloseServiceManagerConnection() override {
    service_manager_connection_closed_callback_.Run();
    context()->QuitNow();
    // This only closed our relationship with the service manager, existing
    // |bindings_|
    // remain active.
  }

  void BindingLost() {
    if (bindings_.empty()) {
      // Deletes |this|.
      destruct_callback_.Run();
    }
  }

  mojo::BindingSet<service_manager::test::mojom::LifecycleControl> bindings_;

  // Run when this object's connection to the service manager is closed.
  base::Closure service_manager_connection_closed_callback_;
  // Run when this object is destructed.
  base::Closure destruct_callback_;

  DISALLOW_COPY_AND_ASSIGN(PackagedApp);
};

class Package : public service_manager::ForwardingService,
                public service_manager::InterfaceFactory<
                    service_manager::mojom::ServiceFactory>,
                public service_manager::mojom::ServiceFactory {
 public:
  Package() : ForwardingService(&app_client_) {}
  ~Package() override {}

 private:
  // ForwardingService:
  bool OnConnect(const service_manager::ServiceInfo& remote_info,
                 service_manager::InterfaceRegistry* registry) override {
    registry->AddInterface<service_manager::mojom::ServiceFactory>(this);
    return ForwardingService::OnConnect(remote_info, registry);
  }

  // service_manager::InterfaceFactory<service_manager::mojom::ServiceFactory>:
  void Create(const service_manager::Identity& remote_identity,
              service_manager::mojom::ServiceFactoryRequest request) override {
    bindings_.AddBinding(this, std::move(request));
  }

  // service_manager::mojom::ServiceFactory:
  void CreateService(service_manager::mojom::ServiceRequest request,
                     const std::string& name) override {
    ++service_manager_connection_refcount_;
    int id = next_id_++;
    std::unique_ptr<service_manager::ServiceContext> context =
        base::MakeUnique<service_manager::ServiceContext>(
            base::MakeUnique<PackagedApp>(
                base::Bind(&Package::AppServiceManagerConnectionClosed,
                           base::Unretained(this)),
                base::Bind(&Package::DestroyService, base::Unretained(this),
                           id)),
            std::move(request));
    service_manager::ServiceContext* raw_context = context.get();
    contexts_.insert(std::make_pair(raw_context, std::move(context)));
    id_to_context_.insert(std::make_pair(id, raw_context));
  }

  void AppServiceManagerConnectionClosed() {
    if (!--service_manager_connection_refcount_)
      app_client_.CloseServiceManagerConnection();
  }

  void DestroyService(int id) {
    auto id_it = id_to_context_.find(id);
    DCHECK(id_it != id_to_context_.end());

    auto it = contexts_.find(id_it->second);
    DCHECK(it != contexts_.end());
    contexts_.erase(it);
    id_to_context_.erase(id_it);
    if (contexts_.empty() && base::MessageLoop::current()->is_running())
      base::MessageLoop::current()->QuitWhenIdle();
  }

  service_manager::test::AppClient app_client_;
  int service_manager_connection_refcount_ = 0;
  mojo::BindingSet<service_manager::mojom::ServiceFactory> bindings_;

  using ServiceContextMap =
      std::map<service_manager::ServiceContext*,
               std::unique_ptr<service_manager::ServiceContext>>;
  ServiceContextMap contexts_;

  int next_id_ = 0;
  std::map<int, service_manager::ServiceContext*> id_to_context_;

  DISALLOW_COPY_AND_ASSIGN(Package);
};

}  // namespace

MojoResult ServiceMain(MojoHandle service_request_handle) {
  service_manager::ServiceRunner runner(new Package);
  return runner.Run(service_request_handle);
}
