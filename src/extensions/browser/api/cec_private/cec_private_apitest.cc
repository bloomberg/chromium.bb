// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/macros.h"
#include "chromeos/dbus/cec_service_client.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/fake_cec_service_client.h"
#include "extensions/common/features/feature_session_type.h"
#include "extensions/common/switches.h"
#include "extensions/shell/test/shell_apitest.h"
#include "extensions/test/extension_test_message_listener.h"
#include "extensions/test/result_catcher.h"

namespace extensions {

namespace {

constexpr char kTestAppId[] = "jabiebdnficieldhmegebckfhpfidfla";

class CecPrivateKioskApiTest : public ShellApiTest {
 public:
  CecPrivateKioskApiTest()
      : session_type_(
            ScopedCurrentFeatureSessionType(FeatureSessionType::KIOSK)) {}
  ~CecPrivateKioskApiTest() override = default;

  void SetUpOnMainThread() override {
    cec_ = static_cast<chromeos::FakeCecServiceClient*>(
        chromeos::DBusThreadManager::Get()->GetCecServiceClient());
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitchASCII(
        extensions::switches::kWhitelistedExtensionID, kTestAppId);
    ShellApiTest::SetUpCommandLine(command_line);
  }

 protected:
  chromeos::FakeCecServiceClient* cec_ = nullptr;

 private:
  std::unique_ptr<base::AutoReset<FeatureSessionType>> session_type_;
  DISALLOW_COPY_AND_ASSIGN(CecPrivateKioskApiTest);
};

using CecPrivateNonKioskApiTest = ShellApiTest;

}  // namespace

IN_PROC_BROWSER_TEST_F(CecPrivateKioskApiTest, TestAllApiFunctions) {
  cec_->set_tv_power_states({chromeos::CecServiceClient::PowerState::kOn,
                             chromeos::CecServiceClient::PowerState::kOn});
  extensions::ResultCatcher catcher;
  ExtensionTestMessageListener standby_call_count("standby_call_count", true);
  ExtensionTestMessageListener wakeup_call_count("wakeup_call_count", true);

  ASSERT_TRUE(LoadApp("api_test/cec_private/api"));

  ASSERT_TRUE(standby_call_count.WaitUntilSatisfied())
      << standby_call_count.message();
  standby_call_count.Reply(cec_->stand_by_call_count());

  ASSERT_TRUE(wakeup_call_count.WaitUntilSatisfied())
      << wakeup_call_count.message();
  wakeup_call_count.Reply(cec_->wake_up_call_count());

  ASSERT_TRUE(catcher.GetNextResult()) << catcher.message();
}

IN_PROC_BROWSER_TEST_F(CecPrivateNonKioskApiTest, TestCecPrivateNotAvailable) {
  ASSERT_TRUE(RunAppTest("api_test/cec_private/non_kiosk_api_not_available"))
      << message_;
}

}  // namespace extensions
