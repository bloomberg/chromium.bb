// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "services/service_manager/public/cpp/connection.h"
#include "services/service_manager/public/cpp/connector.h"
#include "services/service_manager/public/cpp/service.h"
#include "services/service_manager/public/cpp/service_context.h"
#include "services/service_manager/runner/child/test_native_main.h"
#include "services/service_manager/runner/init.h"
#include "services/service_manager/tests/service_manager/service_manager_unittest.mojom.h"

using service_manager::test::mojom::CreateInstanceTestPtr;

namespace {

class Target : public service_manager::Service {
 public:
  Target() {}
  ~Target() override {}

 private:
  // service_manager::Service:
  void OnStart() override {
    CreateInstanceTestPtr service;
    context()->connector()->ConnectToInterface(
        "service_manager_unittest", &service);
    service->SetTargetIdentity(context()->identity());
  }

  bool OnConnect(const service_manager::ServiceInfo& remote_info,
                 service_manager::InterfaceRegistry* registry) override {
    return false;
  }

  DISALLOW_COPY_AND_ASSIGN(Target);
};

}  // namespace

int main(int argc, char** argv) {
  base::AtExitManager at_exit;
  base::CommandLine::Init(argc, argv);

  service_manager::InitializeLogging();
  return service_manager::TestNativeMain(base::MakeUnique<Target>());
}
