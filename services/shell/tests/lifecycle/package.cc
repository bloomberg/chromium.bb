// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <memory>

#include "base/bind.h"
#include "base/macros.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "services/shell/public/c/main.h"
#include "services/shell/public/cpp/service_context.h"
#include "services/shell/public/cpp/service_runner.h"
#include "services/shell/public/interfaces/service_factory.mojom.h"
#include "services/shell/tests/lifecycle/app_client.h"
#include "services/shell/tests/lifecycle/lifecycle_unittest.mojom.h"

namespace {

class PackagedApp : public shell::Service,
                    public shell::InterfaceFactory<LifecycleControl>,
                    public LifecycleControl {
 public:
  using DestructCallback = base::Callback<void(PackagedApp*)>;

  PackagedApp(shell::mojom::ServiceRequest request,
              const DestructCallback& shell_connection_closed_callback,
              const DestructCallback& destruct_callback)
      : connection_(new shell::ServiceContext(this, std::move(request))),
        shell_connection_closed_callback_(shell_connection_closed_callback),
        destruct_callback_(destruct_callback) {
    bindings_.set_connection_error_handler(base::Bind(&PackagedApp::BindingLost,
                                                      base::Unretained(this)));
  }
  ~PackagedApp() override {
    destruct_callback_.Run(this);
  }

 private:
  // shell::Service:
  bool OnConnect(const shell::Identity& remote_identity,
                 shell::InterfaceRegistry* registry) override {
    registry->AddInterface<LifecycleControl>(this);
    return true;
  }

  // shell::InterfaceFactory<LifecycleControl>
  void Create(const shell::Identity& remote_identity,
              LifecycleControlRequest request) override {
    bindings_.AddBinding(this, std::move(request));
  }

  // LifecycleControl:
  void Ping(const PingCallback& callback) override {
    callback.Run();
  }
  void GracefulQuit() override {
    shell_connection_closed_callback_.Run(this);
    delete this;
    // This will have closed all |bindings_|.
  }
  void Crash() override {
    // When multiple instances are vended from the same package instance, this
    // will cause all instances to be quit.
    exit(1);
  }
  void CloseShellConnection() override {
    shell_connection_closed_callback_.Run(this);
    connection_.reset();
    // This only closed our relationship with the shell, existing |bindings_|
    // remain active.
  }

  void BindingLost() {
    if (bindings_.empty())
      delete this;
  }

  std::unique_ptr<shell::ServiceContext> connection_;
  mojo::BindingSet<LifecycleControl> bindings_;
  // Run when this object's connection to the shell is closed.
  DestructCallback shell_connection_closed_callback_;
  // Run when this object is destructed.
  DestructCallback destruct_callback_;

  DISALLOW_COPY_AND_ASSIGN(PackagedApp);
};

class Package
    : public shell::Service,
      public shell::InterfaceFactory<shell::mojom::ServiceFactory>,
      public shell::mojom::ServiceFactory {
 public:
  Package() {}
  ~Package() override {}

  void set_runner(shell::ServiceRunner* runner) {
    app_client_.set_runner(runner);
  }

 private:
  // shell::test::AppClient:
  bool OnConnect(const shell::Identity& remote_identity,
                 shell::InterfaceRegistry* registry) override {
    registry->AddInterface<shell::mojom::ServiceFactory>(this);
    return app_client_.OnConnect(remote_identity, registry);
  }

  // shell::InterfaceFactory<shell::mojom::ServiceFactory>:
  void Create(const shell::Identity& remote_identity,
              shell::mojom::ServiceFactoryRequest request) override {
    bindings_.AddBinding(this, std::move(request));
  }

  // shell::mojom::ServiceFactory:
  void CreateService(shell::mojom::ServiceRequest request,
                     const std::string& name) override {
    ++shell_connection_refcount_;
    apps_.push_back(
        new PackagedApp(std::move(request),
                        base::Bind(&Package::AppShellConnectionClosed,
                                   base::Unretained(this)),
                        base::Bind(&Package::AppDestructed,
                                   base::Unretained(this))));
  }

  void AppShellConnectionClosed(PackagedApp* app) {
    if (!--shell_connection_refcount_)
      app_client_.CloseShellConnection();
  }

  void AppDestructed(PackagedApp* app) {
    auto it = std::find(apps_.begin(), apps_.end(), app);
    DCHECK(it != apps_.end());
    apps_.erase(it);
    if (apps_.empty() && base::MessageLoop::current()->is_running())
      base::MessageLoop::current()->QuitWhenIdle();
  }

  shell::test::AppClient app_client_;
  int shell_connection_refcount_ = 0;
  mojo::BindingSet<shell::mojom::ServiceFactory> bindings_;
  std::vector<PackagedApp*> apps_;

  DISALLOW_COPY_AND_ASSIGN(Package);
};

}  // namespace

MojoResult ServiceMain(MojoHandle service_request_handle) {
  Package* package = new Package;
  shell::ServiceRunner runner(package);
  package->set_runner(&runner);
  return runner.Run(service_request_handle);
}
