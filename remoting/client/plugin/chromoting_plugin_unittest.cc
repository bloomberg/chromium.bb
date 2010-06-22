// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "base/scoped_ptr.h"
#include "remoting/client/plugin/chromoting_plugin.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {

// TODO(ajwong): Once ChromotingPlugin stablizes a little more, come up with
// sane unittests.
class ChromotingPluginTest : public testing::Test {
 protected:
  virtual void SetUp() {
  }
};

TEST_F(ChromotingPluginTest, ParseUrl) {
  const char url[] = "chromotocol://hostid?user=auser&auth=someauth&jid=ajid";
  std::string user_id;
  std::string auth_token;
  std::string host_jid;
  ASSERT_TRUE(
      ChromotingPlugin::ParseUrl(url, &user_id, &auth_token, &host_jid));

  EXPECT_EQ("auser", user_id);
  EXPECT_EQ("someauth", auth_token);
  EXPECT_EQ("ajid", host_jid);
}

}  // namespace remoting
