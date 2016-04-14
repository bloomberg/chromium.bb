// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <memory>

#include "base/bind.h"
#include "base/macros.h"
#include "mojo/public/c/system/main.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "services/shell/public/cpp/application_runner.h"
#include "services/shell/public/cpp/shell_connection.h"
#include "services/shell/public/interfaces/shell_client_factory.mojom.h"
#include "services/shell/tests/lifecycle/app_client.h"
#include "services/shell/tests/lifecycle/lifecycle_unittest.mojom.h"

namespace {

class PackagedApp : public shell::ShellClient,
                    public shell::InterfaceFactory<LifecycleControl>,
                    public LifecycleControl {
 public:
  using DestructCallback = base::Callback<void(PackagedApp*)>;

  PackagedApp(shell::mojom::ShellClientRequest request,
              const DestructCallback& shell_connection_closed_callback,
              const DestructCallback& destruct_callback)
      : connection_(new shell::ShellConnection(this, std::move(request))),
        shell_connection_closed_callback_(shell_connection_closed_callback),
        destruct_callback_(destruct_callback) {
    bindings_.set_connection_error_handler(base::Bind(&PackagedApp::BindingLost,
                                                      base::Unretained(this)));
  }
  ~PackagedApp() override {
    destruct_callback_.Run(this);
  }

 private:
  // shell::ShellClient:
  bool AcceptConnection(shell::Connection* connection) override {
    connection->AddInterface<LifecycleControl>(this);
    return true;
  }

  // shell::InterfaceFactory<LifecycleControl>
  void Create(shell::Connection* connection,
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

  std::unique_ptr<shell::ShellConnection> connection_;
  mojo::BindingSet<LifecycleControl> bindings_;
  // Run when this object's connection to the shell is closed.
  DestructCallback shell_connection_closed_callback_;
  // Run when this object is destructed.
  DestructCallback destruct_callback_;

  DISALLOW_COPY_AND_ASSIGN(PackagedApp);
};

class Package
    : public shell::ShellClient,
      public shell::InterfaceFactory<shell::mojom::ShellClientFactory>,
      public shell::mojom::ShellClientFactory {
 public:
  Package() {}
  ~Package() override {}

  void set_runner(shell::ApplicationRunner* runner) {
    app_client_.set_runner(runner);
  }

 private:
  // shell::test::AppClient:
  bool AcceptConnection(shell::Connection* connection) override {
    connection->AddInterface<shell::mojom::ShellClientFactory>(this);
    return app_client_.AcceptConnection(connection);
  }

  // shell::InterfaceFactory<shell::mojom::ShellClientFactory>:
  void Create(shell::Connection* connection,
              shell::mojom::ShellClientFactoryRequest request) override {
    bindings_.AddBinding(this, std::move(request));
  }

  // shell::mojom::ShellClientFactory:
  void CreateShellClient(shell::mojom::ShellClientRequest request,
                         const mojo::String& name) override {
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
  mojo::BindingSet<shell::mojom::ShellClientFactory> bindings_;
  std::vector<PackagedApp*> apps_;

  DISALLOW_COPY_AND_ASSIGN(Package);
};

}  // namespace

MojoResult MojoMain(MojoHandle shell_handle) {
  Package* package = new Package;
  shell::ApplicationRunner runner(package);
  package->set_runner(&runner);
  return runner.Run(shell_handle);
}
