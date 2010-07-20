// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "base/scoped_ptr.h"
#include "remoting/client/client_config.h"
#include "remoting/client/client_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {

// TODO(ajwong): Once ChromotingPlugin stablizes a little more, come up with
// sane unittests.
class ClientUtilTest : public testing::Test {
 protected:
  virtual void SetUp() {
  }
};

TEST_F(ClientUtilTest, GetLoginInfoFromUrlParams) {
  const char url[] = "chromotocol://hostid?user=auser&auth=someauth&jid=ajid";
  std::string user_id;
  std::string auth_token;
  std::string host_jid;
  ClientConfig config;

  ASSERT_TRUE(
      GetLoginInfoFromUrlParams(url, &config));

  EXPECT_EQ("auser", config.username());
  EXPECT_EQ("someauth", config.auth_token());
  EXPECT_EQ("ajid", config.host_jid());
}

}  // namespace remoting
