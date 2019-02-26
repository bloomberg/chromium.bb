// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/run_loop.h"
#include "base/test/scoped_task_environment.h"
#include "services/service_manager/public/cpp/connector.h"
#include "services/service_manager/public/cpp/service.h"
#include "services/service_manager/public/cpp/service_binding.h"
#include "services/service_manager/public/cpp/test/test_service_manager.h"
#include "services/service_manager/tests/catalog_source.h"
#include "services/service_manager/tests/shutdown/shutdown_unittest.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace service_manager {
namespace {

class ShutdownTest : public testing::Test {
 public:
  ShutdownTest()
      : test_service_manager_(test::CreateTestCatalog()),
        test_service_binding_(
            &test_service_,
            test_service_manager_.RegisterTestInstance("shutdown_unittest")) {}
  ~ShutdownTest() override = default;

  Connector* connector() { return test_service_binding_.GetConnector(); }

 private:
  base::test::ScopedTaskEnvironment task_environment_;
  TestServiceManager test_service_manager_;
  Service test_service_;
  ServiceBinding test_service_binding_;

  DISALLOW_COPY_AND_ASSIGN(ShutdownTest);
};

TEST_F(ShutdownTest, ConnectRace) {
  // This test exercises a number of potential shutdown races that can lead to
  // client deadlock if any of various parts of the EDK or service manager are
  // not
  // working as intended.

  mojom::ShutdownTestClientControllerPtr control;
  connector()->BindInterface("shutdown_client", &control);

  // Connect to shutdown_service and immediately request that it shut down.
  mojom::ShutdownTestServicePtr service;
  connector()->BindInterface("shutdown_service", &service);
  service->ShutDown();

  // Tell shutdown_client to connect to an interface on shutdown_service and
  // then block waiting for the interface pipe to signal something. If anything
  // goes wrong, its pipe won't signal and the client process will hang without
  // responding to this request.
  base::RunLoop loop;
  control->ConnectAndWait(loop.QuitClosure());
  loop.Run();
}

}  // namespace
}  // namespace service_manager
