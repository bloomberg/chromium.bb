// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/extension_telemetry/tabs_execute_script_signal_processor.h"
#include "chrome/browser/safe_browsing/extension_telemetry/tabs_execute_script_signal.h"
#include "components/safe_browsing/core/common/proto/csd.pb.h"
#include "crypto/sha2.h"
#include "extensions/common/extension_id.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace safe_browsing {

namespace {

using SignalInfo = ExtensionTelemetryReportRequest_SignalInfo;
using TabsExecuteScriptInfo =
    ExtensionTelemetryReportRequest_SignalInfo_TabsExecuteScriptInfo;
using ScriptInfo =
    ExtensionTelemetryReportRequest_SignalInfo_TabsExecuteScriptInfo_ScriptInfo;

constexpr const char* kExtensionId[] = {"aaaaaaaabbbbbbbbccccccccdddddddd",
                                        "eeeeeeeeffffffffgggggggghhhhhhhh"};

struct ScriptData {
  explicit ScriptData(const std::string& script_code)
      : code(script_code), hash(crypto::SHA256HashString(script_code)) {}
  std::string code;
  std::string hash;
};

class TabsExecuteScriptSignalProcessorTest : public ::testing::Test {
 protected:
  TabsExecuteScriptSignalProcessorTest()
      : script_data_{{ScriptData("document.write('Hello World')")},
                     {ScriptData("document.write('Goodbye World')")}} {}

  TabsExecuteScriptSignalProcessor processor_;
  const ScriptData script_data_[2];
};

TEST_F(TabsExecuteScriptSignalProcessorTest, NoDataPresentInitially) {
  EXPECT_FALSE(processor_.HasDataToReportForTest());
}

TEST_F(TabsExecuteScriptSignalProcessorTest, StoresDataAfterProcessingSignal) {
  // Process a signal.
  auto signal = std::make_unique<TabsExecuteScriptSignal>(kExtensionId[0],
                                                          script_data_[0].code);
  processor_.ProcessSignal(std::move(signal));

  // Verify that processor now has some data to report.
  EXPECT_TRUE(processor_.HasDataToReportForTest());

  // Verify that there is signal info only for the correct extension id.
  EXPECT_TRUE(processor_.GetSignalInfoForReport(kExtensionId[0]));
  EXPECT_FALSE(processor_.GetSignalInfoForReport(kExtensionId[1]));
}

TEST_F(TabsExecuteScriptSignalProcessorTest, ReportsSignalInfoCorrectly) {
  // Process 3 signals for the first extension, each corresponding to the
  // execution of the first test script.
  for (int i = 0; i < 3; i++) {
    auto signal = std::make_unique<TabsExecuteScriptSignal>(
        kExtensionId[0], script_data_[0].code);
    processor_.ProcessSignal(std::move(signal));
  }

  // Process 3 signals for second extension. Two signal corresponds to the
  // execution of first script, the third to the execution of the second script.
  for (int i = 0; i < 2; i++) {
    auto signal = std::make_unique<TabsExecuteScriptSignal>(
        kExtensionId[1], script_data_[0].code);
    processor_.ProcessSignal(std::move(signal));
  }
  {
    auto signal = std::make_unique<TabsExecuteScriptSignal>(
        kExtensionId[1], script_data_[1].code);
    processor_.ProcessSignal(std::move(signal));
  }

  // Retrieve signal info for first extension.
  std::unique_ptr<SignalInfo> extension_0_signal_info =
      processor_.GetSignalInfoForReport(kExtensionId[0]);
  ASSERT_NE(extension_0_signal_info, nullptr);

  // Verify that processor still has some data to report (for second extension).
  EXPECT_TRUE(processor_.HasDataToReportForTest());

  // Retrieve signal info for second extension.
  std::unique_ptr<SignalInfo> extension_1_signal_info =
      processor_.GetSignalInfoForReport(kExtensionId[1]);
  ASSERT_NE(extension_1_signal_info, nullptr);

  // Verify that processor no longer has data to report.
  EXPECT_FALSE(processor_.HasDataToReportForTest());

  // Verify signal info contents for first extension.
  {
    const TabsExecuteScriptInfo& tabs_execute_script_info =
        extension_0_signal_info->tabs_execute_script_info();

    // Verify data stored : only 1 script (3 executions).
    ASSERT_EQ(tabs_execute_script_info.scripts_size(), 1);
    const ScriptInfo& script_info = tabs_execute_script_info.scripts(0);
    EXPECT_EQ(script_info.hash(), script_data_[0].hash);
    EXPECT_EQ(script_info.execution_count(), static_cast<uint32_t>(3));
  }

  // Verify signal info contents for second extension.
  {
    const TabsExecuteScriptInfo& tabs_execute_script_info =
        extension_1_signal_info->tabs_execute_script_info();

    // Verify data stored : 2 scripts (2 executions for 1st, 1 for the 2nd).
    ASSERT_EQ(tabs_execute_script_info.scripts_size(), 2);
    {
      const ScriptInfo& script_info = tabs_execute_script_info.scripts(0);
      EXPECT_EQ(script_info.hash(), script_data_[0].hash);
      EXPECT_EQ(script_info.execution_count(), static_cast<uint32_t>(2));
    }
    {
      const ScriptInfo& script_info = tabs_execute_script_info.scripts(1);
      EXPECT_EQ(script_info.hash(), script_data_[1].hash);
      EXPECT_EQ(script_info.execution_count(), static_cast<uint32_t>(1));
    }
  }
}

}  // namespace

}  // namespace safe_browsing
