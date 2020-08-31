// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_SERVICE_MANAGER_TESTS_LIFECYCLE_APP_CLIENT_H_
#define SERVICES_SERVICE_MANAGER_TESTS_LIFECYCLE_APP_CLIENT_H_

#include <memory>

#include "base/bind.h"
#include "base/callback.h"
#include "base/macros.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "services/service_manager/public/cpp/binder_registry.h"
#include "services/service_manager/public/cpp/service.h"
#include "services/service_manager/public/cpp/service_binding.h"
#include "services/service_manager/public/mojom/service.mojom.h"
#include "services/service_manager/tests/lifecycle/lifecycle.test-mojom.h"

namespace service_manager {
namespace test {

class AppClient : public Service,
                  public mojom::LifecycleControl {
 public:
  explicit AppClient(service_manager::mojom::ServiceRequest request);
  ~AppClient() override;

  // Service:
  void OnBindInterface(const BindSourceInfo& source_info,
                       const std::string& interface_name,
                       mojo::ScopedMessagePipeHandle interface_pipe) override;
  void OnDisconnected() override;

  void Create(mojo::PendingReceiver<mojom::LifecycleControl> receiver);

  // LifecycleControl:
  void Ping(PingCallback callback) override;
  void GracefulQuit() override;
  void Crash() override;
  void CloseServiceManagerConnection() override;

 private:
  void LifecycleControlBindingLost();

  ServiceBinding service_binding_;
  BinderRegistry registry_;
  mojo::ReceiverSet<mojom::LifecycleControl> receivers_;

  DISALLOW_COPY_AND_ASSIGN(AppClient);
};

}  // namespace test
}  // namespace service_manager

#endif  // SERVICES_SERVICE_MANAGER_TESTS_LIFECYCLE_APP_CLIENT_H_
