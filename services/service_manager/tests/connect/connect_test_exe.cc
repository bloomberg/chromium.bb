// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/macros.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "services/service_manager/public/cpp/connection.h"
#include "services/service_manager/public/cpp/connector.h"
#include "services/service_manager/public/cpp/interface_factory.h"
#include "services/service_manager/public/cpp/interface_registry.h"
#include "services/service_manager/public/cpp/service.h"
#include "services/service_manager/runner/child/test_native_main.h"
#include "services/service_manager/runner/init.h"
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
  void OnStart(const service_manager::ServiceInfo& info) override {
    identity_ = info.identity;
  }
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
    callback.Run(identity_.instance());
  }

  service_manager::Identity identity_;
  mojo::BindingSet<ConnectTestService> bindings_;

  DISALLOW_COPY_AND_ASSIGN(Target);
};

}  // namespace

int main(int argc, char** argv) {
  base::AtExitManager at_exit;
  base::CommandLine::Init(argc, argv);

  service_manager::InitializeLogging();

  Target target;
  return service_manager::TestNativeMain(&target);
}
