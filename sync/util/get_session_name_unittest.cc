// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/bind.h"
#include "base/message_loop.h"
#include "sync/util/get_session_name.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace csync {

namespace {

class GetSessionNameTest : public ::testing::Test {
 public:
  void SetSessionNameAndQuit(const std::string& session_name) {
    session_name_ = session_name;
    message_loop_.Quit();
  }

 protected:
  MessageLoop message_loop_;
  std::string session_name_;
};

// Call GetSessionNameSynchronouslyForTesting and make sure its return
// value looks sane.
TEST_F(GetSessionNameTest, GetSessionNameSynchronously) {
  const std::string& session_name = GetSessionNameSynchronouslyForTesting();
  EXPECT_FALSE(session_name.empty());
}

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

}  // namespace csync
