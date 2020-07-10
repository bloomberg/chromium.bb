// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cast/common/channel/cast_socket.h"

#include "cast/common/channel/message_framer.h"
#include "cast/common/channel/proto/cast_channel.pb.h"
#include "cast/common/channel/test/fake_cast_socket.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace cast {
namespace channel {
namespace {

using ::testing::_;
using ::testing::Invoke;

using openscreen::ErrorOr;

class CastSocketTest : public ::testing::Test {
 public:
  void SetUp() override {
    message_.set_protocol_version(CastMessage::CASTV2_1_0);
    message_.set_source_id("source");
    message_.set_destination_id("destination");
    message_.set_namespace_("namespace");
    message_.set_payload_type(CastMessage::STRING);
    message_.set_payload_utf8("payload");
    ErrorOr<std::vector<uint8_t>> serialized_or_error =
        message_serialization::Serialize(message_);
    ASSERT_TRUE(serialized_or_error);
    frame_serial_ = std::move(serialized_or_error.value());
  }

 protected:
  MockTlsConnection& connection() { return *fake_socket_.connection; }
  MockCastSocketClient& mock_client() { return fake_socket_.mock_client; }
  CastSocket& socket() { return fake_socket_.socket; }

  FakeCastSocket fake_socket_;
  CastMessage message_;
  std::vector<uint8_t> frame_serial_;
};

}  // namespace

TEST_F(CastSocketTest, SendMessage) {
  EXPECT_CALL(connection(), Write(_, _))
      .WillOnce(Invoke([this](const void* data, size_t len) {
        EXPECT_EQ(
            frame_serial_,
            std::vector<uint8_t>(reinterpret_cast<const uint8_t*>(data),
                                 reinterpret_cast<const uint8_t*>(data) + len));
      }));
  ASSERT_TRUE(socket().SendMessage(message_).ok());
}

TEST_F(CastSocketTest, ReadCompleteMessage) {
  const uint8_t* data = frame_serial_.data();
  EXPECT_CALL(mock_client(), OnMessage(_, _))
      .WillOnce(Invoke([this](CastSocket* socket, CastMessage message) {
        EXPECT_EQ(message_.SerializeAsString(), message.SerializeAsString());
      }));
  connection().OnRead(std::vector<uint8_t>(data, data + frame_serial_.size()));
}

TEST_F(CastSocketTest, ReadChunkedMessage) {
  const uint8_t* data = frame_serial_.data();
  EXPECT_CALL(mock_client(), OnMessage(_, _)).Times(0);
  connection().OnRead(std::vector<uint8_t>(data, data + 10));

  EXPECT_CALL(mock_client(), OnMessage(_, _))
      .WillOnce(Invoke([this](CastSocket* socket, CastMessage message) {
        EXPECT_EQ(message_.SerializeAsString(), message.SerializeAsString());
      }));
  connection().OnRead(
      std::vector<uint8_t>(data + 10, data + frame_serial_.size()));

  std::vector<uint8_t> double_message;
  double_message.insert(double_message.end(), frame_serial_.begin(),
                        frame_serial_.end());
  double_message.insert(double_message.end(), frame_serial_.begin(),
                        frame_serial_.end());
  data = double_message.data();
  EXPECT_CALL(mock_client(), OnMessage(_, _))
      .WillOnce(Invoke([this](CastSocket* socket, CastMessage message) {
        EXPECT_EQ(message_.SerializeAsString(), message.SerializeAsString());
      }));
  connection().OnRead(
      std::vector<uint8_t>(data, data + frame_serial_.size() + 10));

  EXPECT_CALL(mock_client(), OnMessage(_, _))
      .WillOnce(Invoke([this](CastSocket* socket, CastMessage message) {
        EXPECT_EQ(message_.SerializeAsString(), message.SerializeAsString());
      }));
  connection().OnRead(std::vector<uint8_t>(data + frame_serial_.size() + 10,
                                           data + double_message.size()));
}

TEST_F(CastSocketTest, SendMessageWhileBlocked) {
  connection().OnWriteBlocked();
  EXPECT_CALL(connection(), Write(_, _)).Times(0);
  ASSERT_TRUE(socket().SendMessage(message_).ok());

  EXPECT_CALL(connection(), Write(_, _))
      .WillOnce(Invoke([this](const void* data, size_t len) {
        EXPECT_EQ(
            frame_serial_,
            std::vector<uint8_t>(reinterpret_cast<const uint8_t*>(data),
                                 reinterpret_cast<const uint8_t*>(data) + len));
      }));
  connection().OnWriteUnblocked();

  EXPECT_CALL(connection(), Write(_, _)).Times(0);
  connection().OnWriteBlocked();
  connection().OnWriteUnblocked();
}

TEST_F(CastSocketTest, ErrorWhileEmptyingQueue) {
  connection().OnWriteBlocked();
  EXPECT_CALL(connection(), Write(_, _)).Times(0);
  ASSERT_TRUE(socket().SendMessage(message_).ok());

  EXPECT_CALL(connection(), Write(_, _))
      .WillOnce(Invoke([this](const void* data, size_t len) {
        EXPECT_EQ(
            frame_serial_,
            std::vector<uint8_t>(reinterpret_cast<const uint8_t*>(data),
                                 reinterpret_cast<const uint8_t*>(data) + len));
        connection().OnError(Error::Code::kUnknownError);
      }));
  connection().OnWriteUnblocked();

  EXPECT_CALL(connection(), Write(_, _)).Times(0);
  ASSERT_FALSE(socket().SendMessage(message_).ok());
}

}  // namespace channel
}  // namespace cast
