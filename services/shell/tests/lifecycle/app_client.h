// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_SHELL_TESTS_LIFECYCLE_APP_CLIENT_H_
#define SERVICES_SHELL_TESTS_LIFECYCLE_APP_CLIENT_H_

#include <memory>

#include "base/bind.h"
#include "base/macros.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "services/shell/public/cpp/interface_factory.h"
#include "services/shell/public/cpp/service.h"
#include "services/shell/public/cpp/service_runner.h"
#include "services/shell/public/interfaces/service.mojom.h"
#include "services/shell/tests/lifecycle/lifecycle_unittest.mojom.h"

using LifecycleControl = shell::test::mojom::LifecycleControl;
using LifecycleControlRequest = shell::test::mojom::LifecycleControlRequest;

namespace shell {
class ServiceContext;

namespace test {

class AppClient : public Service,
                  public InterfaceFactory<LifecycleControl>,
                  public LifecycleControl {
 public:
  AppClient();
  explicit AppClient(shell::mojom::ServiceRequest request);
  ~AppClient() override;

  void set_runner(ServiceRunner* runner) {
    runner_ = runner;
  }

  // Service:
  bool OnConnect(const Identity& remote_identity,
                 InterfaceRegistry* registry) override;

  // InterfaceFactory<LifecycleControl>:
  void Create(const Identity& remote_identity,
              LifecycleControlRequest request) override;

  // LifecycleControl:
  void Ping(const PingCallback& callback) override;
  void GracefulQuit() override;
  void Crash() override;
  void CloseShellConnection() override;

 private:
  void BindingLost();

  ServiceRunner* runner_ = nullptr;
  mojo::BindingSet<LifecycleControl> bindings_;
  std::unique_ptr<ServiceContext> context_;

  DISALLOW_COPY_AND_ASSIGN(AppClient);
};

}  // namespace test
}  // namespace shell

#endif  // SERVICES_SHELL_TESTS_LIFECYCLE_APP_CLIENT_H_
