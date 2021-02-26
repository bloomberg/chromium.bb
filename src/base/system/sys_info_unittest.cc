// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/environment.h"
#include "base/files/file_util.h"
#include "base/optional.h"
#include "base/process/process_metrics.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/system/sys_info.h"
#include "base/test/task_environment.h"
#include "base/threading/platform_thread.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "testing/gtest/include/gtest/gtest-death-test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

namespace base {

#if defined(OS_ANDROID)
// Some Android (Cast) test devices have a large portion of physical memory
// reserved. During investigation, around 115-150 MB were seen reserved, so we
// track this here with a factory of safety of 2.
static constexpr int kReservedPhysicalMemory = 300 * 1024;  // In _K_bytes.
#endif  // defined(OS_ANDROID)

using SysInfoTest = PlatformTest;

TEST_F(SysInfoTest, NumProcs) {
  // We aren't actually testing that it's correct, just that it's sane.
  EXPECT_GE(SysInfo::NumberOfProcessors(), 1);
}

TEST_F(SysInfoTest, AmountOfMem) {
  // We aren't actually testing that it's correct, just that it's sane.
  EXPECT_GT(SysInfo::AmountOfPhysicalMemory(), 0);
  EXPECT_GT(SysInfo::AmountOfPhysicalMemoryMB(), 0);
  // The maxmimal amount of virtual memory can be zero which means unlimited.
  EXPECT_GE(SysInfo::AmountOfVirtualMemory(), 0);
}

#if defined(OS_LINUX) || defined(OS_CHROMEOS) || defined(OS_ANDROID)
#if defined(OS_LINUX) || defined(OS_CHROMEOS)
#define MAYBE_AmountOfAvailablePhysicalMemory \
  DISABLED_AmountOfAvailablePhysicalMemory
#else
#define MAYBE_AmountOfAvailablePhysicalMemory AmountOfAvailablePhysicalMemory
#endif  // defined(OS_LINUX) || defined(OS_CHROMEOS)
TEST_F(SysInfoTest, MAYBE_AmountOfAvailablePhysicalMemory) {
  // Note: info is in _K_bytes.
  SystemMemoryInfoKB info;
  ASSERT_TRUE(GetSystemMemoryInfo(&info));
  EXPECT_GT(info.free, 0);
  if (info.available != 0) {
    // If there is MemAvailable from kernel.
    EXPECT_LT(info.available, info.total);
    const int64_t amount = SysInfo::AmountOfAvailablePhysicalMemory(info);
    // We aren't actually testing that it's correct, just that it's sane.
    // Available memory is |free - reserved + reclaimable (inactive, non-free)|.
    // On some android platforms, reserved is a substantial portion.
    const int available =
#if defined(OS_ANDROID)
        info.free - kReservedPhysicalMemory;
#else
        info.free;
#endif  // defined(OS_ANDROID)
    EXPECT_GT(amount, static_cast<int64_t>(available) * 1024);
    EXPECT_LT(amount / 1024, info.available);
    // Simulate as if there is no MemAvailable.
    info.available = 0;
  }

  // There is no MemAvailable. Check the fallback logic.
  const int64_t amount = SysInfo::AmountOfAvailablePhysicalMemory(info);
  // We aren't actually testing that it's correct, just that it's sane.
  EXPECT_GT(amount, static_cast<int64_t>(info.free) * 1024);
  EXPECT_LT(amount / 1024, info.total);
}
#endif  // defined(OS_LINUX) || defined(OS_CHROMEOS) || defined(OS_ANDROID)

TEST_F(SysInfoTest, AmountOfFreeDiskSpace) {
  // We aren't actually testing that it's correct, just that it's sane.
  FilePath tmp_path;
  ASSERT_TRUE(GetTempDir(&tmp_path));
#if defined(OS_FUCHSIA)
  // Fuchsia currently requires "total disk space" be set explicitly.
  // See crbug.com/1148334.
  SysInfo::SetAmountOfTotalDiskSpace(tmp_path, 1024);
#endif
  EXPECT_GE(SysInfo::AmountOfFreeDiskSpace(tmp_path), 0) << tmp_path.value();
}

TEST_F(SysInfoTest, AmountOfTotalDiskSpace) {
  // We aren't actually testing that it's correct, just that it's sane.
  FilePath tmp_path;
  ASSERT_TRUE(GetTempDir(&tmp_path));
#if defined(OS_FUCHSIA)
  // Fuchsia currently requires "total disk space" be set explicitly.
  // See crbug.com/1148334.
  SysInfo::SetAmountOfTotalDiskSpace(tmp_path, 1024);
#endif
  EXPECT_GT(SysInfo::AmountOfTotalDiskSpace(tmp_path), 0) << tmp_path.value();
}

#if defined(OS_FUCHSIA)
// Verify that specifying total disk space for nested directories matches
// the deepest-nested.
TEST_F(SysInfoTest, NestedVolumesAmountOfTotalDiskSpace) {
  constexpr int64_t kOuterVolumeQuota = 1024;
  constexpr int64_t kInnerVolumeQuota = kOuterVolumeQuota / 2;

  FilePath tmp_path;
  ASSERT_TRUE(GetTempDir(&tmp_path));
  SysInfo::SetAmountOfTotalDiskSpace(tmp_path, kOuterVolumeQuota);
  const FilePath subdirectory_path = tmp_path.Append("subdirectory");
  SysInfo::SetAmountOfTotalDiskSpace(subdirectory_path, kInnerVolumeQuota);

  EXPECT_EQ(SysInfo::AmountOfTotalDiskSpace(tmp_path), kOuterVolumeQuota);
  EXPECT_EQ(SysInfo::AmountOfTotalDiskSpace(subdirectory_path),
            kInnerVolumeQuota);

  // Remove the inner directory quota setting and check again.
  SysInfo::SetAmountOfTotalDiskSpace(subdirectory_path, -1);
  EXPECT_EQ(SysInfo::AmountOfTotalDiskSpace(subdirectory_path),
            kOuterVolumeQuota);
}
#endif  // defined(OS_FUCHSIA)

#if defined(OS_WIN) || defined(OS_APPLE) || defined(OS_LINUX) || \
    defined(OS_CHROMEOS) || defined(OS_FUCHSIA)
TEST_F(SysInfoTest, OperatingSystemVersionNumbers) {
  int32_t os_major_version = -1;
  int32_t os_minor_version = -1;
  int32_t os_bugfix_version = -1;
  SysInfo::OperatingSystemVersionNumbers(&os_major_version, &os_minor_version,
                                         &os_bugfix_version);
  EXPECT_GT(os_major_version, -1);
  EXPECT_GT(os_minor_version, -1);
  EXPECT_GT(os_bugfix_version, -1);
}
#endif

#if defined(OS_IOS)
TEST_F(SysInfoTest, GetIOSBuildNumber) {
  std::string build_number(SysInfo::GetIOSBuildNumber());
  EXPECT_GT(build_number.length(), 0U);
}
#endif  // defined(OS_IOS)

TEST_F(SysInfoTest, Uptime) {
  TimeDelta up_time_1 = SysInfo::Uptime();
  // UpTime() is implemented internally using TimeTicks::Now(), which documents
  // system resolution as being 1-15ms. Sleep a little longer than that.
  PlatformThread::Sleep(TimeDelta::FromMilliseconds(20));
  TimeDelta up_time_2 = SysInfo::Uptime();
  EXPECT_GT(up_time_1.InMicroseconds(), 0);
  EXPECT_GT(up_time_2.InMicroseconds(), up_time_1.InMicroseconds());
}

#if defined(OS_APPLE)
TEST_F(SysInfoTest, HardwareModelNameFormatMacAndiOS) {
  std::string hardware_model = SysInfo::HardwareModelName();
  ASSERT_FALSE(hardware_model.empty());
  // Check that the model is of the expected format "Foo,Bar" where "Bar" is
  // a number.
  std::vector<StringPiece> pieces =
      SplitStringPiece(hardware_model, ",", KEEP_WHITESPACE, SPLIT_WANT_ALL);
  ASSERT_EQ(2u, pieces.size()) << hardware_model;
  int value;
  EXPECT_TRUE(StringToInt(pieces[1], &value)) << hardware_model;
}
#endif

TEST_F(SysInfoTest, GetHardwareInfo) {
  test::TaskEnvironment task_environment;
  base::Optional<SysInfo::HardwareInfo> hardware_info;

  auto callback = base::BindOnce(
      [](base::Optional<SysInfo::HardwareInfo>* target_info,
         SysInfo::HardwareInfo info) { *target_info = std::move(info); },
      &hardware_info);
  SysInfo::GetHardwareInfo(std::move(callback));
  task_environment.RunUntilIdle();

  ASSERT_TRUE(hardware_info.has_value());
  EXPECT_TRUE(IsStringUTF8(hardware_info->manufacturer));
  EXPECT_TRUE(IsStringUTF8(hardware_info->model));
  bool empty_result_expected =
#if defined(OS_ANDROID) || defined(OS_APPLE) || defined(OS_WIN) || \
    defined(OS_LINUX) || defined(OS_CHROMEOS)
      false;
#else
      true;
#endif
  EXPECT_EQ(hardware_info->manufacturer.empty(), empty_result_expected);
  EXPECT_EQ(hardware_info->model.empty(), empty_result_expected);

#if defined(OS_WIN)
  EXPECT_FALSE(hardware_info->serial_number.empty());
#else
  // TODO(crbug.com/907518): Implement support on other platforms.
  EXPECT_EQ(hardware_info->serial_number, std::string());
#endif
}

#if defined(OS_CHROMEOS) || BUILDFLAG(IS_LACROS)

TEST_F(SysInfoTest, GoogleChromeOSVersionNumbers) {
  int32_t os_major_version = -1;
  int32_t os_minor_version = -1;
  int32_t os_bugfix_version = -1;
  const char kLsbRelease[] =
      "FOO=1234123.34.5\n"
      "CHROMEOS_RELEASE_VERSION=1.2.3.4\n";
  SysInfo::SetChromeOSVersionInfoForTest(kLsbRelease, Time());
  SysInfo::OperatingSystemVersionNumbers(&os_major_version, &os_minor_version,
                                         &os_bugfix_version);
  EXPECT_EQ(1, os_major_version);
  EXPECT_EQ(2, os_minor_version);
  EXPECT_EQ(3, os_bugfix_version);
}

TEST_F(SysInfoTest, GoogleChromeOSVersionNumbersFirst) {
  int32_t os_major_version = -1;
  int32_t os_minor_version = -1;
  int32_t os_bugfix_version = -1;
  const char kLsbRelease[] =
      "CHROMEOS_RELEASE_VERSION=1.2.3.4\n"
      "FOO=1234123.34.5\n";
  SysInfo::SetChromeOSVersionInfoForTest(kLsbRelease, Time());
  SysInfo::OperatingSystemVersionNumbers(&os_major_version, &os_minor_version,
                                         &os_bugfix_version);
  EXPECT_EQ(1, os_major_version);
  EXPECT_EQ(2, os_minor_version);
  EXPECT_EQ(3, os_bugfix_version);
}

TEST_F(SysInfoTest, GoogleChromeOSNoVersionNumbers) {
  int32_t os_major_version = -1;
  int32_t os_minor_version = -1;
  int32_t os_bugfix_version = -1;
  const char kLsbRelease[] = "FOO=1234123.34.5\n";
  SysInfo::SetChromeOSVersionInfoForTest(kLsbRelease, Time());
  SysInfo::OperatingSystemVersionNumbers(&os_major_version, &os_minor_version,
                                         &os_bugfix_version);
  EXPECT_EQ(0, os_major_version);
  EXPECT_EQ(0, os_minor_version);
  EXPECT_EQ(0, os_bugfix_version);
}

TEST_F(SysInfoTest, GoogleChromeOSLsbReleaseTime) {
  const char kLsbRelease[] = "CHROMEOS_RELEASE_VERSION=1.2.3.4";
  // Use a fake time that can be safely displayed as a string.
  const Time lsb_release_time(Time::FromDoubleT(12345.6));
  SysInfo::SetChromeOSVersionInfoForTest(kLsbRelease, lsb_release_time);
  Time parsed_lsb_release_time = SysInfo::GetLsbReleaseTime();
  EXPECT_DOUBLE_EQ(lsb_release_time.ToDoubleT(),
                   parsed_lsb_release_time.ToDoubleT());
}

TEST_F(SysInfoTest, IsRunningOnChromeOS) {
  SysInfo::SetChromeOSVersionInfoForTest("", Time());
  EXPECT_FALSE(SysInfo::IsRunningOnChromeOS());

  const char kLsbRelease1[] =
      "CHROMEOS_RELEASE_NAME=Non Chrome OS\n"
      "CHROMEOS_RELEASE_VERSION=1.2.3.4\n";
  SysInfo::SetChromeOSVersionInfoForTest(kLsbRelease1, Time());
  EXPECT_FALSE(SysInfo::IsRunningOnChromeOS());

  const char kLsbRelease2[] =
      "CHROMEOS_RELEASE_NAME=Chrome OS\n"
      "CHROMEOS_RELEASE_VERSION=1.2.3.4\n";
  SysInfo::SetChromeOSVersionInfoForTest(kLsbRelease2, Time());
  EXPECT_TRUE(SysInfo::IsRunningOnChromeOS());

  const char kLsbRelease3[] = "CHROMEOS_RELEASE_NAME=Chromium OS\n";
  SysInfo::SetChromeOSVersionInfoForTest(kLsbRelease3, Time());
  EXPECT_TRUE(SysInfo::IsRunningOnChromeOS());
}

TEST_F(SysInfoTest, CrashOnBaseImage) {
  const char kLsbRelease2[] =
      "CHROMEOS_RELEASE_NAME=Chrome OS\n"
      "CHROMEOS_RELEASE_VERSION=1.2.3.4\n"
      "CHROMEOS_RELEASE_TRACK=stable-channel\n";
  SysInfo::SetChromeOSVersionInfoForTest(kLsbRelease2, Time());
  EXPECT_TRUE(SysInfo::IsRunningOnChromeOS());
  EXPECT_DEATH_IF_SUPPORTED({ SysInfo::CrashIfChromeOSNonTestImage(); }, "");
}

TEST_F(SysInfoTest, NoCrashOnTestImage) {
  const char kLsbRelease2[] =
      "CHROMEOS_RELEASE_NAME=Chrome OS\n"
      "CHROMEOS_RELEASE_VERSION=1.2.3.4\n"
      "CHROMEOS_RELEASE_TRACK=testimage-channel\n";
  SysInfo::SetChromeOSVersionInfoForTest(kLsbRelease2, Time());
  EXPECT_TRUE(SysInfo::IsRunningOnChromeOS());
  // Should not crash.
  SysInfo::CrashIfChromeOSNonTestImage();
}

TEST_F(SysInfoTest, NoCrashOnLinuxBuild) {
  SysInfo::SetChromeOSVersionInfoForTest("", Time());
  EXPECT_FALSE(SysInfo::IsRunningOnChromeOS());
  // Should not crash.
  SysInfo::CrashIfChromeOSNonTestImage();
}

#endif  // OS_CHROMEOS || BUILDFLAG(IS_LACROS)

}  // namespace base
