// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cast_channel/cast_message_util.h"

#include "base/json/json_reader.h"
#include "base/values.h"
#include "components/cast_channel/proto/cast_channel.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cast_channel {

TEST(CastMessageUtilTest, IsCastInternalNamespace) {
  EXPECT_TRUE(IsCastInternalNamespace("urn:x-cast:com.google.cast.receiver"));
  EXPECT_FALSE(IsCastInternalNamespace("urn:x-cast:com.google.youtube"));
  EXPECT_FALSE(IsCastInternalNamespace("urn:x-cast:com.foo"));
  EXPECT_FALSE(IsCastInternalNamespace("foo"));
  EXPECT_FALSE(IsCastInternalNamespace(""));
}

TEST(CastMessageUtilTest, CastMessageType) {
  for (int i = 0; i < static_cast<int>(CastMessageType::kOther); ++i) {
    CastMessageType type = static_cast<CastMessageType>(i);
    EXPECT_EQ(type, CastMessageTypeFromString(CastMessageTypeToString(type)));
  }
}

TEST(CastMessageUtilTest, GetLaunchSessionResponseOk) {
  std::string payload = R"(
    {
      "type": "RECEIVER_STATUS",
      "requestId": 123,
      "status": {}
    }
  )";

  std::unique_ptr<base::Value> value = base::JSONReader::Read(payload);
  ASSERT_TRUE(value);

  LaunchSessionResponse response = GetLaunchSessionResponse(*value);
  EXPECT_EQ(LaunchSessionResponse::Result::kOk, response.result);
  EXPECT_TRUE(response.receiver_status);
}

TEST(CastMessageUtilTest, GetLaunchSessionResponseError) {
  std::string payload = R"(
    {
      "type": "LAUNCH_ERROR",
      "requestId": 123
    }
  )";

  std::unique_ptr<base::Value> value = base::JSONReader::Read(payload);
  ASSERT_TRUE(value);

  LaunchSessionResponse response = GetLaunchSessionResponse(*value);
  EXPECT_EQ(LaunchSessionResponse::Result::kError, response.result);
  EXPECT_FALSE(response.receiver_status);
}

TEST(CastMessageUtilTest, GetLaunchSessionResponseUnknown) {
  // Unrelated type.
  std::string payload = R"(
    {
      "type": "APPLICATION_BROADCAST",
      "requestId": 123,
      "status": {}
    }
  )";

  std::unique_ptr<base::Value> value = base::JSONReader::Read(payload);
  ASSERT_TRUE(value);

  LaunchSessionResponse response = GetLaunchSessionResponse(*value);
  EXPECT_EQ(LaunchSessionResponse::Result::kUnknown, response.result);
  EXPECT_FALSE(response.receiver_status);
}

TEST(CastMessageUtilTest, CreateStopRequest) {
  std::string expected_message = R"(
    {
      "type": "STOP",
      "requestId": 123,
      "sessionId": "sessionId"
    }
  )";

  std::unique_ptr<base::Value> expected_value =
      base::JSONReader::Read(expected_message);
  ASSERT_TRUE(expected_value);

  CastMessage message = CreateStopRequest("sourceId", 123, "sessionId");
  ASSERT_TRUE(IsCastMessageValid(message));

  std::unique_ptr<base::Value> actual_value =
      base::JSONReader::Read(message.payload_utf8());
  ASSERT_TRUE(actual_value);
  EXPECT_EQ(*expected_value, *actual_value);
}

}  // namespace cast_channel
