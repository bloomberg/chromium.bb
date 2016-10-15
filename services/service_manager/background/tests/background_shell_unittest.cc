// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/service_manager/background/background_shell.h"

#include <memory>

#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "services/service_manager/background/tests/test.mojom.h"
#include "services/service_manager/public/cpp/connector.h"
#include "services/service_manager/public/cpp/service.h"
#include "services/service_manager/public/cpp/service_context.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace shell {
namespace {

const char kTestName[] = "service:background_shell_unittest";
const char kAppName[] = "service:background_shell_test_service";

class ServiceImpl : public Service {
 public:
  ServiceImpl() {}
  ~ServiceImpl() override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(ServiceImpl);
};

void SetFlagAndRunClosure(bool* flag, const base::Closure& closure) {
  *flag = true;
  closure.Run();
}

}  // namespace

// Uses BackgroundShell to start the shell in the background and connects to
// background_shell_test_app, verifying we can send a message to the app.
#if defined(OS_ANDROID)
// TODO(crbug.com/589784): This test is disabled, as it fails
// on the Android GN bot.
#define MAYBE_Basic DISABLED_Basic
#else
#define MAYBE_Basic Basic
#endif
TEST(BackgroundShellTest, MAYBE_Basic) {
  base::MessageLoop message_loop;
  BackgroundShell background_shell;
  background_shell.Init(nullptr);
  ServiceImpl service;
  ServiceContext service_context(
      &service, background_shell.CreateServiceRequest(kTestName));
  mojom::TestServicePtr test_service;
  service_context.connector()->ConnectToInterface(kAppName, &test_service);
  base::RunLoop run_loop;
  bool got_result = false;
  test_service->Test(base::Bind(&SetFlagAndRunClosure, &got_result,
                                run_loop.QuitClosure()));
  run_loop.Run();
  EXPECT_TRUE(got_result);
}

}  // namespace shell
