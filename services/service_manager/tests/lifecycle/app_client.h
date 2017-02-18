// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_SERVICE_MANAGER_TESTS_LIFECYCLE_APP_CLIENT_H_
#define SERVICES_SERVICE_MANAGER_TESTS_LIFECYCLE_APP_CLIENT_H_

#include <memory>

#include "base/bind.h"
#include "base/macros.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "services/service_manager/public/cpp/interface_factory.h"
#include "services/service_manager/public/cpp/service.h"
#include "services/service_manager/public/cpp/service_runner.h"
#include "services/service_manager/public/interfaces/service.mojom.h"
#include "services/service_manager/tests/lifecycle/lifecycle_unittest.mojom.h"

namespace service_manager {
namespace test {

class AppClient : public Service,
                  public InterfaceFactory<mojom::LifecycleControl>,
                  public mojom::LifecycleControl {
 public:
  AppClient();
  ~AppClient() override;

  void set_runner(ServiceRunner* runner) { runner_ = runner; }

  // Service:
  bool OnConnect(const ServiceInfo& remote_info,
                 InterfaceRegistry* registry) override;
  bool OnServiceManagerConnectionLost() override;

  // InterfaceFactory<LifecycleControl>:
  void Create(const Identity& remote_identity,
              mojom::LifecycleControlRequest request) override;

  // LifecycleControl:
  void Ping(const PingCallback& callback) override;
  void GracefulQuit() override;
  void Crash() override;
  void CloseServiceManagerConnection() override;

 private:
  class ServiceImpl;

  void BindingLost();

  ServiceRunner* runner_ = nullptr;
  mojo::BindingSet<mojom::LifecycleControl> bindings_;

  DISALLOW_COPY_AND_ASSIGN(AppClient);
};

}  // namespace test
}  // namespace service_manager

#endif  // SERVICES_SERVICE_MANAGER_TESTS_LIFECYCLE_APP_CLIENT_H_
