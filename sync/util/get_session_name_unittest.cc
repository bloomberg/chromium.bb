// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "base/sys_info.h"
#include "sync/util/get_session_name.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_CHROMEOS)
#include "base/command_line.h"
#include "chromeos/chromeos_switches.h"
#endif  // OS_CHROMEOS

namespace syncer {

namespace {

class GetSessionNameTest : public ::testing::Test {
 public:
  void SetSessionNameAndQuit(const std::string& session_name) {
    session_name_ = session_name;
    message_loop_.Quit();
  }

 protected:
  base::MessageLoop message_loop_;
  std::string session_name_;
};

// Call GetSessionNameSynchronouslyForTesting and make sure its return
// value looks sane.
TEST_F(GetSessionNameTest, GetSessionNameSynchronously) {
  const std::string& session_name = GetSessionNameSynchronouslyForTesting();
  EXPECT_FALSE(session_name.empty());
}

#if defined(OS_CHROMEOS)

// Call GetSessionNameSynchronouslyForTesting on ChromeOS where the board type
// is "lumpy-signed-mp-v2keys" and make sure the return value is "Chromebook".
TEST_F(GetSessionNameTest, GetSessionNameSynchronouslyChromebook) {
  const char* kLsbRelease = "CHROMEOS_RELEASE_BOARD=lumpy-signed-mp-v2keys\n";
  base::SysInfo::SetChromeOSVersionInfoForTest(kLsbRelease, base::Time());
  const std::string& session_name = GetSessionNameSynchronouslyForTesting();
  EXPECT_EQ("Chromebook", session_name);
}

// Call GetSessionNameSynchronouslyForTesting on ChromeOS where the board type
// is "stumpy-signed-mp-v2keys" and make sure the return value is "Chromebox".
TEST_F(GetSessionNameTest, GetSessionNameSynchronouslyChromebox) {
  const char* kLsbRelease = "CHROMEOS_RELEASE_BOARD=stumpy-signed-mp-v2keys\n";
  base::SysInfo::SetChromeOSVersionInfoForTest(kLsbRelease, base::Time());
  const std::string& session_name = GetSessionNameSynchronouslyForTesting();
  EXPECT_EQ("Chromebox", session_name);
}

#endif  // OS_CHROMEOS

// Calls GetSessionName and runs the message loop until it comes back
// with a session name.  Makes sure the returned session name is equal
// to the return value of GetSessionNameSynchronouslyForTesting().
TEST_F(GetSessionNameTest, GetSessionName) {
  GetSessionName(message_loop_.message_loop_proxy(),
                 base::Bind(&GetSessionNameTest::SetSessionNameAndQuit,
                            base::Unretained(this)));
  message_loop_.Run();
  EXPECT_EQ(session_name_, GetSessionNameSynchronouslyForTesting());
}

}  // namespace

}  // namespace syncer
