// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "services/service_manager/public/c/main.h"
#include "services/service_manager/public/cpp/interface_factory.h"
#include "services/service_manager/public/cpp/interface_registry.h"
#include "services/service_manager/public/cpp/service.h"
#include "services/service_manager/public/cpp/service_context.h"
#include "services/service_manager/public/cpp/service_runner.h"
#include "services/service_manager/tests/connect/connect_test.mojom.h"

using service_manager::test::mojom::ConnectTestService;
using service_manager::test::mojom::ConnectTestServiceRequest;

namespace {

class Target : public service_manager::Service,
               public service_manager::InterfaceFactory<ConnectTestService>,
               public ConnectTestService {
 public:
  Target() {}
  ~Target() override {}

 private:
  // service_manager::Service:
  bool OnConnect(const service_manager::ServiceInfo& remote_info,
                 service_manager::InterfaceRegistry* registry) override {
    registry->AddInterface<ConnectTestService>(this);
    return true;
  }

  // service_manager::InterfaceFactory<ConnectTestService>:
  void Create(const service_manager::Identity& remote_identity,
              ConnectTestServiceRequest request) override {
    bindings_.AddBinding(this, std::move(request));
  }

  // ConnectTestService:
  void GetTitle(const GetTitleCallback& callback) override {
    callback.Run("connect_test_exe");
  }

  void GetInstance(const GetInstanceCallback& callback) override {
    callback.Run(context()->identity().instance());
  }

  mojo::BindingSet<ConnectTestService> bindings_;

  DISALLOW_COPY_AND_ASSIGN(Target);
};

}  // namespac

MojoResult ServiceMain(MojoHandle service_request_handle) {
  service_manager::ServiceRunner runner(new Target);
  return runner.Run(service_request_handle);
}
