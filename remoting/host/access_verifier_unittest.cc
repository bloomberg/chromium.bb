// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/ref_counted.h"
#include "base/task.h"
#include "remoting/host/access_verifier.h"
#include "remoting/host/in_memory_host_config.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {

namespace {
const char kTestJid[] = "host@domain.com";
}  // namespace

class AccessVerifierTest : public testing::Test {
 protected:
  class TestConfigUpdater :
      public base::RefCountedThreadSafe<TestConfigUpdater> {
   public:
    void DoUpdate(scoped_refptr<InMemoryHostConfig> target) {
      target->SetString(kXmppLoginConfigPath, kTestJid);
    }
  };

  virtual void SetUp() {
    config_ = new InMemoryHostConfig();
  }

  void InitConfig() {
    scoped_refptr<TestConfigUpdater> config_updater(new TestConfigUpdater());
    config_->Update(
        NewRunnableMethod(config_updater.get(), &TestConfigUpdater::DoUpdate,
                          config_));
  }

  scoped_refptr<InMemoryHostConfig> config_;
};

TEST_F(AccessVerifierTest, InvalidConfig) {
  AccessVerifier target;
  EXPECT_FALSE(target.Init(config_));
}

TEST_F(AccessVerifierTest, VerifyPermissions) {
  AccessVerifier target;
  InitConfig();
  ASSERT_TRUE(target.Init(config_));
  EXPECT_TRUE(target.VerifyPermissions("host@domain.com/123123"));
  EXPECT_FALSE(target.VerifyPermissions("host@domain.com"));
  EXPECT_FALSE(target.VerifyPermissions("otherhost@domain.com/123123"));
  EXPECT_FALSE(target.VerifyPermissions("host@otherdomain.com/123123"));
  EXPECT_FALSE(target.VerifyPermissions(""));
  EXPECT_FALSE(target.VerifyPermissions("host@domain.co/saf"));
  EXPECT_FALSE(target.VerifyPermissions("host@domain.com.other/blah"));
}

}  // namespace remoting
