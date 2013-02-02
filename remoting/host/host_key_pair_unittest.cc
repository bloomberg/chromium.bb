// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/files/scoped_temp_dir.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop.h"
#include "base/message_loop_proxy.h"
#include "base/string_util.h"
#include "remoting/host/host_key_pair.h"
#include "remoting/host/json_host_config.h"
#include "remoting/host/test_key_pair.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {

namespace {
const char kTestMessage[] = "Test Message";

// |kTestMessage| signed with the key from |kTestHostKeyPair|.
const char kExpectedSignature[] =
"LfUyXU2AiKM4rpWivUR3bLiQiRt1W3iIenNfJEB8RWyoEfnvSBoD52x8q9yFvtLFDEMPWyIrwM+N2"
"LuaWBKG1c0R7h+twBgvpExzZneJl+lbGMRx9ba8m/KAFrUWA/NRzOen2NHCuPybOEasgrPgGWBrmf"
"gDcvyW8QiGuKLopGj/4c5CQT4yE8JjsyU3Qqo2ZPK4neJYQhOmAlg+Q5dAPLpzWMj5HQyOVHJaSXZ"
"Y8vl/LiKvbdofYLeYNVKAE4q5mfpQMrsysPYpbxBV60AhFyrvtC040MFGcflKQRZNiZwMXVb7DclC"
"BPgvK7rI5Y0ERtVm+yNmH7vCivfyAnDUYA==";
}  // namespace

class HostKeyPairTest : public testing::Test {
 protected:
  virtual void SetUp() {
    ASSERT_TRUE(test_dir_.CreateUniqueTempDir());
    base::FilePath config_path = test_dir_.path().AppendASCII("test_config.json");
    config_.reset(new JsonHostConfig(config_path));
  }

  MessageLoop message_loop_;
  base::ScopedTempDir test_dir_;
  scoped_ptr<JsonHostConfig> config_;
};

TEST_F(HostKeyPairTest, SaveLoad) {
  // Save a key to a config, then load it back, and verify that we
  // generate the same signature with both keys.
  HostKeyPair exported_key;
  exported_key.LoadFromString(kTestHostKeyPair);
  exported_key.Save(config_.get());

  message_loop_.RunUntilIdle();

  HostKeyPair imported_key;
  imported_key.Load(*config_);

  ASSERT_EQ(exported_key.GetSignature(kTestMessage),
            imported_key.GetSignature(kTestMessage));
}

TEST_F(HostKeyPairTest, Signatures) {
  // Sign a message and check that we get expected signature.
  HostKeyPair key_pair;
  key_pair.LoadFromString(kTestHostKeyPair);

  std::string signature_base64 = key_pair.GetSignature(kTestMessage);
  ASSERT_EQ(signature_base64, std::string(kExpectedSignature));
}

}  // namespace remoting
