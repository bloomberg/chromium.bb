// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/run_loop.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "services/shell/public/cpp/shell_test.h"
#include "services/shell/tests/shutdown/shutdown_unittest.mojom.h"

namespace shell {
namespace {

class ShutdownTest : public test::ShellTest {
 public:
  ShutdownTest() : test::ShellTest("mojo:shutdown_unittest") {}
  ~ShutdownTest() override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(ShutdownTest);
};

TEST_F(ShutdownTest, ConnectRace) {
  // This test exercises a number of potential shutdown races that can lead to
  // client deadlock if any of various parts of the EDK or shell service are not
  // working as intended.

  mojom::ShutdownTestClientControllerPtr control;
  connector()->ConnectToInterface("mojo:shutdown_client", &control);

  // Connect to shutdown_service and immediately request that it shut down.
  mojom::ShutdownTestServicePtr service;
  connector()->ConnectToInterface("mojo:shutdown_service", &service);
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
}  // namespace shell
