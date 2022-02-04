// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/telemetry/api/diagnostics_api_converters.h"
#include "ash/webui/telemetry_extension_ui/mojom/diagnostics_service.mojom.h"
#include "chrome/common/chromeos/extensions/api/diagnostics.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace converters {

namespace {

using MojoRoutineCommandType = ash::health::mojom::DiagnosticRoutineCommandEnum;
using MojoRoutineStatus = ::ash::health::mojom::DiagnosticRoutineStatusEnum;
using MojoRoutineType = ::ash::health::mojom::DiagnosticRoutineEnum;
using MojoRoutineUserMessageType =
    ash::health::mojom::DiagnosticRoutineUserMessageEnum;

using RoutineCommandType = ::chromeos::api::os_diagnostics::RoutineCommandType;
using RoutineStatus = ::chromeos::api::os_diagnostics::RoutineStatus;
using RoutineType = ::chromeos::api::os_diagnostics::RoutineType;
using RoutineUserMessageType = ::chromeos::api::os_diagnostics::UserMessageType;

}  // namespace

// Tests that ConvertMojoRoutineTest() correctly converts the supported Mojo
// routine type values to the API's routine type values. For the unsupported
// type values, the call should fail (ConvertMojoRoutineTest() returns false);
TEST(TelemetryExtensionDiagnosticsApiConvertersUnitTest,
     ConvertMojoRoutineTest) {
  // Tests for supported routines.
  {
    RoutineType out;
    EXPECT_TRUE(ConvertMojoRoutine(MojoRoutineType::kBatteryCapacity, &out));
    EXPECT_EQ(out, RoutineType::ROUTINE_TYPE_BATTERY_CAPACITY);
  }
  {
    RoutineType out;
    EXPECT_TRUE(ConvertMojoRoutine(MojoRoutineType::kBatteryCharge, &out));
    EXPECT_EQ(out, RoutineType::ROUTINE_TYPE_BATTERY_CHARGE);
  }
  {
    RoutineType out;
    EXPECT_TRUE(ConvertMojoRoutine(MojoRoutineType::kBatteryDischarge, &out));
    EXPECT_EQ(out, RoutineType::ROUTINE_TYPE_BATTERY_DISCHARGE);
  }
  {
    RoutineType out;
    EXPECT_TRUE(ConvertMojoRoutine(MojoRoutineType::kBatteryHealth, &out));
    EXPECT_EQ(out, RoutineType::ROUTINE_TYPE_BATTERY_HEALTH);
  }
  {
    RoutineType out;
    EXPECT_TRUE(ConvertMojoRoutine(MojoRoutineType::kCpuCache, &out));
    EXPECT_EQ(out, RoutineType::ROUTINE_TYPE_CPU_CACHE);
  }
  {
    RoutineType out;
    EXPECT_TRUE(
        ConvertMojoRoutine(MojoRoutineType::kFloatingPointAccuracy, &out));
    EXPECT_EQ(out, RoutineType::ROUTINE_TYPE_CPU_FLOATING_POINT_ACCURACY);
  }
  {
    RoutineType out;
    EXPECT_TRUE(ConvertMojoRoutine(MojoRoutineType::kPrimeSearch, &out));
    EXPECT_EQ(out, RoutineType::ROUTINE_TYPE_CPU_PRIME_SEARCH);
  }
  {
    RoutineType out;
    EXPECT_TRUE(ConvertMojoRoutine(MojoRoutineType::kCpuStress, &out));
    EXPECT_EQ(out, RoutineType::ROUTINE_TYPE_CPU_STRESS);
  }
  {
    RoutineType out;
    EXPECT_TRUE(ConvertMojoRoutine(MojoRoutineType::kMemory, &out));
    EXPECT_EQ(out, RoutineType::ROUTINE_TYPE_MEMORY);
  }

  // Tests for unsupported routines.
  // Note: If an unsupported routine becomes supported, the respective test
  // should be changed.
  {
    RoutineType out = RoutineType::ROUTINE_TYPE_NONE;
    EXPECT_FALSE(ConvertMojoRoutine(MojoRoutineType::kSmartctlCheck, &out));
    EXPECT_EQ(out, RoutineType::ROUTINE_TYPE_NONE);
  }
  {
    RoutineType out = RoutineType::ROUTINE_TYPE_NONE;
    EXPECT_FALSE(ConvertMojoRoutine(MojoRoutineType::kAcPower, &out));
    EXPECT_EQ(out, RoutineType::ROUTINE_TYPE_NONE);
  }
  {
    RoutineType out = RoutineType::ROUTINE_TYPE_NONE;
    EXPECT_FALSE(ConvertMojoRoutine(MojoRoutineType::kNvmeWearLevel, &out));
    EXPECT_EQ(out, RoutineType::ROUTINE_TYPE_NONE);
  }
  {
    RoutineType out = RoutineType::ROUTINE_TYPE_NONE;
    EXPECT_FALSE(ConvertMojoRoutine(MojoRoutineType::kNvmeSelfTest, &out));
    EXPECT_EQ(out, RoutineType::ROUTINE_TYPE_NONE);
  }
  {
    RoutineType out = RoutineType::ROUTINE_TYPE_NONE;
    EXPECT_FALSE(ConvertMojoRoutine(MojoRoutineType::kDiskRead, &out));
    EXPECT_EQ(out, RoutineType::ROUTINE_TYPE_NONE);
  }
}

