// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ui/public/cpp/tests/window_server_shelltest_base.h"

#include <memory>

#include "base/command_line.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "services/shell/public/cpp/service.h"
#include "services/shell/public/cpp/service_test.h"
#include "services/ui/common/switches.h"
#include "ui/gl/gl_switches.h"

namespace ui {

namespace {

const char kTestAppName[] = "mojo:mus_ws_unittests_app";

class WindowServerServiceTestClient : public shell::test::ServiceTestClient {
 public:
  explicit WindowServerServiceTestClient(WindowServerServiceTestBase* test)
      : ServiceTestClient(test), test_(test) {}
  ~WindowServerServiceTestClient() override {}

 private:
  // shell::test::ServiceTestClient:
  bool OnConnect(shell::Connection* connection) override {
    return test_->OnConnect(connection);
  }

  WindowServerServiceTestBase* test_;

  DISALLOW_COPY_AND_ASSIGN(WindowServerServiceTestClient);
};

void EnsureCommandLineSwitch(const std::string& name) {
  base::CommandLine* cmd_line = base::CommandLine::ForCurrentProcess();
  if (!cmd_line->HasSwitch(name))
    cmd_line->AppendSwitch(name);
}

}  // namespace

WindowServerServiceTestBase::WindowServerServiceTestBase()
    : ServiceTest(kTestAppName) {
  EnsureCommandLineSwitch(switches::kUseTestConfig);
  EnsureCommandLineSwitch(::switches::kOverrideUseGLWithOSMesaForTests);
}

WindowServerServiceTestBase::~WindowServerServiceTestBase() {}

std::unique_ptr<shell::Service>
WindowServerServiceTestBase::CreateService() {
  return base::WrapUnique(new WindowServerServiceTestClient(this));
}

}  // namespace ui
