// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/command_line.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "services/shell/public/cpp/service_test.h"
#include "services/ui/public/interfaces/window_server_test.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ui {
namespace demo {

namespace {

const char kTestAppName[] = "exe:mus_demo_unittests";

void RunCallback(bool* success, const base::Closure& callback, bool result) {
  *success = result;
  callback.Run();
}

class MusDemoTest : public shell::test::ServiceTest {
 public:
  MusDemoTest() : shell::test::ServiceTest(kTestAppName) {}
  ~MusDemoTest() override {}

  void SetUp() override {
    base::CommandLine::ForCurrentProcess()->AppendSwitch("use-test-config");
    ServiceTest::SetUp();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(MusDemoTest);
};

}  // namespace

TEST_F(MusDemoTest, CheckMusDemoDraws) {
  connector()->Connect("service:mus_demo");

  ::ui::mojom::WindowServerTestPtr test_interface;
  connector()->ConnectToInterface("service:ui", &test_interface);

  base::RunLoop run_loop;
  bool success = false;
  // TODO(kylechar): Fix WindowServer::CreateTreeForWindowManager so that the
  // WindowTree has the correct name instead of an empty name.
  test_interface->EnsureClientHasDrawnWindow(
      "",  // WindowTree name is empty.
      base::Bind(&RunCallback, &success, run_loop.QuitClosure()));
  run_loop.Run();
  EXPECT_TRUE(success);
}

}  // namespace demo
}  // namespace ui
