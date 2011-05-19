// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/file_util.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop.h"
#include "base/message_loop_proxy.h"
#include "base/path_service.h"
#include "base/scoped_temp_dir.h"
#include "remoting/host/json_host_config.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {

namespace {
const char* kTestConfig =
"{\n"
"  \"xmpp_login\" : \"test@gmail.com\",\n"
"  \"xmpp_auth_token\" : \"TEST_AUTH_TOKEN\",\n"
"  \"host_id\" : \"TEST_HOST_ID\",\n"
"  \"host_name\" : \"TEST_MACHINE_NAME\",\n"
"  \"private_key\" : \"TEST_PRIVATE_KEY\"\n"
"}\n";
}  // namespace

class JsonHostConfigTest : public testing::Test {
 protected:
  virtual void SetUp() {
    message_loop_proxy_ = base::MessageLoopProxy::CreateForCurrentThread();
  }

  static void WriteTestFile(const FilePath& filename) {
    file_util::WriteFile(filename, kTestConfig, std::strlen(kTestConfig));
  }

  // The temporary directory used to contain the test operations.
  ScopedTempDir test_dir_;
  // A message loop that we can use as the file thread message loop.
  MessageLoop message_loop_;
  scoped_refptr<base::MessageLoopProxy> message_loop_proxy_;
};

TEST_F(JsonHostConfigTest, InvalidFile) {
  ASSERT_TRUE(test_dir_.CreateUniqueTempDir());
  FilePath non_existent_file =
      test_dir_.path().AppendASCII("non_existent.json");
  scoped_refptr<JsonHostConfig> target(
      new JsonHostConfig(non_existent_file, message_loop_proxy_.get()));
  EXPECT_FALSE(target->Read());
}

TEST_F(JsonHostConfigTest, Read) {
  ASSERT_TRUE(test_dir_.CreateUniqueTempDir());
  FilePath test_file = test_dir_.path().AppendASCII("read.json");
  WriteTestFile(test_file);
  scoped_refptr<JsonHostConfig> target(
      new JsonHostConfig(test_file, message_loop_proxy_.get()));
  ASSERT_TRUE(target->Read());

  std::string value;
  EXPECT_TRUE(target->GetString(kXmppLoginConfigPath, &value));
  EXPECT_EQ("test@gmail.com", value);
  EXPECT_TRUE(target->GetString(kXmppAuthTokenConfigPath, &value));
  EXPECT_EQ("TEST_AUTH_TOKEN", value);
  EXPECT_TRUE(target->GetString(kHostIdConfigPath, &value));
  EXPECT_EQ("TEST_HOST_ID", value);
  EXPECT_TRUE(target->GetString(kHostNameConfigPath, &value));
  EXPECT_EQ("TEST_MACHINE_NAME", value);
  EXPECT_TRUE(target->GetString(kPrivateKeyConfigPath, &value));
  EXPECT_EQ("TEST_PRIVATE_KEY", value);

  EXPECT_FALSE(target->GetString("non_existent_value", &value));
}

TEST_F(JsonHostConfigTest, Write) {
  ASSERT_TRUE(test_dir_.CreateUniqueTempDir());

  FilePath test_file = test_dir_.path().AppendASCII("write.json");
  WriteTestFile(test_file);
  scoped_refptr<JsonHostConfig> target(
      new JsonHostConfig(test_file, message_loop_proxy_.get()));
  ASSERT_TRUE(target->Read());

  std::string new_auth_token_value = "NEW_AUTH_TOKEN";
  target->SetString(kXmppAuthTokenConfigPath, new_auth_token_value);
  target->Save();

  message_loop_.RunAllPending();

  // Now read the file again and check that the value has been written.
  scoped_refptr<JsonHostConfig> reader(
      new JsonHostConfig(test_file, message_loop_proxy_.get()));

  ASSERT_TRUE(reader->Read());

  std::string value;
  EXPECT_TRUE(reader->GetString(kXmppLoginConfigPath, &value));
  EXPECT_EQ("test@gmail.com", value);
  EXPECT_TRUE(reader->GetString(kXmppAuthTokenConfigPath, &value));
  EXPECT_EQ(new_auth_token_value, value);
  EXPECT_TRUE(reader->GetString(kHostIdConfigPath, &value));
  EXPECT_EQ("TEST_HOST_ID", value);
  EXPECT_TRUE(reader->GetString(kHostNameConfigPath, &value));
  EXPECT_EQ("TEST_MACHINE_NAME", value);
  EXPECT_TRUE(reader->GetString(kPrivateKeyConfigPath, &value));
  EXPECT_EQ("TEST_PRIVATE_KEY", value);
}

}