TEST(TelemetryExtensionDiagnosticsApiConvertersUnitTest, ConvertRoutineStatus) {
  EXPECT_EQ(ConvertRoutineStatus(MojoRoutineStatus::kReady),
            RoutineStatus::ROUTINE_STATUS_READY);
  EXPECT_EQ(ConvertRoutineStatus(MojoRoutineStatus::kRunning),
            RoutineStatus::ROUTINE_STATUS_RUNNING);
  EXPECT_EQ(ConvertRoutineStatus(MojoRoutineStatus::kWaiting),
            RoutineStatus::ROUTINE_STATUS_WAITING_USER_ACTION);
  EXPECT_EQ(ConvertRoutineStatus(MojoRoutineStatus::kPassed),
            RoutineStatus::ROUTINE_STATUS_PASSED);
  EXPECT_EQ(ConvertRoutineStatus(MojoRoutineStatus::kFailed),
            RoutineStatus::ROUTINE_STATUS_FAILED);
  EXPECT_EQ(ConvertRoutineStatus(MojoRoutineStatus::kError),
            RoutineStatus::ROUTINE_STATUS_ERROR);
  EXPECT_EQ(ConvertRoutineStatus(MojoRoutineStatus::kCancelled),
            RoutineStatus::ROUTINE_STATUS_CANCELLED);
  EXPECT_EQ(ConvertRoutineStatus(MojoRoutineStatus::kFailedToStart),
            RoutineStatus::ROUTINE_STATUS_FAILED_TO_START);
  EXPECT_EQ(ConvertRoutineStatus(MojoRoutineStatus::kRemoved),
            RoutineStatus::ROUTINE_STATUS_REMOVED);
  EXPECT_EQ(ConvertRoutineStatus(MojoRoutineStatus::kCancelling),
            RoutineStatus::ROUTINE_STATUS_CANCELLING);
  EXPECT_EQ(ConvertRoutineStatus(MojoRoutineStatus::kUnsupported),
            RoutineStatus::ROUTINE_STATUS_UNSUPPORTED);
  EXPECT_EQ(ConvertRoutineStatus(MojoRoutineStatus::kNotRun),
            RoutineStatus::ROUTINE_STATUS_NOT_RUN);
}

TEST(TelemetryExtensionDiagnosticsApiConvertersUnitTest,
     ConvertRoutineCommand) {
  EXPECT_EQ(
      ConvertRoutineCommand(RoutineCommandType::ROUTINE_COMMAND_TYPE_CANCEL),
      MojoRoutineCommandType::kCancel);
  EXPECT_EQ(
      ConvertRoutineCommand(RoutineCommandType::ROUTINE_COMMAND_TYPE_REMOVE),
      MojoRoutineCommandType::kRemove);
  EXPECT_EQ(
      ConvertRoutineCommand(RoutineCommandType::ROUTINE_COMMAND_TYPE_RESUME),
      MojoRoutineCommandType::kContinue);
  EXPECT_EQ(
      ConvertRoutineCommand(RoutineCommandType::ROUTINE_COMMAND_TYPE_STATUS),
      MojoRoutineCommandType::kGetStatus);
}

TEST(TelemetryExtensionDiagnosticsApiConvertersUnitTest,
     ConvertRoutineUserMessage) {
  EXPECT_EQ(
      ConvertRoutineUserMessage(MojoRoutineUserMessageType::kUnplugACPower),
      RoutineUserMessageType::USER_MESSAGE_TYPE_UNPLUG_AC_POWER);
  EXPECT_EQ(
      ConvertRoutineUserMessage(MojoRoutineUserMessageType::kPlugInACPower),
      RoutineUserMessageType::USER_MESSAGE_TYPE_PLUG_IN_AC_POWER);
}

}  // namespace converters
}  // namespace chromeos
