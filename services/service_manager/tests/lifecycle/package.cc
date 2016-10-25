// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <memory>

#include "base/bind.h"
#include "base/macros.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "services/service_manager/public/c/main.h"
#include "services/service_manager/public/cpp/service_context.h"
#include "services/service_manager/public/cpp/service_runner.h"
#include "services/service_manager/public/interfaces/service_factory.mojom.h"
#include "services/service_manager/tests/lifecycle/app_client.h"
#include "services/service_manager/tests/lifecycle/lifecycle_unittest.mojom.h"

namespace {

class PackagedApp : public service_manager::Service,
                    public service_manager::InterfaceFactory<LifecycleControl>,
                    public LifecycleControl {
 public:
  using DestructCallback = base::Callback<void(PackagedApp*)>;

  PackagedApp(
      service_manager::mojom::ServiceRequest request,
      const DestructCallback& service_manager_connection_closed_callback,
      const DestructCallback& destruct_callback)
      : connection_(
            new service_manager::ServiceContext(this, std::move(request))),
        service_manager_connection_closed_callback_(
            service_manager_connection_closed_callback),
        destruct_callback_(destruct_callback) {
    bindings_.set_connection_error_handler(base::Bind(&PackagedApp::BindingLost,
                                                      base::Unretained(this)));
  }
  ~PackagedApp() override {
    destruct_callback_.Run(this);
  }

 private:
  // service_manager::Service:
  bool OnConnect(const service_manager::ServiceInfo& remote_info,
                 service_manager::InterfaceRegistry* registry) override {
    registry->AddInterface<LifecycleControl>(this);
    return true;
  }

  // service_manager::InterfaceFactory<LifecycleControl>
  void Create(const service_manager::Identity& remote_identity,
              LifecycleControlRequest request) override {
    bindings_.AddBinding(this, std::move(request));
  }

  // LifecycleControl:
  void Ping(const PingCallback& callback) override {
    callback.Run();
  }
  void GracefulQuit() override {
    service_manager_connection_closed_callback_.Run(this);
    delete this;
    // This will have closed all |bindings_|.
  }
  void Crash() override {
    // When multiple instances are vended from the same package instance, this
    // will cause all instances to be quit.
    exit(1);
  }
  void CloseServiceManagerConnection() override {
    service_manager_connection_closed_callback_.Run(this);
    connection_.reset();
    // This only closed our relationship with the service manager, existing
    // |bindings_|
    // remain active.
  }

  void BindingLost() {
    if (bindings_.empty())
      delete this;
  }

  std::unique_ptr<service_manager::ServiceContext> connection_;
  mojo::BindingSet<LifecycleControl> bindings_;
  // Run when this object's connection to the service manager is closed.
  DestructCallback service_manager_connection_closed_callback_;
  // Run when this object is destructed.
  DestructCallback destruct_callback_;

  DISALLOW_COPY_AND_ASSIGN(PackagedApp);
};

class Package : public service_manager::Service,
                public service_manager::InterfaceFactory<
                    service_manager::mojom::ServiceFactory>,
                public service_manager::mojom::ServiceFactory {
 public:
  Package() {}
  ~Package() override {}

  void set_runner(service_manager::ServiceRunner* runner) {
    app_client_.set_runner(runner);
  }

 private:
  // service_manager::test::AppClient:
  bool OnConnect(const service_manager::ServiceInfo& remote_info,
                 service_manager::InterfaceRegistry* registry) override {
    registry->AddInterface<service_manager::mojom::ServiceFactory>(this);
    return app_client_.OnConnect(remote_info, registry);
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
    apps_.push_back(new PackagedApp(
        std::move(request),
        base::Bind(&Package::AppServiceManagerConnectionClosed,
                   base::Unretained(this)),
        base::Bind(&Package::AppDestructed, base::Unretained(this))));
  }

  void AppServiceManagerConnectionClosed(PackagedApp* app) {
    if (!--service_manager_connection_refcount_)
      app_client_.CloseServiceManagerConnection();
  }

  void AppDestructed(PackagedApp* app) {
    auto it = std::find(apps_.begin(), apps_.end(), app);
    DCHECK(it != apps_.end());
    apps_.erase(it);
    if (apps_.empty() && base::MessageLoop::current()->is_running())
      base::MessageLoop::current()->QuitWhenIdle();
  }

  service_manager::test::AppClient app_client_;
  int service_manager_connection_refcount_ = 0;
  mojo::BindingSet<service_manager::mojom::ServiceFactory> bindings_;
  std::vector<PackagedApp*> apps_;

  DISALLOW_COPY_AND_ASSIGN(Package);
};

}  // namespace

MojoResult ServiceMain(MojoHandle service_request_handle) {
  Package* package = new Package;
  service_manager::ServiceRunner runner(package);
  package->set_runner(&runner);
  return runner.Run(service_request_handle);
}
