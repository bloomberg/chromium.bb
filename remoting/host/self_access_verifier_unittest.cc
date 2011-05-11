// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/ref_counted.h"
#include "base/task.h"
#include "remoting/host/in_memory_host_config.h"
#include "remoting/host/self_access_verifier.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {

namespace {
const char kTestJid[] = "host@domain.com";
}  // namespace

class SelfAccessVerifierTest : public testing::Test {
 protected:
  virtual void SetUp() {
    config_ = new InMemoryHostConfig();
  }

  void InitConfig() {
    config_->SetString(kXmppLoginConfigPath, kTestJid);
  }

  scoped_refptr<InMemoryHostConfig> config_;
};

TEST_F(SelfAccessVerifierTest, InvalidConfig) {
  SelfAccessVerifier target;
  EXPECT_FALSE(target.Init(config_));
}

TEST_F(SelfAccessVerifierTest, VerifyPermissions) {
  SelfAccessVerifier target;
  InitConfig();
  ASSERT_TRUE(target.Init(config_));
  EXPECT_TRUE(target.VerifyPermissions("host@domain.com/123123", ""));
  EXPECT_TRUE(target.VerifyPermissions("hOsT@domain.com/123123", ""));
  EXPECT_FALSE(target.VerifyPermissions("host@domain.com", ""));
  EXPECT_FALSE(target.VerifyPermissions("otherhost@domain.com/123123", ""));
  EXPECT_FALSE(target.VerifyPermissions("host@otherdomain.com/123123", ""));
  EXPECT_FALSE(target.VerifyPermissions("", ""));
  EXPECT_FALSE(target.VerifyPermissions("host@domain.co/saf", ""));
  EXPECT_FALSE(target.VerifyPermissions("host@domain.com.other/blah", ""));

  // Non ASCII string.
  EXPECT_FALSE(target.VerifyPermissions("абв@domain.com/saf", ""));
}

}  // namespace remoting
