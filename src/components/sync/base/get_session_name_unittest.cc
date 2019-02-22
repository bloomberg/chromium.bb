// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/base/get_session_name.h"

#include <utility>

#include "base/sys_info.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_CHROMEOS)
#include "base/command_line.h"
#include "chromeos/chromeos_switches.h"
#endif  // OS_CHROMEOS

namespace syncer {
namespace {

// Call GetSessionNameBlocking and make sure its return
// value looks sane.
TEST(GetSessionNameTest, GetSessionNameBlocking) {
  const std::string& session_name = GetSessionNameBlocking();
  EXPECT_FALSE(session_name.empty());
}

#if defined(OS_CHROMEOS)

// Call GetSessionNameBlocking on ChromeOS where the board type
// is CHROMEBOOK and make sure the return value is "Chromebook".
TEST(GetSessionNameTest, GetSessionNameBlockingChromebook) {
  const char* kLsbRelease = "DEVICETYPE=CHROMEBOOK\n";
  base::SysInfo::SetChromeOSVersionInfoForTest(kLsbRelease, base::Time());
  const std::string& session_name = GetSessionNameBlocking();
  EXPECT_EQ("Chromebook", session_name);
}

// Call GetSessionNameBlocking on ChromeOS where the board type
// is a CHROMEBOX and make sure the return value is "Chromebox".
TEST(GetSessionNameTest, GetSessionNameBlockingChromebox) {
  const char* kLsbRelease = "DEVICETYPE=CHROMEBOX\n";
  base::SysInfo::SetChromeOSVersionInfoForTest(kLsbRelease, base::Time());
  const std::string& session_name = GetSessionNameBlocking();
  EXPECT_EQ("Chromebox", session_name);
}

#endif  // OS_CHROMEOS

}  // namespace
}  // namespace syncer
